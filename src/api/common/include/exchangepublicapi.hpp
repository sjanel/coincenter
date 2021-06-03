#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "cachedresultvault.hpp"
#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangebase.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"

namespace cct {

class FiatConverter;

namespace api {
class CryptowatchAPI;

class ExchangePublic : public ExchangeBase {
 public:
  using MarketSet = FlatSet<Market>;
  using MarketOrderBookMap = std::unordered_map<Market, MarketOrderBook>;
  using MarketPriceMap = std::unordered_map<Market, MonetaryAmount>;
  using WithdrawalFeeMap = std::unordered_map<CurrencyCode, MonetaryAmount>;

  ExchangePublic(const ExchangePublic &) = delete;
  ExchangePublic &operator=(const ExchangePublic &) = delete;

  ExchangePublic(ExchangePublic &&) = default;
  ExchangePublic &operator=(ExchangePublic &&) = default;

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
  virtual MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = 10) = 0;

  /// Retrieve the order book of given market.
  /// It should be more precise that previous version with possibility to go deeper.
  virtual MarketOrderBook queryOrderBook(Market m, int depth = 10) = 0;

  /// Get the name of the exchange in lower case.
  std::string_view name() const { return _name; }

  using Currencies = cct::SmallVector<CurrencyCode, 6>;

  /// Retrieve the fastest conversion path (fastest in terms of number of conversions)
  /// of 'm.base()' towards 'm.quote()' currency code
  /// @return ordered list of currency code, or empty list if conversion is not possible
  Currencies findFastestConversionPath(Market conversionMarket, bool considerStableCoinsAsFiats = false);

  MonetaryAmount computeLimitOrderPrice(Market m, MonetaryAmount from);

  MonetaryAmount computeAvgOrderPrice(Market m, MonetaryAmount from, bool isTakerStrategy, int depth = 10);

  /// Retrieve the market in the correct order proposed by the exchange for given couple of currencies.
  Market retrieveMarket(CurrencyCode c1, CurrencyCode c2);

  MarketPriceMap marketPriceMapFromMarketOrderBookMap(const MarketOrderBookMap &marketOrderBookMap) const;

 protected:
  ExchangePublic(std::string_view name, FiatConverter &fiatConverter, CryptowatchAPI &cryptowatchApi,
                 const CoincenterInfo &coincenterInfo)
      : _name(name), _fiatConverter(fiatConverter), _cryptowatchApi(cryptowatchApi), _coincenterInfo(coincenterInfo) {}

  std::string _name;
  FiatConverter &_fiatConverter;
  CryptowatchAPI &_cryptowatchApi;
  const CoincenterInfo &_coincenterInfo;
};
}  // namespace api
}  // namespace cct
