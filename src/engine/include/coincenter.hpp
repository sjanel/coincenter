#pragma once

#include <execution>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "cct_flatset.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangevector.hpp"
#include "marketorderbooks.hpp"

namespace cct {

namespace api {
class CryptowatchAPI;
class TradeOptions;
}  // namespace api

class CoincenterInfo;
class FiatConverter;

class Coincenter {
 public:
  using MarketOrderBookConversionRate = std::pair<MarketOrderBook, std::optional<MonetaryAmount>>;
  using MarketOrderBookConversionRates = cct::SmallVector<MarketOrderBookConversionRate, kTypicalNbExchanges>;

  Coincenter(const CoincenterInfo &coincenterInfo, FiatConverter &fiatConverter, api::CryptowatchAPI &cryptowatchAPI,
             ExchangeVector &&exchanges);

  Coincenter(const Coincenter &) = delete;
  Coincenter &operator=(const Coincenter &) = delete;

  Coincenter(Coincenter &&) noexcept = default;
  Coincenter &operator=(Coincenter &&) noexcept = default;

  /// Retrieve market order book of market for given exchanges
  MarketOrderBooks getMarketOrderBooks(Market m, PublicExchangeNames exchangeNames,
                                       std::optional<int> depth = std::nullopt);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market m, PublicExchangeNames exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Query the private balance
  BalancePortfolio getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                              CurrencyCode equiCurrency = CurrencyCode::kNeutral);

  void printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode);

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

  static SelectedExchanges RetrieveSelectedExchanges(PublicExchangeNames exchangeNames, std::span<Exchange> exchanges);

  const CoincenterInfo &_coincenterInfo;
  FiatConverter &_fiatConverter;
  ExchangeVector _exchanges;
  api::CryptowatchAPI &_cryptowatchAPI;
};
}  // namespace cct
