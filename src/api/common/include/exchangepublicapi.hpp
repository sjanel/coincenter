#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "cct_flatset.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
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

namespace api {
class CryptowatchAPI;

class ExchangePublic : public ExchangeBase {
 public:
  static constexpr int kDefaultDepth = MarketOrderBook::kDefaultDepth;
  static constexpr int kNbLastTradesDefault = 100;

  using MarketSet = FlatSet<Market>;
  using MarketOrderBookMap = std::unordered_map<Market, MarketOrderBook>;
  using MarketPriceMap = std::unordered_map<Market, MonetaryAmount>;
  using WithdrawalFeeMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
  using LastTradesVector = vector<PublicTrade>;

  ExchangePublic(const ExchangePublic &) = delete;
  ExchangePublic &operator=(const ExchangePublic &) = delete;

  ExchangePublic(ExchangePublic &&) = default;
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

  /// Get the name of the exchange in lower case.
  std::string_view name() const { return _name; }

  using ConversionPath = SmallVector<Market, 3>;

  /// Retrieve the fastest conversion path (fastest in terms of number of conversions)
  /// of 'fromCurrencyCode' to 'toCurrencyCode'
  /// @return ordered list of Market (in the order in which they are defined in the exchange),
  ///         or empty list if conversion is not possible
  ConversionPath findFastestConversionPath(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode,
                                           bool considerStableCoinsAsFiats = false);

  MonetaryAmount computeLimitOrderPrice(Market m, MonetaryAmount from, TradePriceStrategy priceStrategy);

  MonetaryAmount computeAvgOrderPrice(Market m, MonetaryAmount from, TradePriceStrategy priceStrategy,
                                      int depth = kDefaultDepth);

  /// Retrieve the market in the correct order proposed by the exchange for given couple of currencies.
  Market retrieveMarket(CurrencyCode c1, CurrencyCode c2);

  MarketPriceMap marketPriceMapFromMarketOrderBookMap(const MarketOrderBookMap &marketOrderBookMap) const;

  const CoincenterInfo &coincenterInfo() const { return _coincenterInfo; }

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
