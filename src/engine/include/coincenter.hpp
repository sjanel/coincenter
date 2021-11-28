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
#include "cct_vector.hpp"
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
#include "kucoinprivateapi.hpp"
#include "kucoinpublicapi.hpp"
#include "marketorderbooks.hpp"
#include "monitoringinfo.hpp"
#include "upbitprivateapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {

class CoincenterParsedOptions;
class TradeOptions;

class Coincenter {
 public:
  using MarketOrderBookConversionRate = std::tuple<std::string_view, MarketOrderBook, std::optional<MonetaryAmount>>;
  using MarketOrderBookConversionRates = FixedCapacityVector<MarketOrderBookConversionRate, kNbSupportedExchanges>;
  using MarketsPerExchange = FixedCapacityVector<api::ExchangePublic::MarketSet, kNbSupportedExchanges>;
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;
  using MonetaryAmountPerExchange = FixedCapacityVector<MonetaryAmount, kNbSupportedExchanges>;
  using MarketOrderBookMaps = FixedCapacityVector<api::ExchangePublic::MarketOrderBookMap, kNbSupportedExchanges>;
  using ExchangeTickerMaps = std::pair<ExchangeRetriever::PublicExchangesVec, Coincenter::MarketOrderBookMaps>;
  using BalancePerExchange = SmallVector<std::pair<const Exchange *, BalancePortfolio>, kTypicalNbPrivateAccounts>;
  using WalletPerExchange = SmallVector<std::pair<const Exchange *, Wallet>, kTypicalNbPrivateAccounts>;

  Coincenter(settings::RunMode runMode, std::string_view dataDir, const MonitoringInfo &monitoringInfo)
      : Coincenter(PublicExchangeNames(), false, runMode, dataDir, monitoringInfo) {}

  Coincenter(const PublicExchangeNames &exchangesWithoutSecrets, bool allExchangesWithoutSecrets,
             settings::RunMode runMode, std::string_view dataDir, const MonitoringInfo &monitoringInfo);

  Coincenter(const Coincenter &) = delete;
  Coincenter &operator=(const Coincenter &) = delete;

  Coincenter(Coincenter &&) = delete;
  Coincenter &operator=(Coincenter &&) = delete;

  void process(const CoincenterParsedOptions &opts);

  /// Retrieve the markets for given selected public exchanges, or all if empty.
  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur, std::span<const ExchangeName> exchangeNames);

  /// Retrieve ticker information for given selected public exchanges, or all if empty.
  ExchangeTickerMaps getTickerInformation(std::span<const ExchangeName> exchangeNames);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market m, std::span<const ExchangeName> exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Retrieve the last 24h traded volume for exchanges supporting given market.
  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market m, std::span<const ExchangeName> exchangeNames);

  /// Retrieve the last price for exchanges supporting given market.
  MonetaryAmountPerExchange getLastPricePerExchange(Market m, std::span<const ExchangeName> exchangeNames);

  /// Retrieve all matching Exchange references trading currency, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode,
                                                            std::span<const ExchangeName> exchangeNames);

  /// Retrieve all matching Exchange references proposing market, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingMarket(Market m, std::span<const ExchangeName> exchangeNames);

  /// Query the private balance
  BalancePerExchange getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                CurrencyCode equiCurrency = CurrencyCode::kNeutral);

  WalletPerExchange getDepositInfo(std::span<const PrivateExchangeName> privateExchangeNames,
                                   CurrencyCode depositCurrency);

  /// A Multi trade is similar to a single trade, at the difference that it retrieves the fastest currency conversion
  /// path and will launch several 'single' trades to reach that final goal. Example:
  ///  - Convert XRP to XLM on an exchange only proposing XRP-BTC and BTC-XLM markets will make 2 trades on these
  ///    markets.
  MonetaryAmount trade(MonetaryAmount &startAmount, CurrencyCode toCurrency,
                       const PrivateExchangeName &privateExchangeName, const TradeOptions &tradeOptions);

  MonetaryAmount tradeAll(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                          const PrivateExchangeName &privateExchangeName, const TradeOptions &tradeOptions);

  /// Single withdraw of 'grossAmount' from 'fromExchangeName' to 'toExchangeName'
  WithdrawInfo withdraw(MonetaryAmount grossAmount, const PrivateExchangeName &fromPrivateExchangeName,
                        const PrivateExchangeName &toPrivateExchangeName);

  void printMarkets(CurrencyCode currencyCode, std::span<const ExchangeName> exchangeNames);

  void printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const;

  void printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode);

  void printDepositInfo(const PrivateExchangeNames &privateExchangeNames, CurrencyCode depositCurrencyCode);

  void printConversionPath(std::span<const ExchangeName> exchangeNames, Market m);

  void printWithdrawFees(CurrencyCode currencyCode, std::span<const ExchangeName> exchangeNames);

  void printLast24hTradedVolume(Market m, std::span<const ExchangeName> exchangeNames);

  void printLastPrice(Market m, std::span<const ExchangeName> exchangeNames);

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
  using ExchangeVector = vector<Exchange>;

  void processReadRequests(const CoincenterParsedOptions &opts);
  void processWriteRequests(const CoincenterParsedOptions &opts);

  void exportBalanceMetrics(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) const;

  void exportTickerMetrics(std::span<api::ExchangePublic *> exchanges,
                           const MarketOrderBookMaps &marketOrderBookMaps) const;

  void exportOrderbookMetrics(Market m, const MarketOrderBookConversionRates &marketOrderBookConversionRates) const;

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
  api::KucoinPublic _kucoinPublic;
  api::UpbitPublic _upbitPublic;

  // Private exchanges (based on provided keys)
  // Use forward_list to guarantee validity of the iterators and pointers, as we give them to Exchange object as
  // pointers
  std::forward_list<api::BinancePrivate> _binancePrivates;
  std::forward_list<api::BithumbPrivate> _bithumbPrivates;
  std::forward_list<api::HuobiPrivate> _huobiPrivates;
  std::forward_list<api::KrakenPrivate> _krakenPrivates;
  std::forward_list<api::KucoinPrivate> _kucoinPrivates;
  std::forward_list<api::UpbitPrivate> _upbitPrivates;

  ExchangeVector _exchanges;
  ExchangeRetriever _exchangeRetriever;
  ConstExchangeRetriever _cexchangeRetriever;
};
}  // namespace cct
