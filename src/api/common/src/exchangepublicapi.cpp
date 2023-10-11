#include "exchangepublicapi.hpp"

#include <unordered_set>

#include "cct_allocator.hpp"
#include "cct_exception.hpp"
#include "coincenterinfo.hpp"
#include "fiatconverter.hpp"
#include "unreachable.hpp"

namespace cct::api {
ExchangePublic::ExchangePublic(std::string_view name, FiatConverter &fiatConverter, CommonAPI &commonApi,
                               const CoincenterInfo &coincenterInfo)
    : _name(name),
      _fiatConverter(fiatConverter),
      _commonApi(commonApi),
      _coincenterInfo(coincenterInfo),
      _exchangeInfo(coincenterInfo.exchangeInfo(name)) {}

std::optional<MonetaryAmount> ExchangePublic::convert(MonetaryAmount amount, CurrencyCode toCurrency,
                                                      const MarketsPath &conversionPath, const Fiats &fiats,
                                                      MarketOrderBookMap &marketOrderBookMap,
                                                      const PriceOptions &priceOptions) {
  if (amount.currencyCode() == toCurrency) {
    return amount;
  }
  if (conversionPath.empty()) {
    return std::nullopt;
  }
  const ExchangeInfo::FeeType feeType =
      priceOptions.isTakerStrategy() ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
  for (Market mk : conversionPath) {
    CurrencyCode mFromCurrencyCode = amount.currencyCode();
    assert(mk.canTrade(mFromCurrencyCode));
    CurrencyCode mToCurrencyCode = mk.base() == amount.currencyCode() ? mk.quote() : mk.base();
    std::optional<CurrencyCode> optFiatLikeFrom = _coincenterInfo.fiatCurrencyIfStableCoin(mFromCurrencyCode);
    CurrencyCode fiatFromLikeCurCode = (optFiatLikeFrom ? *optFiatLikeFrom : mFromCurrencyCode);
    std::optional<CurrencyCode> optFiatLikeTo = _coincenterInfo.fiatCurrencyIfStableCoin(mToCurrencyCode);
    CurrencyCode fiatToLikeCurCode = (optFiatLikeTo ? *optFiatLikeTo : mToCurrencyCode);
    bool isFromFiatLike = optFiatLikeFrom || fiats.contains(mFromCurrencyCode);
    bool isToFiatLike = optFiatLikeTo || fiats.contains(mToCurrencyCode);
    if (isFromFiatLike && isToFiatLike) {
      amount = _fiatConverter.convert(MonetaryAmount(amount, fiatFromLikeCurCode), fiatToLikeCurCode);
    } else {
      if (marketOrderBookMap.empty()) {
        marketOrderBookMap = queryAllApproximatedOrderBooks(1);
      }
      auto it = marketOrderBookMap.find(mk);
      if (it == marketOrderBookMap.end()) {
        return std::nullopt;
      }
      const MarketOrderBook &marketOrderbook = it->second;
      std::optional<MonetaryAmount> optA = marketOrderbook.convert(amount, priceOptions);
      if (!optA) {
        return std::nullopt;
      }
      amount = _exchangeInfo.applyFee(*optA, feeType);
    }
  }
  return amount;
}

namespace {

// Optimized struct containing a currency and a reverse bool to keep market directionality information
struct CurrencyDir {
  CurrencyCode cur;
  bool isLastRealMarketReversed;
};

using CurrencyDirPath = SmallVector<CurrencyDir, 3>;

class CurrencyDirFastestPathComparator {
 public:
  explicit CurrencyDirFastestPathComparator(CommonAPI &commonApi) : _commonApi(commonApi) {}

  bool operator()(const CurrencyDirPath &lhs, const CurrencyDirPath &rhs) {
    // First, favor the shortest path
    if (lhs.size() != rhs.size()) {
      return lhs.size() > rhs.size();
    }
    // For equal path sizes, favor non-fiat currencies. Two reasons for this:
    // - In some countries, tax are automatically collected when any conversion to a fiat on an exchange is made
    // - It may have the highest volume, as fiats are only present on some regions
    auto isFiat = [this](CurrencyDir curDir) { return _commonApi.queryIsCurrencyCodeFiat(curDir.cur); };
    return std::ranges::count_if(lhs, isFiat) > std::ranges::count_if(rhs, isFiat);
  }

 private:
  CommonAPI &_commonApi;
};
}  // namespace

MarketsPath ExchangePublic::findMarketsPath(CurrencyCode fromCurrency, CurrencyCode toCurrency, MarketSet &markets,
                                            const Fiats &fiats, bool considerStableCoinsAsFiats) {
  MarketsPath ret;
  if (fromCurrency == toCurrency) {
    return ret;
  }

  std::optional<CurrencyCode> optFiatFromStableCoin =
      considerStableCoinsAsFiats ? _coincenterInfo.fiatCurrencyIfStableCoin(toCurrency) : std::nullopt;
  const bool isToFiatLike = optFiatFromStableCoin.has_value() || fiats.contains(toCurrency);

  CurrencyDirFastestPathComparator comp(_commonApi);

  vector<CurrencyDirPath> searchPaths(1, CurrencyDirPath(1, CurrencyDir(fromCurrency, false)));
  using VisitedCurrenciesSet = std::unordered_set<CurrencyCode>;
  VisitedCurrenciesSet visitedCurrencies;
  do {
    std::pop_heap(searchPaths.begin(), searchPaths.end(), comp);
    CurrencyDirPath path = std::move(searchPaths.back());
    searchPaths.pop_back();
    CurrencyCode lastCurrencyCode = path.back().cur;
    if (visitedCurrencies.contains(lastCurrencyCode)) {
      continue;
    }
    if (lastCurrencyCode == toCurrency) {
      const int nbCurDir = path.size();
      ret.reserve(nbCurDir - 1);
      for (int curDirPos = 1; curDirPos < nbCurDir; ++curDirPos) {
        if (path[curDirPos].isLastRealMarketReversed) {
          ret.emplace_back(path[curDirPos].cur, path[curDirPos - 1].cur);
        } else {
          ret.emplace_back(path[curDirPos - 1].cur, path[curDirPos].cur);
        }
      }
      return ret;
    }
    if (markets.empty()) {
      markets = queryTradableMarkets();
      if (markets.empty()) {
        log::error("No markets retrieved for {}", _name);
        return ret;
      }
    }
    for (Market mk : markets) {
      if (mk.canTrade(lastCurrencyCode)) {
        CurrencyDirPath &newPath = searchPaths.emplace_back(path);
        const bool isLastRealMarketReversed = lastCurrencyCode == mk.quote();
        const CurrencyCode newCur = mk.opposite(lastCurrencyCode);
        newPath.emplace_back(newCur, isLastRealMarketReversed);
        std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
      }
    }
    std::optional<CurrencyCode> optLastFiat =
        considerStableCoinsAsFiats ? _coincenterInfo.fiatCurrencyIfStableCoin(lastCurrencyCode) : std::nullopt;
    const bool isLastFiatLike = optLastFiat || fiats.contains(lastCurrencyCode);
    if (isToFiatLike && isLastFiatLike) {
      searchPaths.emplace_back(std::move(path)).emplace_back(toCurrency, false);
      std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
    }
    visitedCurrencies.insert(std::move(lastCurrencyCode));
  } while (!searchPaths.empty());

  return ret;
}

ExchangePublic::CurrenciesPath ExchangePublic::findCurrenciesPath(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                                                  bool considerStableCoinsAsFiats) {
  MarketsPath marketsPath = findMarketsPath(fromCurrency, toCurrency, considerStableCoinsAsFiats);
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
    if (ret->quote().size() < kMinimalCryptoAcronymLen) {
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

}  // namespace cct::api