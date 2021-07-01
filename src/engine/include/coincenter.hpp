#pragma once

#include <execution>
#include <forward_list>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "binancepublicapi.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangeretriever.hpp"
#include "fiatconverter.hpp"
#include "huobiprivateapi.hpp"
#include "huobipublicapi.hpp"
#include "krakenprivateapi.hpp"
#include "krakenpublicapi.hpp"
#include "marketorderbooks.hpp"
#include "upbitprivateapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {

class CoincenterParsedOptions;

namespace api {
class TradeOptions;
}  // namespace api

class Coincenter {
 public:
  using MarketOrderBookConversionRate = std::tuple<std::string_view, MarketOrderBook, std::optional<MonetaryAmount>>;
  using MarketOrderBookConversionRates = cct::FixedCapacityVector<MarketOrderBookConversionRate, kNbSupportedExchanges>;
  using MarketsPerExchange = FixedCapacityVector<api::ExchangePublic::MarketSet, kNbSupportedExchanges>;
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;
  using MonetaryAmountPerExchange = FixedCapacityVector<MonetaryAmount, kNbSupportedExchanges>;

  explicit Coincenter(settings::RunMode runMode = settings::RunMode::kProd);

  Coincenter(const Coincenter &) = delete;
  Coincenter &operator=(const Coincenter &) = delete;

  Coincenter(Coincenter &&) = delete;
  Coincenter &operator=(Coincenter &&) = delete;

  void process(const CoincenterParsedOptions &opts);

  /// Retrieve the markets for given selected public exchanges, or all if empty.
  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur, std::span<const PublicExchangeName> exchangeNames);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market m, std::span<const PublicExchangeName> exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Retrieve the last 24h traded volume for exchanges supporting given market.
  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market m,
                                                              std::span<const PublicExchangeName> exchangeNames);

  /// Retrieve the last price for exchanges supporting given market.
  MonetaryAmountPerExchange getLastPricePerExchange(Market m, std::span<const PublicExchangeName> exchangeNames);

  /// Retrieve all matching Exchange references trading currency, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode,
                                                            std::span<const PublicExchangeName> exchangeNames);

  /// Retrieve all matching Exchange references proposing market, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingMarket(Market m, std::span<const PublicExchangeName> exchangeNames);

  /// Query the private balance
  BalancePortfolio getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                              CurrencyCode equiCurrency = CurrencyCode::kNeutral);

  /// Single trade from 'startAmount' into 'toCurrency', on exchange named 'exchangeName'.
  /// Options should be wisely chosen here to avoid mistakes.
  MonetaryAmount trade(MonetaryAmount &startAmount, CurrencyCode toCurrency,
                       const PrivateExchangeName &privateExchangeName, const api::TradeOptions &tradeOptions);

  /// Single withdraw of 'grossAmount' from 'fromExchangeName' to 'toExchangeName'
  WithdrawInfo withdraw(MonetaryAmount grossAmount, const PrivateExchangeName &fromPrivateExchangeName,
                        const PrivateExchangeName &toPrivateExchangeName);

  void printMarkets(CurrencyCode currencyCode, std::span<const PublicExchangeName> exchangeNames);

  void printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode);

  void printConversionPath(std::span<const PublicExchangeName> exchangeNames, Market m);

  void printWithdrawFees(CurrencyCode currencyCode, std::span<const PublicExchangeName> exchangeNames);

  void printLast24hTradedVolume(Market m, std::span<const PublicExchangeName> exchangeNames);

  void printLastPrice(Market m, std::span<const PublicExchangeName> exchangeNames);

  PublicExchangeNames getPublicExchangeNames() const;

  /// Dumps the content of all file caches in data directory to save cURL queries.
  void updateFileCaches() const;

  std::span<Exchange> exchanges() { return _exchanges; }
  std::span<const Exchange> exchanges() const { return _exchanges; }

  CoincenterInfo &coincenterInfo() { return _coincenterInfo; }
  const CoincenterInfo &coincenterInfo() const { return _coincenterInfo; }

  api::CryptowatchAPI &cryptowatchAPI() { return _cryptowatchAPI; }
  const api::CryptowatchAPI &cryptowatchAPI() const { return _cryptowatchAPI; }

  FiatConverter &fiatConverter() { return _fiatConverter; }
  const FiatConverter &fiatConverter() const { return _fiatConverter; }

 private:
  using ExchangeVector = SmallVector<Exchange, kTypicalNbPrivateAccounts>;

  CurlInitRAII _curlInitRAII;
  CoincenterInfo _coincenterInfo;
  api::CryptowatchAPI _cryptowatchAPI;
  FiatConverter _fiatConverter;
  api::APIKeysProvider _apiKeyProvider;

  // Public exchanges
  api::BinancePublic _binancePublic;
  api::BithumbPublic _bithumbPublic;
  api::HuobiPublic _huobiPublic;
  api::KrakenPublic _krakenPublic;
  api::UpbitPublic _upbitPublic;

  // Private exchanges (based on provided keys)
  // Use forward_list to guarantee validity of the iterators and pointers, as we give them to Exchange object as
  // pointers
  std::forward_list<api::KrakenPrivate> _krakenPrivates;
  std::forward_list<api::BinancePrivate> _binancePrivates;
  std::forward_list<api::HuobiPrivate> _huobiPrivates;
  std::forward_list<api::BithumbPrivate> _bithumbPrivates;
  std::forward_list<api::UpbitPrivate> _upbitPrivates;

  ExchangeVector _exchanges;
  ExchangeRetriever _exchangeRetriever;
  ConstExchangeRetriever _cexchangeRetriever;
};
}  // namespace cct
