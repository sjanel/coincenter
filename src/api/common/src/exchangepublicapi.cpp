#include "exchangepublicapi.hpp"

#include "cct_exception.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"

namespace cct {
namespace api {
std::optional<MonetaryAmount> ExchangePublic::convertAtAveragePrice(MonetaryAmount a, CurrencyCode toCurrencyCode) {
  Currencies currencies = findFastestConversionPath(a.currencyCode(), toCurrencyCode);
  if (currencies.empty()) {
    return std::nullopt;
  }
  const int nbMarkets = currencies.size() - 1;
  const bool canUseCryptoWatch = _cryptowatchApi.queryIsExchangeSupported(_name);
  std::optional<MarketOrderBookMap> optMarketOrderBook;
  for (int curPos = 0; curPos < nbMarkets; ++curPos) {
    Market m(currencies[curPos], currencies[curPos + 1]);
    assert(m.base() == a.currencyCode());
    const bool isFromFiat = _cryptowatchApi.queryIsCurrencyCodeFiat(m.base());
    const bool isToFiat = _cryptowatchApi.queryIsCurrencyCodeFiat(m.quote());
    if (isFromFiat && isToFiat) {
      a = _fiatConverter.convert(a, m.quote());
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

ExchangePublic::Currencies ExchangePublic::findFastestConversionPath(CurrencyCode fromCurrencyCode,
                                                                     CurrencyCode toCurrencyCode) {
  const bool isToFiat = _cryptowatchApi.queryIsCurrencyCodeFiat(toCurrencyCode);
  MarketSet markets = queryTradableMarkets();

  cct::vector<Currencies> searchPaths(1, {fromCurrencyCode});
  auto comp = [](const Currencies &lhs, const Currencies &rhs) { return lhs.size() > rhs.size(); };
  do {
    std::pop_heap(searchPaths.begin(), searchPaths.end(), comp);
    Currencies path = std::move(searchPaths.back());
    searchPaths.pop_back();
    CurrencyCode lastCurrencyCode = path.back();
    if (lastCurrencyCode == toCurrencyCode) {
      return path;
    }
    for (Market m : markets) {
      if (m.canTrade(lastCurrencyCode)) {
        CurrencyCode newCurrencyCode(lastCurrencyCode == m.base() ? m.quote() : m.base());
        if (std::find(path.begin(), path.end(), newCurrencyCode) == path.end()) {
          searchPaths.emplace_back(path).push_back(newCurrencyCode);
          std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
        }
      }
    }
    if (isToFiat && _cryptowatchApi.queryIsCurrencyCodeFiat(lastCurrencyCode)) {
      searchPaths.emplace_back(std::move(path)).push_back(toCurrencyCode);
      std::push_heap(searchPaths.begin(), searchPaths.end(), comp);
    }
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