#pragma once

#include <limits>
#include <string>
#include <string_view>
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

class HuobiPublic : public ExchangePublic {
 public:
  static constexpr char kUrlBase[] = "https://api.huobi.pro";

  static constexpr char kUrlAlt[] = "https://api-aws.huobi.pro";  // More optimized if coincenter is used within 'AWS'

  static constexpr char kUserAgent[] = "Huobi C++ API Client";

  static constexpr int kDefaultDepth = 150;

  HuobiPublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override;

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = 10) override { return _allOrderBooksCache.get(depth); }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderbookCache.get(m, depth); }

  VolAndPriNbDecimals queryVolAndPriNbDecimals(Market m);

  MonetaryAmount sanitizePrice(Market m, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market m, CurrencyCode fromCurrencyCode, MonetaryAmount vol,
                                MonetaryAmount sanitizedPrice, bool isTakerOrder);

 private:
  friend class HuobiPrivate;

  struct TradableCurrenciesFunc {
    explicit TradableCurrenciesFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}

    json operator()();

    CurlHandle& _curlHandle;
  };

  struct MarketsFunc {
    MarketsFunc(CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _config(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    struct MarketInfo {
      MarketInfo() noexcept
          : maxOrderValueUSDT(std::numeric_limits<MonetaryAmount::AmountType>::max(), CurrencyCode("USDT"), 0) {}

      VolAndPriNbDecimals volAndPriNbDecimals;

      MonetaryAmount minOrderValue;
      MonetaryAmount maxOrderValueUSDT;

      MonetaryAmount limitMinOrderAmount;
      MonetaryAmount limitMaxOrderAmount;

      MonetaryAmount sellMarketMinOrderAmount;
      MonetaryAmount sellMarketMaxOrderAmount;

      MonetaryAmount buyMarketMaxOrderValue;
    };

    using MarketInfoMap = std::unordered_map<Market, MarketInfo>;

    std::pair<MarketSet, MarketInfoMap> operator()();

    CoincenterInfo& _config;
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
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
};

}  // namespace api
}  // namespace cct
