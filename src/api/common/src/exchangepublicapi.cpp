#include "exchangepublicapi.hpp"

#include "cct_allocator.hpp"
#include "cct_exception.hpp"
#include "cct_flatset.hpp"
#include "cct_smallset.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"

namespace cct {
namespace api {
std::optional<MonetaryAmount> ExchangePublic::convertAtAveragePrice(MonetaryAmount a, CurrencyCode toCurrencyCode) {
  if (a.currencyCode() == toCurrencyCode) {
    return a;
  }
  ConversionPath conversionPath = findFastestConversionPath(a.currencyCode(), toCurrencyCode, true);
  if (conversionPath.empty()) {
    return std::nullopt;
  }
  const bool canUseCryptoWatch = _cryptowatchApi.queryIsExchangeSupported(_name);
  std::optional<MarketOrderBookMap> optMarketOrderBook;
  for (Market m : conversionPath) {
    assert(m.canTrade(a.currencyCode()));
    CurrencyCode fromCurrencyCode = a.currencyCode();
    CurrencyCode toCurrencyCode = m.base() == a.currencyCode() ? m.quote() : m.base();
    std::optional<CurrencyCode> optFiatLikeFrom = _coincenterInfo.fiatCurrencyIfStableCoin(fromCurrencyCode);
    CurrencyCode fiatFromLikeCurCode = (optFiatLikeFrom ? *optFiatLikeFrom : fromCurrencyCode);
    std::optional<CurrencyCode> optFiatLikeTo = _coincenterInfo.fiatCurrencyIfStableCoin(toCurrencyCode);
    CurrencyCode fiatToLikeCurCode = (optFiatLikeTo ? *optFiatLikeTo : toCurrencyCode);
    const bool isFromFiatLike = optFiatLikeFrom || _cryptowatchApi.queryIsCurrencyCodeFiat(fromCurrencyCode);
    const bool isToFiatLike = optFiatLikeTo || _cryptowatchApi.queryIsCurrencyCodeFiat(toCurrencyCode);
    if (isFromFiatLike && isToFiatLike) {
      a = _fiatConverter.convert(MonetaryAmount(a, fiatFromLikeCurCode), fiatToLikeCurCode);
    } else {
      if (canUseCryptoWatch) {
        std::optional<double> rate = _cryptowatchApi.queryPrice(_name, Market(fromCurrencyCode, toCurrencyCode));
        if (rate) {
          a = a.toNeutral() * MonetaryAmount(*rate, toCurrencyCode);
          continue;
        }
      }
      if (!optMarketOrderBook) {
        optMarketOrderBook = queryAllApproximatedOrderBooks(1);
      }
      auto it = optMarketOrderBook->find(m);
      if (it == optMarketOrderBook->end()) {
        return std::nullopt;
      }
      std::optional<MonetaryAmount> optA = it->second.convertAtAvgPrice(a);
      if (!optA) {
        return std::nullopt;
      }
      a = *optA;
    }
  }
  return a;
}

namespace {

// Optimized struct containing a currency and a reverse bool to keep market directionnality information
struct CurrencyDir {
  CurrencyDir(CurrencyCode c, bool b) : cur(c), isLastRealMarketReversed(b) {}

  CurrencyCode cur;
  bool isLastRealMarketReversed;
};

using CurrencyDirPath = SmallVector<CurrencyDir, 4>;

class CurrencyDirFastestPathComparator {
 public:
  explicit CurrencyDirFastestPathComparator(CryptowatchAPI &cryptowatchApi) : _cryptowatchApi(cryptowatchApi) {}

  bool operator()(const CurrencyDirPath &lhs, const CurrencyDirPath &rhs) {
    // First, favor the shortest path
    if (lhs.size() != rhs.size()) {
      return lhs.size() > rhs.size();
    }
    // For equal path sizes, favor non-fiat currencies. Two reasons for this:
    // - In some countries, tax are automatically collected when any conversion to a fiat on an exchange is made
    // - It may have the highest volume, as fiats are only present on some regions
    auto isFiat = [this](CurrencyDir c) { return _cryptowatchApi.queryIsCurrencyCodeFiat(c.cur); };
    return std::count_if(lhs.begin(), lhs.end(), isFiat) > std::count_if(rhs.begin(), rhs.end(), isFiat);
  }

 private:
  CryptowatchAPI &_cryptowatchApi;
};
}  // namespace

ExchangePublic::ConversionPath ExchangePublic::findFastestConversionPath(CurrencyCode fromCurrencyCode,
                                                                         CurrencyCode toCurrencyCode,
                                                                         bool considerStableCoinsAsFiats) {
  ConversionPath ret;
  if (fromCurrencyCode == toCurrencyCode) {
    log::error("Cannot convert {} to itself", fromCurrencyCode.str());
    return ret;
  }
  std::optional<CurrencyCode> optFiatFromStableCoin =
      considerStableCoinsAsFiats ? _coincenterInfo.fiatCurrencyIfStableCoin(toCurrencyCode) : std::nullopt;
  const bool isToFiatLike = optFiatFromStableCoin || _cryptowatchApi.queryIsCurrencyCodeFiat(toCurrencyCode);
  MarketSet markets = queryTradableMarkets();

  CurrencyDirFastestPathComparator comp(_cryptowatchApi);

  vector<CurrencyDirPath> searchPaths(1, CurrencyDirPath(1, CurrencyDir(fromCurrencyCode, false)));
  using VisitedCurrenciesSet =
      SmallSet<CurrencyCode, 10, std::less<CurrencyCode>, allocator<CurrencyCode>, FlatSet<CurrencyCode>>;
  VisitedCurrenciesSet visitedCurrencies;
  do {
    std::pop_heap(searchPaths.begin(), searchPaths.end(), comp);
    CurrencyDirPath path = std::move(searchPaths.back());
    searchPaths.pop_back();
    CurrencyCode lastCurrencyCode = path.back().cur;
    if (visitedCurrencies.contains(lastCurrencyCode)) {
      continue;
    }
    if (lastCurrencyCode == toCurrencyCode) {
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
    for (Market m : markets) {
      if (m.canTrade(lastCurrencyCode)) {
        CurrencyDirPath &newPath = searchPaths.emplace_back(path);
        const bool isLastRealMarketReversed = lastCurrencyCode == m.quote();
        const CurrencyCode newCur = lastCurrencyCode == m.base() ? m.quote() : m.base();
        newPath.emplace_back(newCur, isLastRealMarketReversed);
        std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
      }
    }
    std::optional<CurrencyCode> optLastFiat =
        considerStableCoinsAsFiats ? _coincenterInfo.fiatCurrencyIfStableCoin(lastCurrencyCode) : std::nullopt;
    const bool isLastFiatLike = optLastFiat || _cryptowatchApi.queryIsCurrencyCodeFiat(lastCurrencyCode);
    if (isToFiatLike && isLastFiatLike) {
      searchPaths.emplace_back(std::move(path)).emplace_back(toCurrencyCode, false);
      std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
    }
    visitedCurrencies.insert(std::move(lastCurrencyCode));
  } while (!searchPaths.empty());

  return ret;
}

MonetaryAmount ExchangePublic::computeLimitOrderPrice(Market m, MonetaryAmount from) {
  MarketOrderBook marketOrderBook = queryOrderBook(m);
  return from.currencyCode() == m.base() ? marketOrderBook.lowestAskPrice() : marketOrderBook.highestBidPrice();
}

MonetaryAmount ExchangePublic::computeAvgOrderPrice(Market m, MonetaryAmount from, bool isTakerStrategy, int depth) {
  MarketOrderBook marketOrderBook = queryOrderBook(m, depth);
  if (isTakerStrategy) {
    std::optional<MonetaryAmount> optRet = marketOrderBook.computeAvgPriceForTakerAmount(from);
    if (optRet) {
      return *optRet;
    }
    log::error("{} is too big to be matched immediately on {}, return limit price instead", from.str(), m.str());
  }
  return from.currencyCode() == m.base() ? marketOrderBook.lowestAskPrice() : marketOrderBook.highestBidPrice();
}

Market ExchangePublic::retrieveMarket(CurrencyCode c1, CurrencyCode c2) {
  MarketSet markets = queryTradableMarkets();
  Market m(c1, c2);
  if (!markets.contains(m)) {
    m = m.reverse();
    if (!markets.contains(m)) {
      throw exception("Cannot trade " + string(c1.str()) + " into " + string(c2.str()) + " on " + _name);
    }
  }
  return m;
}

ExchangePublic::MarketPriceMap ExchangePublic::marketPriceMapFromMarketOrderBookMap(
    const MarketOrderBookMap &marketOrderBookMap) const {
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

}  // namespace api
}  // namespace cct