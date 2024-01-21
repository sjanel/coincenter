#include "exchangepublicapi.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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
#include "exchangeinfo.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "unreachable.hpp"

namespace cct::api {
ExchangePublic::ExchangePublic(std::string_view name, FiatConverter &fiatConverter, CommonAPI &commonApi,
                               const CoincenterInfo &coincenterInfo)
    : _name(name),
      _fiatConverter(fiatConverter),
      _commonApi(commonApi),
      _coincenterInfo(coincenterInfo),
      _exchangeInfo(coincenterInfo.exchangeInfo(name)) {}

std::optional<MonetaryAmount> ExchangePublic::convert(MonetaryAmount from, CurrencyCode toCurrency,
                                                      const MarketsPath &conversionPath, const Fiats &fiats,
                                                      MarketOrderBookMap &marketOrderBookMap,
                                                      const PriceOptions &priceOptions) {
  if (from.currencyCode() == toCurrency) {
    return from;
  }
  if (conversionPath.empty()) {
    return std::nullopt;
  }
  const ExchangeInfo::FeeType feeType =
      priceOptions.isTakerStrategy() ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;

  if (marketOrderBookMap.empty()) {
    std::lock_guard<std::mutex> guard(_allOrderBooksMutex);
    marketOrderBookMap = queryAllApproximatedOrderBooks(1);
  }

  for (Market mk : conversionPath) {
    switch (mk.type()) {
      case Market::Type::kFiatConversionMarket: {
        // should be last market
        const bool isToCurrencyFiatLike = fiats.contains(toCurrency);
        if (!isToCurrencyFiatLike) {
          // convert of fiat like crypto-currency (stable coin) to fiat currency is only possible if the destination
          // currency is a fiat. It cannot be done for an intermediate conversion
          return std::nullopt;
        }
        const CurrencyCode mFromCurrencyCode = from.currencyCode();
        const CurrencyCode mToCurrencyCode = mk.opposite(mFromCurrencyCode);
        const CurrencyCode fiatLikeFrom = _coincenterInfo.tryConvertStableCoinToFiat(mFromCurrencyCode);
        const CurrencyCode fiatFromLikeCurCode = fiatLikeFrom.isNeutral() ? mFromCurrencyCode : fiatLikeFrom;
        const CurrencyCode fiatLikeTo = _coincenterInfo.tryConvertStableCoinToFiat(mToCurrencyCode);
        const CurrencyCode fiatToLikeCurCode = fiatLikeTo.isNeutral() ? mToCurrencyCode : fiatLikeTo;

        const bool isFromFiatLike = fiatLikeFrom.isDefined() || fiats.contains(mFromCurrencyCode);
        const bool isToFiatLike = fiatLikeTo.isDefined() || fiats.contains(mToCurrencyCode);

        if (isFromFiatLike && isToFiatLike) {
          return _fiatConverter.convert(MonetaryAmount(from, fiatFromLikeCurCode), fiatToLikeCurCode);
        }
        return std::nullopt;
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
        from = _exchangeInfo.applyFee(*optA, feeType);
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
  constexpr auto operator<=>(const CurrencyDir &) const noexcept = default;

  CurrencyCode cur;
  bool isLastRealMarketReversed = false;
  bool isRegularExchangeMarket = false;
};

using CurrencyDirPath = SmallVector<CurrencyDir, 3>;

class CurrencyDirFastestPathComparator {
 public:
  explicit CurrencyDirFastestPathComparator(CommonAPI &commonApi) : _commonApi(commonApi) {}

  bool operator()(const CurrencyDirPath &lhs, const CurrencyDirPath &rhs) {
    // First, favor paths with the least number of non regular markets
    const auto hasNonRegularMarket = [](CurrencyDir curDir) { return !curDir.isRegularExchangeMarket; };
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
                                            const Fiats &fiats, MarketPathMode marketsPathMode) {
  MarketsPath ret;
  if (fromCurrency == toCurrency) {
    return ret;
  }

  const auto isFiatLike = [this, marketsPathMode, &fiats](CurrencyCode cur) {
    return (marketsPathMode == MarketPathMode::kWithLastFiatConversion &&
            _coincenterInfo.tryConvertStableCoinToFiat(cur).isDefined()) ||
           fiats.contains(cur);
  };

  const auto isToCurrencyFiatLike = isFiatLike(toCurrency);

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
        const auto marketType =
            curDir.isRegularExchangeMarket ? Market::Type::kRegularExchangeMarket : Market::Type::kFiatConversionMarket;
        if (curDir.isLastRealMarketReversed) {
          ret.emplace_back(curDir.cur, path[curDirPos - 1].cur, marketType);
        } else {
          ret.emplace_back(path[curDirPos - 1].cur, curDir.cur, marketType);
        }
      }
      return ret;
    }
    // Retrieve markets if not already done
    if (markets.empty()) {
      std::lock_guard<std::mutex> guard(_tradableMarketsMutex);
      markets = queryTradableMarkets();
      if (markets.empty()) {
        log::error("No markets retrieved for {}", _name);
        return ret;
      }
    }
    bool alreadyInsertedTargetCurrency = false;
    for (Market mk : markets | std::views::filter([cur](Market mk) { return mk.canTrade(cur); })) {
      const bool isLastRealMarketReversed = cur == mk.quote();
      constexpr bool isRegularExchangeMarket = true;
      const CurrencyCode newCur = mk.opposite(cur);
      alreadyInsertedTargetCurrency |= newCur == toCurrency;

      CurrencyDirPath &newPath = searchPaths.emplace_back(path);
      newPath.emplace_back(newCur, isLastRealMarketReversed, isRegularExchangeMarket);
      std::ranges::push_heap(searchPaths, comp);
    }
    if (isToCurrencyFiatLike && !alreadyInsertedTargetCurrency && isFiatLike(cur)) {
      constexpr bool isLastRealMarketReversed = false;
      constexpr bool isRegularExchangeMarket = false;
      const CurrencyCode newCur = toCurrency;

      CurrencyDirPath &newPath = searchPaths.emplace_back(std::move(path));
      newPath.emplace_back(newCur, isLastRealMarketReversed, isRegularExchangeMarket);
      std::ranges::push_heap(searchPaths, comp);
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
  return queryOrderBook(mk, depth).computeLimitPrice(fromCurrencyCode, priceOptions);
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
  return queryOrderBook(mk, depth).computeAvgPrice(from, priceOptions);
}

Market ExchangePublic::retrieveMarket(CurrencyCode c1, CurrencyCode c2, const MarketSet &markets) {
  Market mk(c1, c2);
  if (!markets.contains(mk)) {
    mk = mk.reverse();
    if (!markets.contains(mk)) {
      throw exception("Cannot find {}-{} nor {}-{} markets on {}", c1, c2, c2, c1, _name);
    }
  }
  return mk;
}

MarketPriceMap ExchangePublic::MarketPriceMapFromMarketOrderBookMap(const MarketOrderBookMap &marketOrderBookMap) {
  MarketPriceMap marketPriceMap;
  marketPriceMap.reserve(marketOrderBookMap.size());
  for (const auto &it : marketOrderBookMap) {
    std::optional<MonetaryAmount> optAmount = it.second.averagePrice();
    if (optAmount) {
      marketPriceMap.insert_or_assign(it.first, *optAmount);
    }
  }
  return marketPriceMap;
}

std::optional<Market> ExchangePublic::determineMarketFromMarketStr(std::string_view marketStr, MarketSet &markets,
                                                                   CurrencyCode filterCur) {
  std::optional<Market> ret;
  static constexpr std::string_view::size_type kMinimalCryptoAcronymLen = 3;

  if (!filterCur.isNeutral()) {
    std::size_t firstCurLen;
    auto curStr = filterCur.str();
    std::size_t curPos = marketStr.find(curStr);
    if (curPos == 0) {
      firstCurLen = curStr.size();
    } else {
      firstCurLen = marketStr.size() - curStr.size();
    }
    ret = Market(
        _coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.data(), firstCurLen)),
        _coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.begin() + firstCurLen, marketStr.end())));
  } else if (markets.empty() && marketStr.size() == 2 * kMinimalCryptoAcronymLen) {
    // optim (to avoid possible queryTradableMarkets): there is no crypto currency acronym shorter than 3 chars - we
    // can split the "symbol" string currencies with 3 chars each
    ret = Market(_coincenterInfo.standardizeCurrencyCode(std::string_view(marketStr.data(), kMinimalCryptoAcronymLen)),
                 _coincenterInfo.standardizeCurrencyCode(
                     std::string_view(marketStr.data() + kMinimalCryptoAcronymLen, kMinimalCryptoAcronymLen)));
  } else {  // General case, we need to query the markets
    if (markets.empty()) {
      // Without any currency, and because "marketStr" is returned without hyphen, there is no easy way to guess the
      // currencies so we need to compare with the markets that exist
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
  }
  return ret;
}

Market ExchangePublic::determineMarketFromFilterCurrencies(MarketSet &markets, CurrencyCode filterCur1,
                                                           CurrencyCode filterCur2) {
  if (markets.empty()) {
    markets = queryTradableMarkets();
  }

  Market ret;

  const auto tryAppendBaseCurrency = [&](CurrencyCode cur) {
    if (!cur.isNeutral()) {
      auto firstMarketIt = std::ranges::partition_point(markets, [cur](Market mk) { return mk.base() < cur; });
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

}  // namespace cct::api
