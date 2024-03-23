#pragma once

#include <mutex>
#include <optional>
#include <string_view>

#include "commonapi.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangebase.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "priceoptions.hpp"
#include "public-trade-vector.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeConfig;
class FiatConverter;

namespace api {

class ExchangePublic : public ExchangeBase {
 public:
  static constexpr int kDefaultDepth = MarketOrderBook::kDefaultDepth;
  static constexpr int kNbLastTradesDefault = 100;

  enum class MarketPathMode : int8_t {
    // Only authorize conversions from real markets of the exchange.
    // In particular, fiat conversions will be forbidden  if the fiat pair does not exist as a real market on the
    // exchange.
    kStrict,

    // Authorize unique fiat conversion at one extremity of the conversion path (beginning or end, but not both).
    kWithPossibleFiatConversionAtExtremity
  };

  using Fiats = CommonAPI::Fiats;

  virtual ~ExchangePublic() = default;

  /// Check if public exchange is responding to basic health check, return true in this case.
  /// Exchange that implements the HealthCheck do not need to add a retry mechanism.
  virtual bool healthCheck() = 0;

  /// Retrieve the possible currencies known by current exchange.
  /// If some information is not known without any private key, information can be returned partially.
  virtual CurrencyExchangeFlatSet queryTradableCurrencies() = 0;

  virtual CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) = 0;

  /// Retrieve all the markets proposed by the exchange.
  virtual MarketSet queryTradableMarkets() = 0;

  /// Retrieve all approximated prices per market.
  /// Data will not be necessarily up to date, but it's handy to get a lot of prices at once.
  virtual MarketPriceMap queryAllPrices() = 0;

  /// Attempts to convert amount into a target currency.
  /// Conversion is made according to given price options, which uses the 'Maker' prices by default.
  std::optional<MonetaryAmount> estimatedConvert(MonetaryAmount from, CurrencyCode toCurrency,
                                                 const PriceOptions &priceOptions = PriceOptions()) {
    MarketOrderBookMap marketOrderBookMap;
    Fiats fiats = queryFiats();
    MarketSet markets;
    MarketsPath conversionPath = findMarketsPath(from.currencyCode(), toCurrency, markets, fiats,
                                                 MarketPathMode::kWithPossibleFiatConversionAtExtremity);
    return convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions);
  }

  /// Attempts to convert amount into a target currency.
  /// Conversion is made according to given price options, which uses the 'Maker' prices by default.
  /// No external calls is made with this version, it has all what it needs
  std::optional<MonetaryAmount> convert(MonetaryAmount from, CurrencyCode toCurrency, const MarketsPath &conversionPath,
                                        const Fiats &fiats, MarketOrderBookMap &marketOrderBookMap,
                                        const PriceOptions &priceOptions = PriceOptions());

  /// Retrieve the fixed withdrawal fees per currency.
  /// Depending on the exchange, this could be retrieved dynamically,
  /// or, if not possible, should be retrieved from a static source updated regularly.
  virtual MonetaryAmountByCurrencySet queryWithdrawalFees() = 0;

  /// Retrieve the withdrawal fee of a Currency only
  virtual std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) = 0;

  /// Return true if exchange supports official REST API has an endpoint to get withdrawal fees
  /// For instance, Kraken does not offer such endpoint, we nee to query external sources which may provide inaccurate
  /// results
  virtual bool isWithdrawalFeesSourceReliable() const = 0;

  /// Get all the MarketOrderBooks of this exchange as fast as possible.
  /// Exchanges which do not support retrieval of all of them at once may used heuristic methods.
  virtual MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) = 0;

  /// Retrieve the order book of given market.
  /// It should be more precise that previous version with possibility to go deeper.
  virtual MarketOrderBook queryOrderBook(Market mk, int depth = kDefaultDepth) = 0;

  /// Retrieve the total volume exchange on given market in the last 24 hours.
  virtual MonetaryAmount queryLast24hVolume(Market mk) = 0;

  /// Retrieve an ordered vector of recent last trades
  virtual PublicTradeVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) = 0;

  /// Retrieve the last price of given market.
  virtual MonetaryAmount queryLastPrice(Market mk) = 0;

  Fiats queryFiats() { return _commonApi.queryFiats(); }

  /// Get the name of the exchange in lower case.
  std::string_view name() const { return _name; }

  /// Retrieve the shortest array of markets that can convert 'fromCurrencyCode' to 'toCurrencyCode' (shortest in terms
  /// of number of conversions) of 'fromCurrencyCode' to 'toCurrencyCode'.
  /// @return array of Market (in the order in which they are defined on the exchange),
  ///         or empty array if conversion is not possible
  /// For instance, findMarketsPath("XLM", "XRP") can return:
  ///   - XLM-USDT
  ///   - XRP-USDT
  MarketsPath findMarketsPath(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode, MarketSet &markets,
                              const Fiats &fiats, MarketPathMode marketsPathMode = MarketPathMode::kStrict);

  MarketsPath findMarketsPath(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode,
                              MarketPathMode marketsPathMode = MarketPathMode::kStrict) {
    MarketSet markets;
    return findMarketsPath(fromCurrencyCode, toCurrencyCode, markets, queryFiats(), marketsPathMode);
  }

  using CurrenciesPath = SmallVector<CurrencyCode, 4>;

  /// Retrieve the shortest path allowing to convert 'fromCurrencyCode' to 'toCurrencyCode', as an array of currencies.
  /// This is a variation of 'findMarketsPath', except that instead of returning markets as defined in the exchange, it
  /// gives only the currencies in order.
  /// For instance, findCurrenciesPath("XLM", "XRP") can return ["XLM", "USDT", "XRP"]
  CurrenciesPath findCurrenciesPath(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode,
                                    MarketPathMode marketsPathMode = MarketPathMode::kStrict);

  std::optional<MonetaryAmount> computeLimitOrderPrice(Market mk, CurrencyCode fromCurrencyCode,
                                                       const PriceOptions &priceOptions);

  std::optional<MonetaryAmount> computeAvgOrderPrice(Market mk, MonetaryAmount from, const PriceOptions &priceOptions);

  /// Retrieve the market in the correct order proposed by the exchange for given couple of currencies.
  static std::optional<Market> RetrieveMarket(CurrencyCode c1, CurrencyCode c2, const MarketSet &markets);

  std::optional<Market> retrieveMarket(CurrencyCode c1, CurrencyCode c2);

  /// Helper method to determine ordered Market from this exchange from a market string representation without currency
  /// separator (for instance, "BTCEUR" should be guessed as a market with BTC as base currency, and EUR as price
  /// currency)
  /// @param marketStr the market string representation without separator
  /// @param markets passed as non const reference for cache purposes, if the method is called in a loop.
  ///                Give an empty market set at first call, markets will be retrieved only if necessary to avoid
  ///                useless API calls.
  std::optional<Market> determineMarketFromMarketStr(std::string_view marketStr, MarketSet &markets,
                                                     CurrencyCode filterCur = CurrencyCode());

  /// Helper method to retrieve a filtered market in the correct order from the exchange, according to optional filter
  /// currencies. For base and quote currency of the returned market, it is possible to have a neutral currency, which
  /// means that it has no constraints.
  Market determineMarketFromFilterCurrencies(MarketSet &markets, CurrencyCode filterCur1,
                                             CurrencyCode filterCur2 = CurrencyCode());

  static MarketPriceMap MarketPriceMapFromMarketOrderBookMap(const MarketOrderBookMap &marketOrderBookMap);

  const CoincenterInfo &coincenterInfo() const { return _coincenterInfo; }

  const ExchangeConfig &exchangeConfig() const { return _exchangeConfig; }

  CommonAPI &commonAPI() { return _commonApi; }

  /// Query withdrawal fee for given currency code.
  /// If no data found, return a 0 MonetaryAmount on given currency.
  MonetaryAmount queryWithdrawalFeeOrZero(CurrencyCode currencyCode);

 protected:
  friend class ExchangePrivate;

  ExchangePublic(std::string_view name, FiatConverter &fiatConverter, CommonAPI &commonApi,
                 const CoincenterInfo &coincenterInfo);

  std::string_view _name;
  CachedResultVault _cachedResultVault;
  FiatConverter &_fiatConverter;
  CommonAPI &_commonApi;
  const CoincenterInfo &_coincenterInfo;
  const ExchangeConfig &_exchangeConfig;
  std::recursive_mutex _publicRequestsMutex;
};
}  // namespace api
}  // namespace cct
