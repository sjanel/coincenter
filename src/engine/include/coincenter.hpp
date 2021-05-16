#pragma once

#include <execution>
#include <forward_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "binancepublicapi.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_flatset.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangevector.hpp"
#include "fiatconverter.hpp"
#include "krakenprivateapi.hpp"
#include "krakenpublicapi.hpp"
#include "marketorderbooks.hpp"
#include "upbitprivateapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {

struct CoincenterParsedOptions;

namespace api {
class TradeOptions;
}  // namespace api

class Coincenter {
 public:
  using MarketOrderBookConversionRate = std::pair<MarketOrderBook, std::optional<MonetaryAmount>>;
  using MarketOrderBookConversionRates = cct::SmallVector<MarketOrderBookConversionRate, kTypicalNbExchanges>;

  Coincenter();

  Coincenter(const Coincenter &) = delete;
  Coincenter &operator=(const Coincenter &) = delete;

  Coincenter(Coincenter &&) noexcept = default;
  Coincenter &operator=(Coincenter &&) noexcept = default;

  void process(const CoincenterParsedOptions &opts);

  /// Retrieve market order book of market for given exchanges
  MarketOrderBooks getMarketOrderBooks(Market m, std::span<const PublicExchangeName> exchangeNames,
                                       std::optional<int> depth = std::nullopt);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market m, std::span<const PublicExchangeName> exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Query the private balance
  BalancePortfolio getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                              CurrencyCode equiCurrency = CurrencyCode::kNeutral);

  void printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode);

  void printConversionPath(std::span<const PublicExchangeName> exchangeNames, CurrencyCode fromCurrencyCode,
                           CurrencyCode toCurrencyCode);

  /// Single trade from 'startAmount' into 'toCurrency', on exchange named 'exchangeName'.
  /// Options should be wisely chosen here to avoid mistakes.
  MonetaryAmount trade(MonetaryAmount &startAmount, CurrencyCode toCurrency,
                       const PrivateExchangeName &privateExchangeName, const api::TradeOptions &tradeOptions);

  /// Single withdraw of 'grossAmount' from 'fromExchangeName' to 'toExchangeName'
  WithdrawInfo withdraw(MonetaryAmount grossAmount, const PrivateExchangeName &fromPrivateExchangeName,
                        const PrivateExchangeName &toPrivateExchangeName);

  PublicExchangeNames getPublicExchangeNames() const;

  void updateFileCaches() const;

  std::span<Exchange> exchanges() { return _exchanges; }

  using SelectedExchanges = cct::SmallVector<Exchange *, kTypicalNbExchanges>;

 private:
  using MarketsPerExchange = cct::vector<api::ExchangePublic::MarketSet>;
  using UniqueQueryRefresherHandles = cct::vector<ExchangeBase::UniqueQueryRefresherHandle>;
  using CurrencyExchangeSets = cct::vector<CurrencyExchangeFlatSet>;
  using WithdrawalFeeMapPerExchange = cct::vector<api::ExchangePublic::WithdrawalFeeMap>;
  using MarketsOrderBookPerExchange = cct::vector<api::ExchangePublic::MarketOrderBookMap>;

  static SelectedExchanges RetrieveSelectedExchanges(std::span<const PublicExchangeName> exchangeNames,
                                                     std::span<Exchange> exchanges);

  CurlInitRAII _curlInitRAII;
  CoincenterInfo _coincenterInfo;
  api::CryptowatchAPI _cryptowatchAPI;
  FiatConverter _fiatConverter;
  api::APIKeysProvider _apiKeyProvider;

  // Public exchanges
  api::KrakenPublic _krakenPublic;
  api::BithumbPublic _bithumbPublic;
  api::BinancePublic _binancePublic;
  api::UpbitPublic _upbitPublic;

  // Private exchanges (based on provided keys)
  // Use forward_list to guarantee validity of the iterators and pointers, as we give them to Exchange object as
  // pointers
  std::forward_list<api::KrakenPrivate> _krakenPrivates;
  std::forward_list<api::BinancePrivate> _binancePrivates;
  std::forward_list<api::BithumbPrivate> _bithumbPrivates;
  std::forward_list<api::UpbitPrivate> _upbitPrivates;

  ExchangeVector _exchanges;
};
}  // namespace cct
