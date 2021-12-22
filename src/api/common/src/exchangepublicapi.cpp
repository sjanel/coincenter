#include "exchangepublicapi.hpp"

#include "cct_allocator.hpp"
#include "cct_exception.hpp"
#include "cct_flatset.hpp"
#include "cct_smallset.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "unreachable.hpp"

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
    CurrencyCode mFromCurrencyCode = a.currencyCode();
    CurrencyCode mToCurrencyCode = m.base() == a.currencyCode() ? m.quote() : m.base();
    std::optional<CurrencyCode> optFiatLikeFrom = _coincenterInfo.fiatCurrencyIfStableCoin(mFromCurrencyCode);
    CurrencyCode fiatFromLikeCurCode = (optFiatLikeFrom ? *optFiatLikeFrom : mFromCurrencyCode);
    std::optional<CurrencyCode> optFiatLikeTo = _coincenterInfo.fiatCurrencyIfStableCoin(mToCurrencyCode);
    CurrencyCode fiatToLikeCurCode = (optFiatLikeTo ? *optFiatLikeTo : mToCurrencyCode);
    const bool isFromFiatLike = optFiatLikeFrom || _cryptowatchApi.queryIsCurrencyCodeFiat(mFromCurrencyCode);
    const bool isToFiatLike = optFiatLikeTo || _cryptowatchApi.queryIsCurrencyCodeFiat(mToCurrencyCode);
    if (isFromFiatLike && isToFiatLike) {
      a = _fiatConverter.convert(MonetaryAmount(a, fiatFromLikeCurCode), fiatToLikeCurCode);
    } else {
      if (canUseCryptoWatch) {
        std::optional<double> rate = _cryptowatchApi.queryPrice(_name, Market(mFromCurrencyCode, mToCurrencyCode));
        if (rate) {
          a = a.toNeutral() * MonetaryAmount(*rate, mToCurrencyCode);
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

MonetaryAmount ExchangePublic::computeLimitOrderPrice(Market m, MonetaryAmount from, TradePriceStrategy priceStrategy) {
  MarketOrderBook marketOrderBook = queryOrderBook(m, 1);
  CurrencyCode marketCode = m.base();
  switch (priceStrategy) {
    case TradePriceStrategy::kTaker:
      [[fallthrough]];
    case TradePriceStrategy::kNibble:
      marketCode = m.quote();
      [[fallthrough]];
    case TradePriceStrategy::kMaker:
      return from.currencyCode() == marketCode ? marketOrderBook.lowestAskPrice() : marketOrderBook.highestBidPrice();
    default:
      unreachable();
  }
}

MonetaryAmount ExchangePublic::computeAvgOrderPrice(Market m, MonetaryAmount from, TradePriceStrategy priceStrategy,
                                                    int depth) {
  MarketOrderBook marketOrderBook = queryOrderBook(m, priceStrategy == TradePriceStrategy::kTaker ? depth : 1);
  CurrencyCode marketCode = m.base();
  switch (priceStrategy) {
    case TradePriceStrategy::kTaker: {
      std::optional<MonetaryAmount> optRet = marketOrderBook.computeAvgPriceForTakerAmount(from);
      if (optRet) {
        return *optRet;
      }
      log::error("{} is too big to be matched immediately on {}, return limit price instead", from.str(), m.str());
      [[fallthrough]];
    }
    case TradePriceStrategy::kNibble:
      marketCode = m.quote();
      [[fallthrough]];
    case TradePriceStrategy::kMaker:
      return from.currencyCode() == marketCode ? marketOrderBook.lowestAskPrice() : marketOrderBook.highestBidPrice();
    default:
      unreachable();
  }
}

Market ExchangePublic::retrieveMarket(CurrencyCode c1, CurrencyCode c2) {
  MarketSet markets = queryTradableMarkets();
  Market m(c1, c2);
  if (!markets.contains(m)) {
    m = m.reverse();
    if (!markets.contains(m)) {
      string ex("Cannot find ");
      ex.append(c1.str())
          .append("-")
          .append(c2.str())
          .append(" nor ")
          .append(c2.str())
          .append("-")
          .append(c1.str())
          .append(" markets on ")
          .append(_name);
      throw exception(std::move(ex));
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

std::optional<Market> ExchangePublic::determineMarketFromMarketStr(std::string_view marketStr, MarketSet &markets,
                                                                   CurrencyCode filterCur) {
  std::optional<Market> ret;
  static constexpr int kMinimalCryptoAcronymLen = 3;

  if (filterCur != CurrencyCode()) {
    std::size_t firstCurLen;
    std::string_view curStr = filterCur.str();
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
    const int symbolStrSize = static_cast<int>(marketStr.size());
    for (int splitCurPos = kMinimalCryptoAcronymLen; splitCurPos < symbolStrSize; ++splitCurPos) {
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

  auto tryAppendBaseCurrency = [&](CurrencyCode cur) {
    if (cur != CurrencyCode()) {
      auto firstMarketIt =
          std::partition_point(markets.begin(), markets.end(), [cur](Market m) { return m.base() < cur; });
      if (firstMarketIt != markets.end() && firstMarketIt->base() == cur) {
        ret = Market(cur, CurrencyCode());
        return true;
      }
    }
    return false;
  };

  auto tryAppendQuoteCurrency = [&](CurrencyCode cur1, CurrencyCode cur2) {
    ret = Market(cur1, cur2);
    if (!markets.contains(ret)) {
      log::warn("No market {} on {}, return empty list of opened orders", name(), ret.assetsPairStr('-'));
      ret = Market(cur1, CurrencyCode());
    }
  };

  if (tryAppendBaseCurrency(filterCur1)) {
    if (filterCur2 != CurrencyCode()) {
      tryAppendQuoteCurrency(filterCur1, filterCur2);
    }
  } else {
    if (tryAppendBaseCurrency(filterCur2)) {
      tryAppendQuoteCurrency(filterCur2, filterCur1);
    } else {
      log::warn("Cannot find {} among {} markets", filterCur1.str(), name());
    }
  }
  return ret;
}

}  // namespace api
}  // namespace cct