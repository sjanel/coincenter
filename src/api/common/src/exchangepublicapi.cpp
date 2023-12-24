#include "exchangepublicapi.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include "cct_allocator.hpp"
#include "cct_exception.hpp"
#include "cct_flatset.hpp"
#include "cct_smallset.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "exchangeconfig.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "time-window.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"

#ifdef CCT_ENABLE_PROTO
#include "proto-market-data-deserializer.hpp"
#include "proto-market-data-serializer.hpp"
#else
#include "dummy-market-data-deserializer.hpp"
#include "dummy-market-data-serializer.hpp"
#endif

namespace cct::api {

#ifdef CCT_ENABLE_PROTO
using MarketDataDeserializer = ProtoMarketDataDeserializer;
using MarketDataSerializer = ProtoMarketDataSerializer;
#else
using MarketDataDeserializer = DummyMarketDataDeserializer;
using MarketDataSerializer = DummyMarketDataSerializer;
#endif

ExchangePublic::ExchangePublic(std::string_view name, FiatConverter &fiatConverter, CommonAPI &commonApi,
                               const CoincenterInfo &coincenterInfo)
    : _name(name),
      _fiatConverter(fiatConverter),
      _commonApi(commonApi),
      _coincenterInfo(coincenterInfo),
      _exchangeConfig(coincenterInfo.exchangeConfig(name)),
      _marketDataDeserializerPtr(new MarketDataDeserializer(coincenterInfo.dataDir(), name)) {}

ExchangePublic::~ExchangePublic() = default;

std::optional<MonetaryAmount> ExchangePublic::convert(MonetaryAmount from, CurrencyCode toCurrency,
                                                      const MarketsPath &conversionPath, const CurrencyCodeSet &fiats,
                                                      MarketOrderBookMap &marketOrderBookMap,
                                                      const PriceOptions &priceOptions) {
  if (from.currencyCode() == toCurrency) {
    return from;
  }
  if (conversionPath.empty()) {
    return std::nullopt;
  }
  const ExchangeConfig::FeeType feeType =
      priceOptions.isTakerStrategy() ? ExchangeConfig::FeeType::kTaker : ExchangeConfig::FeeType::kMaker;

  if (marketOrderBookMap.empty()) {
    std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
    marketOrderBookMap = queryAllApproximatedOrderBooks(1);
  }

  for (Market mk : conversionPath) {
    switch (mk.type()) {
      case Market::Type::kFiatConversionMarket: {
        const CurrencyCode mFromCurrencyCode = from.currencyCode();
        const CurrencyCode mToCurrencyCode = mk.opposite(mFromCurrencyCode);

        const CurrencyCode fiatLikeFrom = _coincenterInfo.tryConvertStableCoinToFiat(mFromCurrencyCode);
        const CurrencyCode fiatFromLikeCurCode = fiatLikeFrom.isNeutral() ? mFromCurrencyCode : fiatLikeFrom;

        const CurrencyCode fiatLikeTo = _coincenterInfo.tryConvertStableCoinToFiat(mToCurrencyCode);
        const CurrencyCode fiatToLikeCurCode = fiatLikeTo.isNeutral() ? mToCurrencyCode : fiatLikeTo;

        const bool isFromFiatLike = fiatLikeFrom.isDefined() || fiats.contains(mFromCurrencyCode);
        const bool isToFiatLike = fiatLikeTo.isDefined() || fiats.contains(mToCurrencyCode);

        if (!isFromFiatLike || !isToFiatLike) {
          return std::nullopt;
        }

        const auto optConvertedAmount =
            _fiatConverter.convert(MonetaryAmount(from, fiatFromLikeCurCode), fiatToLikeCurCode);
        if (!optConvertedAmount) {
          return std::nullopt;
        }
        from = *optConvertedAmount;
        break;
      }
      case Market::Type::kRegularExchangeMarket: {
        const auto it = marketOrderBookMap.find(mk);
        if (it == marketOrderBookMap.end()) {
          throw exception("Should not happen - regular market should be present in the markets list");
        }
        const MarketOrderBook &marketOrderBook = it->second;
        const std::optional<MonetaryAmount> optA = marketOrderBook.convert(from, priceOptions);
        if (!optA) {
          return std::nullopt;
        }
        from = _exchangeConfig.applyFee(*optA, feeType);
        break;
      }
      default:
        unreachable();
    }
  }
  return from;
}

namespace {

// Struct containing a currency and additional information to create markets with detailed information (order, market
// type)
struct CurrencyDir {
  enum class Dir : int8_t { kExchangeOrder, kReversed };

  constexpr std::strong_ordering operator<=>(const CurrencyDir &) const noexcept = default;

  CurrencyCode cur;
  Dir dir = Dir::kExchangeOrder;
  Market::Type marketType = Market::Type::kRegularExchangeMarket;
};

using CurrencyDirPath = SmallVector<CurrencyDir, 3>;

class CurrencyDirFastestPathComparator {
 public:
  explicit CurrencyDirFastestPathComparator(CommonAPI &commonApi) : _commonApi(commonApi) {}

  bool operator()(const CurrencyDirPath &lhs, const CurrencyDirPath &rhs) {
    // First, favor paths with the least number of non regular markets
    const auto hasNonRegularMarket = [](CurrencyDir curDir) {
      return curDir.marketType != Market::Type::kRegularExchangeMarket;
    };
    const auto lhsNbNonRegularMarkets = std::ranges::count_if(lhs, hasNonRegularMarket);
    const auto rhsNbNonRegularMarkets = std::ranges::count_if(rhs, hasNonRegularMarket);
    if (lhsNbNonRegularMarkets != rhsNbNonRegularMarkets) {
      return lhsNbNonRegularMarkets > rhsNbNonRegularMarkets;
    }

    // First, favor the shortest path
    if (lhs.size() != rhs.size()) {
      return lhs.size() > rhs.size();
    }
    // For equal path sizes, favor non-fiat currencies. Two reasons for this:
    // - In some countries, tax are automatically collected when any conversion to a fiat on an exchange is made
    // - It may have the highest volume, as fiats are only present on some regions
    const auto isFiat = [this](CurrencyDir curDir) { return _commonApi.queryIsCurrencyCodeFiat(curDir.cur); };
    const auto lhsNbFiats = std::ranges::count_if(lhs, isFiat);
    const auto rhsNbFiats = std::ranges::count_if(rhs, isFiat);
    if (lhsNbFiats != rhsNbFiats) {
      return lhsNbFiats > rhsNbFiats;
    }
    // Equal path length, equal number of fiats. Compare lexicographically the two to ensure deterministic behavior
    return !std::ranges::lexicographical_compare(lhs, rhs);
  }

 private:
  CommonAPI &_commonApi;
};
}  // namespace

MarketsPath ExchangePublic::findMarketsPath(CurrencyCode fromCurrency, CurrencyCode toCurrency, MarketSet &markets,
                                            const CurrencyCodeSet &fiats, MarketPathMode marketsPathMode) {
  MarketsPath ret;
  if (fromCurrency == toCurrency) {
    return ret;
  }

  const auto isFiatConvertible = [this, marketsPathMode, &fiats](CurrencyCode cur) {
    return marketsPathMode == MarketPathMode::kWithPossibleFiatConversionAtExtremity &&
           (_coincenterInfo.tryConvertStableCoinToFiat(cur).isDefined() || fiats.contains(cur));
  };

  const auto isToCurrencyFiatConvertible = isFiatConvertible(toCurrency);

  CurrencyDirFastestPathComparator comp(_commonApi);

  vector<CurrencyDirPath> searchPaths(1, CurrencyDirPath(1, CurrencyDir(fromCurrency)));
  using VisitedCurrencyCodesSet =
      SmallSet<CurrencyCode, 10U, std::less<>, allocator<CurrencyCode>, FlatSet<CurrencyCode, std::less<>>>;
  VisitedCurrencyCodesSet visitedCurrencies;
  while (!searchPaths.empty()) {
    std::ranges::pop_heap(searchPaths, comp);
    CurrencyDirPath path = std::move(searchPaths.back());
    searchPaths.pop_back();

    CurrencyCode cur = path.back().cur;
    if (visitedCurrencies.contains(cur)) {
      continue;
    }

    if (cur == toCurrency) {
      // stop criteria
      const int nbCurDir = path.size();
      ret.reserve(nbCurDir - 1);
      for (int curDirPos = 1; curDirPos < nbCurDir; ++curDirPos) {
        const auto curDir = path[curDirPos];
        switch (curDir.dir) {
          case CurrencyDir::Dir::kExchangeOrder:
            ret.emplace_back(path[curDirPos - 1].cur, curDir.cur, curDir.marketType);
            break;
          case CurrencyDir::Dir::kReversed:
            ret.emplace_back(curDir.cur, path[curDirPos - 1].cur, curDir.marketType);
            break;
          default:
            unreachable();
        }
      }
      return ret;
    }

    // Retrieve markets if not already done
    if (markets.empty()) {
      std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
      markets = queryTradableMarkets();
      if (markets.empty()) {
        log::error("No markets retrieved for {}", _name);
        return ret;
      }
    }

    bool reachedTargetCurrency = false;
    for (Market mk : markets | std::views::filter([cur](Market mk) { return mk.canTrade(cur); })) {
      const auto dir = cur == mk.quote() ? CurrencyDir::Dir::kReversed : CurrencyDir::Dir::kExchangeOrder;
      const CurrencyCode newCur = mk.opposite(cur);

      reachedTargetCurrency = reachedTargetCurrency || (newCur == toCurrency);

      CurrencyDirPath &newPath = searchPaths.emplace_back(path);
      newPath.emplace_back(newCur, dir, Market::Type::kRegularExchangeMarket);
      std::ranges::push_heap(searchPaths, comp);
    }

    if (isFiatConvertible(cur)) {
      if (isToCurrencyFiatConvertible && !reachedTargetCurrency) {
        CurrencyDirPath &newPath = searchPaths.emplace_back(path);
        newPath.emplace_back(toCurrency, CurrencyDir::Dir::kExchangeOrder, Market::Type::kFiatConversionMarket);
        std::ranges::push_heap(searchPaths, comp);
      } else if (path.size() == 1 && searchPaths.empty()) {
        // A conversion is possible from starting fiat currency
        for (Market mk : markets) {
          if (fiats.contains(mk.base())) {
            CurrencyDirPath &newPath = searchPaths.emplace_back(path);
            newPath.emplace_back(mk.base(), CurrencyDir::Dir::kExchangeOrder, Market::Type::kFiatConversionMarket);
            std::ranges::push_heap(searchPaths, comp);
          } else if (fiats.contains(mk.quote())) {
            CurrencyDirPath &newPath = searchPaths.emplace_back(path);
            newPath.emplace_back(mk.quote(), CurrencyDir::Dir::kExchangeOrder, Market::Type::kFiatConversionMarket);
            std::ranges::push_heap(searchPaths, comp);
          }
        }
      }
    }

    visitedCurrencies.insert(std::move(cur));
  }

  return ret;
}

ExchangePublic::CurrenciesPath ExchangePublic::findCurrenciesPath(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                                                  MarketPathMode marketsPathMode) {
  MarketsPath marketsPath = findMarketsPath(fromCurrency, toCurrency, marketsPathMode);
  CurrenciesPath ret;
  if (!marketsPath.empty()) {
    ret.reserve(marketsPath.size() + 1U);
    ret.emplace_back(fromCurrency);
    for (Market mk : marketsPath) {
      if (mk.base() == ret.back()) {
        ret.emplace_back(mk.quote());
      } else {
        ret.emplace_back(mk.base());
      }
    }
  }
  return ret;
}

std::optional<MonetaryAmount> ExchangePublic::computeLimitOrderPrice(Market mk, CurrencyCode fromCurrencyCode,
                                                                     const PriceOptions &priceOptions) {
  const int depth = priceOptions.isRelativePrice() ? std::abs(priceOptions.relativePrice()) : 1;
  return getOrderBook(mk, depth).computeLimitPrice(fromCurrencyCode, priceOptions);
}

std::optional<MonetaryAmount> ExchangePublic::computeAvgOrderPrice(Market mk, MonetaryAmount from,
                                                                   const PriceOptions &priceOptions) {
  if (priceOptions.isFixedPrice()) {
    return MonetaryAmount(priceOptions.fixedPrice(), mk.quote());
  }
  int depth = 1;
  if (priceOptions.isRelativePrice()) {
    depth = std::abs(priceOptions.relativePrice());
  } else if (priceOptions.priceStrategy() == PriceStrategy::kTaker) {
    depth = kDefaultDepth;
  }
  return getOrderBook(mk, depth).computeAvgPrice(from, priceOptions);
}

std::optional<Market> ExchangePublic::RetrieveMarket(CurrencyCode c1, CurrencyCode c2, const MarketSet &markets) {
  Market mk(c1, c2);
  if (!markets.contains(mk)) {
    mk = mk.reverse();
    if (!markets.contains(mk)) {
      return {};
    }
  }
  return mk;
}

std::optional<Market> ExchangePublic::retrieveMarket(CurrencyCode c1, CurrencyCode c2) {
  std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
  return RetrieveMarket(c1, c2, queryTradableMarkets());
}

MarketPriceMap ExchangePublic::MarketPriceMapFromMarketOrderBookMap(const MarketOrderBookMap &marketOrderBookMap) {
  MarketPriceMap marketPriceMap;
  marketPriceMap.reserve(marketOrderBookMap.size());
  for (const auto &[market, marketOrderBook] : marketOrderBookMap) {
    std::optional<MonetaryAmount> optAmount = marketOrderBook.averagePrice();
    if (optAmount) {
      marketPriceMap.insert_or_assign(market, *optAmount);
    }
  }
  return marketPriceMap;
}

std::optional<Market> ExchangePublic::determineMarketFromMarketStr(std::string_view marketStr, MarketSet &markets,
                                                                   CurrencyCode filterCur) {
  if (!filterCur.isNeutral()) {
    std::size_t firstCurLen;
    auto curStr = filterCur.str();
    std::size_t curPos = marketStr.find(curStr);
    if (curPos == 0) {
      firstCurLen = curStr.size();
    } else {
      firstCurLen = marketStr.size() - curStr.size();
    }
    return Market(
        _coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.data(), firstCurLen)),
        _coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.begin() + firstCurLen, marketStr.end())));
  }

  static constexpr std::string_view::size_type kMinimalCryptoAcronymLen = 3;

  if (markets.empty() && marketStr.size() == 2 * kMinimalCryptoAcronymLen) {
    // optim (to avoid possible queryTradableMarkets): assuming there is no crypto currency acronym shorter than 3 chars
    // - we can split the "symbol" string currencies with 3 chars each
    return Market(_coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.data(), kMinimalCryptoAcronymLen)),
                  _coincenterInfo.standardizeCurrencyCode(
                      std::string_view(marketStr.data() + kMinimalCryptoAcronymLen, kMinimalCryptoAcronymLen)));
  }

  std::optional<Market> ret;

  // General case, we need to query the markets
  if (markets.empty()) {
    // Without any currency, and because "marketStr" is returned without hyphen, there is no easy way to guess the
    // currencies so we need to compare with the markets that exist
    std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
    markets = queryTradableMarkets();
  }
  const auto symbolStrSize = marketStr.size();
  for (auto splitCurPos = kMinimalCryptoAcronymLen; splitCurPos < symbolStrSize; ++splitCurPos) {
    ret = Market(_coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.data(), splitCurPos)),
                 _coincenterInfo.standardizeCurrencyCode(
                     std::string_view(marketStr.data() + splitCurPos, symbolStrSize - splitCurPos)));
    if (markets.contains(*ret)) {
      break;
    }
    ret = ret->reverse();
    if (markets.contains(*ret)) {
      break;
    }
  }
  if (!ret || ret->quote().size() < kMinimalCryptoAcronymLen) {
    log::error("Cannot determine market for {}, skipping", marketStr);
    ret = std::nullopt;
  }

  return ret;
}

Market ExchangePublic::determineMarketFromFilterCurrencies(MarketSet &markets, CurrencyCode filterCur1,
                                                           CurrencyCode filterCur2) {
  if (markets.empty()) {
    std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
    markets = queryTradableMarkets();
  }

  Market ret;

  const auto tryAppendBaseCurrency = [&](CurrencyCode cur) {
    if (!cur.isNeutral()) {
      const auto firstMarketIt = std::ranges::partition_point(markets, [cur](Market mk) { return mk.base() < cur; });
      if (firstMarketIt != markets.end() && firstMarketIt->base() == cur) {
        ret = Market(cur, CurrencyCode());
        return true;
      }
    }
    return false;
  };

  const auto tryAppendQuoteCurrency = [&](CurrencyCode lhs, CurrencyCode rhs) {
    ret = Market(lhs, rhs);
    if (!markets.contains(ret)) {
      log::debug("No market {} on {}", name(), ret);
      ret = Market(lhs, CurrencyCode());
    }
  };

  if (tryAppendBaseCurrency(filterCur1)) {
    if (!filterCur2.isNeutral()) {
      tryAppendQuoteCurrency(filterCur1, filterCur2);
    }
  } else {
    if (tryAppendBaseCurrency(filterCur2)) {
      tryAppendQuoteCurrency(filterCur2, filterCur1);
    } else {
      log::debug("Cannot find {} among {} markets", filterCur1, name());
    }
  }
  return ret;
}

MonetaryAmount ExchangePublic::queryWithdrawalFeeOrZero(CurrencyCode currencyCode) {
  std::optional<MonetaryAmount> optWithdrawFee = queryWithdrawalFee(currencyCode);
  MonetaryAmount withdrawFee;
  if (optWithdrawFee) {
    withdrawFee = *optWithdrawFee;
  } else {
    log::error("Unable to retrieve withdraw fee for {} on {}, consider 0", currencyCode, name());
    withdrawFee = MonetaryAmount(0, currencyCode);
  }
  return withdrawFee;
}

MarketOrderBook ExchangePublic::getOrderBook(Market mk, int depth) {
  std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
  const auto marketOrderBook = queryOrderBook(mk, depth);

  if (_exchangeConfig.withMarketDataSerialization()) {
    getMarketDataSerializer().push(marketOrderBook);
  }
  return marketOrderBook;
}

/// Retrieve an ordered vector of recent last trades
PublicTradeVector ExchangePublic::getLastTrades(Market mk, int nbTrades) {
  std::lock_guard<std::recursive_mutex> guard(_publicRequestsMutex);
  const auto lastTrades = queryLastTrades(mk, nbTrades);

  if (_exchangeConfig.withMarketDataSerialization()) {
    getMarketDataSerializer().push(mk, lastTrades);
  }
  return lastTrades;
}

MarketTimestampSet ExchangePublic::pullMarketOrderBooksMarkets(TimeWindow timeWindow) {
  return _marketDataDeserializerPtr->pullMarketOrderBooksMarkets(timeWindow);
}

MarketTimestampSet ExchangePublic::pullTradeMarkets(TimeWindow timeWindow) {
  return _marketDataDeserializerPtr->pullTradeMarkets(timeWindow);
}

PublicTradeVector ExchangePublic::pullTradesForReplay(Market market, TimeWindow timeWindow) {
  return _marketDataDeserializerPtr->pullTrades(market, timeWindow);
}

MarketOrderBookVector ExchangePublic::pullMarketOrderBooksForReplay(Market market, TimeWindow timeWindow) {
  return _marketDataDeserializerPtr->pullMarketOrderBooks(market, timeWindow);
}

AbstractMarketDataSerializer &ExchangePublic::getMarketDataSerializer() {
  if (_marketDataSerializerPtr) {
    return *_marketDataSerializerPtr;
  }

  const auto nowTime = Clock::now();

  // Heuristic: load up to 1 week of data to retrieve the youngest written timestamp.
  // This will be used in order not to write duplicate objects at the start of a new program after that a previous
  // program run was stopped.
  const TimeWindow largeTimeWindow{nowTime - std::chrono::weeks{1}, nowTime};

  const MarketTimestampSets marketTimestampSets{pullMarketOrderBooksMarkets(largeTimeWindow),
                                                pullTradeMarkets(largeTimeWindow)};

  _marketDataSerializerPtr =
      std::make_unique<MarketDataSerializer>(_coincenterInfo.dataDir(), marketTimestampSets, name());

  return *_marketDataSerializerPtr;
}

}  // namespace cct::api
