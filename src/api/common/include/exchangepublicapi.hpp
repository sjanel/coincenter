#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "cct_flatset.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangebase.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "publictrade.hpp"
#include "tradedefinitions.hpp"

namespace cct {

class FiatConverter;
class TradeOptions;

namespace api {

class ExchangePublic : public ExchangeBase {
 public:
  static constexpr int kDefaultDepth = MarketOrderBook::kDefaultDepth;
  static constexpr int kNbLastTradesDefault = 100;

  using MarketSet = FlatSet<Market>;
  using Fiats = CryptowatchAPI::Fiats;
  using MarketOrderBookMap = std::unordered_map<Market, MarketOrderBook>;
  using MarketPriceMap = std::unordered_map<Market, MonetaryAmount>;
  using WithdrawalFeeMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
  using LastTradesVector = vector<PublicTrade>;

  ExchangePublic(const ExchangePublic &) = delete;
  ExchangePublic &operator=(const ExchangePublic &) = delete;

  ExchangePublic(ExchangePublic &&) noexcept = default;
  ExchangePublic &operator=(ExchangePublic &&) = delete;

  virtual ~ExchangePublic() {}

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
  /// Conversion is made with the 'average' price, which is the average of the lowest ask price and
  /// the highest bid price of a market order book if available.
  std::optional<MonetaryAmount> convertAtAveragePrice(MonetaryAmount a, CurrencyCode toCurrencyCode);

  /// Retrieve the fixed withdrawal fees per currency.
  /// Depending on the exchange, this could be retrieved dynamically,
  /// or, if not possible, should be retrieved from a static source updated regularly.
  virtual WithdrawalFeeMap queryWithdrawalFees() = 0;

  /// Retrieve the withdrawal fee of a Currency only
  virtual MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) = 0;

  /// Return true if exchange supports official REST API has an endpoint to get withdrawal fees
  /// For instance, Kraken does not offer such endpoint, we nee to query external sources which may provide inaccurate
  /// results
  virtual bool isWithdrawalFeesSourceReliable() const = 0;

  /// Get all the MarketOrderBooks of this exchange as fast as possible.
  /// Exchanges which do not support retrieval of all of them at once may used heuristic methods.
  virtual MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) = 0;

  /// Retrieve the order book of given market.
  /// It should be more precise that previous version with possibility to go deeper.
  virtual MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) = 0;

  /// Retrieve the total volume exchange on given market in the last 24 hours.
  virtual MonetaryAmount queryLast24hVolume(Market m) = 0;

  /// Retrieve an ordered vector of recent last trades
  virtual LastTradesVector queryLastTrades(Market m, int nbTrades = kNbLastTradesDefault) = 0;

  /// Retrieve the last price of given market.
  virtual MonetaryAmount queryLastPrice(Market m) = 0;

  Fiats queryFiats() { return _cryptowatchApi.queryFiats(); }

  /// Get the name of the exchange in lower case.
  std::string_view name() const { return _name; }

  using ConversionPath = SmallVector<Market, 3>;

  /// Retrieve the fastest conversion path (fastest in terms of number of conversions)
  /// of 'fromCurrencyCode' to 'toCurrencyCode'
  /// @return ordered list of Market (in the order in which they are defined in the exchange),
  ///         or empty list if conversion is not possible
  ConversionPath findFastestConversionPath(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode,
                                           const MarketSet &markets, const Fiats &fiats,
                                           bool considerStableCoinsAsFiats = false);

  ConversionPath findFastestConversionPath(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode,
                                           bool considerStableCoinsAsFiats = false) {
    return findFastestConversionPath(fromCurrencyCode, toCurrencyCode, queryTradableMarkets(), queryFiats(),
                                     considerStableCoinsAsFiats);
  }

  MonetaryAmount computeLimitOrderPrice(Market m, MonetaryAmount from, const TradeOptions &tradeOptions);

  MonetaryAmount computeAvgOrderPrice(Market m, MonetaryAmount from, const TradeOptions &tradeOptions);

  /// Retrieve the market in the correct order proposed by the exchange for given couple of currencies.
  Market retrieveMarket(CurrencyCode c1, CurrencyCode c2);

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

  MarketPriceMap marketPriceMapFromMarketOrderBookMap(const MarketOrderBookMap &marketOrderBookMap) const;

  const CoincenterInfo &coincenterInfo() const { return _coincenterInfo; }

  const ExchangeInfo &exchangeInfo() const { return _coincenterInfo.exchangeInfo(name()); }

 protected:
  friend class ExchangePrivate;

  ExchangePublic(std::string_view name, FiatConverter &fiatConverter, CryptowatchAPI &cryptowatchApi,
                 const CoincenterInfo &coincenterInfo)
      : _name(name), _fiatConverter(fiatConverter), _cryptowatchApi(cryptowatchApi), _coincenterInfo(coincenterInfo) {}

  string _name;
  CachedResultVault _cachedResultVault;
  FiatConverter &_fiatConverter;
  CryptowatchAPI &_cryptowatchApi;
  const CoincenterInfo &_coincenterInfo;
};
}  // namespace api
}  // namespace cct
