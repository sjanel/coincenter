#pragma once

#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "exchangepublicapi.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CryptowatchAPI;
class TradeOptions;

class BinancePublic : public ExchangePublic {
 public:
  static constexpr char kUrlBase[] = "https://api.binance.com";

  static constexpr char kUrlBaseAlt1[] = "https://api1.binance.com";
  static constexpr char kUrlBaseAlt2[] = "https://api2.binance.com";
  static constexpr char kUrlBaseAlt3[] = "https://api3.binance.com";

  static constexpr char kUserAgent[] = "Binance C++ API Client";

  BinancePublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override;

  MonetaryAmount queryWithdrawalFees(CurrencyCode currencyCode) override;

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = 10) override { return _allOrderBooksCache.get(depth); }

  MarketOrderBook queryOrderBook(Market m, int depth = 10) override { return _orderbookCache.get(m, depth); }

  VolAndPriNbDecimals queryVolAndPriNbDecimals(Market m);

  using SanitizedVolAndPri = std::pair<MonetaryAmount, MonetaryAmount>;

  MonetaryAmount sanitizePrice(Market m, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market m, MonetaryAmount vol, MonetaryAmount sanitizedPrice, bool isTakerOrder);

 private:
  friend class BinancePrivate;

  const json& retrieveMarketData(Market m) {
    const BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket& exchangeInfoData = _exchangeInfoCache.get();
    auto it = exchangeInfoData.find(m);
    if (it == exchangeInfoData.end()) {
      throw exception("Unable to retrieve market data " + m.str());
    }
    return it->second;
  }

  struct ExchangeInfoFunc {
    using ExchangeInfoDataByMarket = std::unordered_map<Market, json>;

    ExchangeInfoFunc(CoincenterInfo& config, CurlHandle& curlHandle) : _config(config), _curlHandle(curlHandle) {}

    ExchangeInfoDataByMarket operator()();

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
  };

  struct GlobalInfosFunc {
    json operator()();

    CurlHandle _curlHandle;
  };

  struct MarketsFunc {
    MarketsFunc(CachedResult<ExchangeInfoFunc>& exchangeInfoCache, CurlHandle& curlHandle,
                const ExchangeInfo& exchangeInfo)
        : _exchangeInfoCache(exchangeInfoCache), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketSet operator()();

    CachedResult<ExchangeInfoFunc>& _exchangeInfoCache;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(CachedResult<MarketsFunc>& marketsCache, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _marketsCache(marketsCache), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBookMap operator()(int depth);

    CachedResult<MarketsFunc>& _marketsCache;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct OrderBookFunc {
    OrderBookFunc(CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _config(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBook operator()(Market m, int depth = 10);

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  const ExchangeInfo& _exchangeInfo;
  CurlHandle _curlHandle;
  CachedResult<ExchangeInfoFunc> _exchangeInfoCache;
  CachedResult<GlobalInfosFunc> _globalInfosCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
};

}  // namespace api
}  // namespace cct