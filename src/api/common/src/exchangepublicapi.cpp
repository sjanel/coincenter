#include "exchangepublicapi.hpp"

#include "cct_exception.hpp"
#include "cct_flatset.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"

namespace cct {
namespace api {
std::optional<MonetaryAmount> ExchangePublic::convertAtAveragePrice(MonetaryAmount a, CurrencyCode toCurrencyCode) {
  Currencies currencies = findFastestConversionPath(Market(a.currencyCode(), toCurrencyCode), true);
  if (currencies.empty()) {
    return std::nullopt;
  }
  const int nbMarkets = currencies.size() - 1;
  const bool canUseCryptoWatch = _cryptowatchApi.queryIsExchangeSupported(_name);
  std::optional<MarketOrderBookMap> optMarketOrderBook;
  for (int curPos = 0; curPos < nbMarkets; ++curPos) {
    Market m(currencies[curPos], currencies[curPos + 1]);
    assert(m.base() == a.currencyCode());
    std::optional<CurrencyCode> optFiatLikeFrom = _coincenterInfo.fiatCurrencyIfStableCoin(m.base());
    CurrencyCode fiatFromLikeCurCode = (optFiatLikeFrom ? *optFiatLikeFrom : m.base());
    std::optional<CurrencyCode> optFiatLikeTo = _coincenterInfo.fiatCurrencyIfStableCoin(m.quote());
    CurrencyCode fiatToLikeCurCode = (optFiatLikeTo ? *optFiatLikeTo : m.quote());
    const bool isFromFiatLike = optFiatLikeFrom || _cryptowatchApi.queryIsCurrencyCodeFiat(m.base());
    const bool isToFiatLike = optFiatLikeTo || _cryptowatchApi.queryIsCurrencyCodeFiat(m.quote());
    if (isFromFiatLike && isToFiatLike) {
      a = _fiatConverter.convert(MonetaryAmount(a, fiatFromLikeCurCode), fiatToLikeCurCode);
    } else {
      if (canUseCryptoWatch) {
        std::optional<double> rate = _cryptowatchApi.queryPrice(_name, m);
        if (rate) {
          a = a.toNeutral() * MonetaryAmount(*rate, m.quote());
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

ExchangePublic::Currencies ExchangePublic::findFastestConversionPath(Market conversionMarket,
                                                                     bool considerStableCoinsAsFiats) {
  CurrencyCode fromCurrencyCode(conversionMarket.base());
  CurrencyCode toCurrencyCode(conversionMarket.quote());
  std::optional<CurrencyCode> optFiatFromStableCoin =
      considerStableCoinsAsFiats ? _coincenterInfo.fiatCurrencyIfStableCoin(toCurrencyCode) : std::nullopt;
  const bool isToFiatLike = optFiatFromStableCoin || _cryptowatchApi.queryIsCurrencyCodeFiat(toCurrencyCode);
  MarketSet markets = queryTradableMarkets();

  cct::vector<Currencies> searchPaths(1, {fromCurrencyCode});
  auto comp = [](const Currencies &lhs, const Currencies &rhs) { return lhs.size() > rhs.size(); };
  cct::FlatSet<CurrencyCode> visitedCurrencies;
  do {
    std::pop_heap(searchPaths.begin(), searchPaths.end(), comp);
    Currencies path = std::move(searchPaths.back());
    searchPaths.pop_back();
    CurrencyCode lastCurrencyCode = path.back();
    if (visitedCurrencies.contains(lastCurrencyCode)) {
      continue;
    }
    if (lastCurrencyCode == toCurrencyCode) {
      return path;
    }
    for (Market m : markets) {
      if (m.canTrade(lastCurrencyCode)) {
        searchPaths.emplace_back(path).push_back(CurrencyCode(lastCurrencyCode == m.base() ? m.quote() : m.base()));
        std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
      }
    }
    std::optional<CurrencyCode> optLastFiat =
        considerStableCoinsAsFiats ? _coincenterInfo.fiatCurrencyIfStableCoin(lastCurrencyCode) : std::nullopt;
    const bool isLastFiatLike = optLastFiat || _cryptowatchApi.queryIsCurrencyCodeFiat(lastCurrencyCode);
    if (isToFiatLike && isLastFiatLike) {
      searchPaths.emplace_back(std::move(path)).push_back(toCurrencyCode);
      std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
    }
    visitedCurrencies.insert(std::move(lastCurrencyCode));
  } while (!searchPaths.empty());

  return Currencies();
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
      throw exception("Cannot trade " + std::string(c1.str()) + " into " + std::string(c2.str()) + " on " + _name);
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