#pragma once

#include <chrono>

#include "apikey.hpp"
#include "balanceportfolio.hpp"
#include "cachedresultvault.hpp"
#include "cct_flatset.hpp"
#include "curlhandle.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangebase.hpp"
#include "exchangepublicapi.hpp"
#include "market.hpp"
#include "tradeinfo.hpp"
#include "wallet.hpp"
#include "withdrawinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class APIKey;
class TradeOptions;

class ExchangePrivate : public ExchangeBase {
 public:
  ExchangePrivate(const ExchangePrivate &) = delete;
  ExchangePrivate &operator=(const ExchangePrivate &) = delete;

  ExchangePrivate(ExchangePrivate &&) = default;
  ExchangePrivate &operator=(ExchangePrivate &&) = default;

  virtual ~ExchangePrivate() {}

  std::string_view keyName() const { return _apiKey.name(); }

  /// Retrieve the possible currencies known by current exchange.
  /// Information should be fully set with private key.
  virtual CurrencyExchangeFlatSet queryTradableCurrencies() = 0;

  /// Get a fast overview of the available assets on this exchange.
  /// @param equiCurrency (optional) if provided, attempt to convert each asset to given currency as an
  ///                     additional value information
  virtual BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) = 0;

  /// Get the deposit wallet of given currency associated to this exchange.
  virtual Wallet queryDepositWallet(CurrencyCode currencyCode) = 0;

  /// Convert given amount on one market determined by the currencies of start amount and the destination one.
  /// Returned MonetaryAmount is a net amount (fees deduced) in the other currency.
  /// This function is necessary a blocking call (synchronous) as it returns the converted amount.
  /// Because of this, it needs to expire at some point (and thus returning a non fully converted amount, or even 0 if
  /// nothing was traded).
  /// @param from the starting amount from which conversion will be done.
  ///             Will be modified containing the remaining (untraded) amount (0 if trade is complete)
  /// @param toCurrencyCode the destination currency
  /// @return converted net amount (fees deduced)
  MonetaryAmount trade(MonetaryAmount &from, CurrencyCode toCurrencyCode, const TradeOptions &options);

  /// The waiting time between each query of withdraw info to check withdraw status from an exchange.
  /// A very small value is not relevant as withdraw time order of magnitude are minutes (or hours with Bitcoin)
  static constexpr auto kWithdrawInfoRefreshTime = std::chrono::seconds(2);

  /// Orders a withdraw of an amount from current exchange to a destination wallet.
  /// This method will wait that current exchange has given confirmation of the withdraw,
  /// but will not wait for the deposit on destination address (it's not this exchange's object job anyway).
  /// @param grossAmount gross amount to withdraw, including the fee that will be deduced.
  /// @param targetExchange private exchange to which we should deliver the transfer
  /// @return information about the withdraw to be used by destination exchange to confirm the deposit
  virtual WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate &targetExchange) = 0;

 protected:
  ExchangePrivate(ExchangePublic &exchangePublic, const CoincenterInfo &config, const APIKey &apiKey)
      : ExchangeBase(), _exchangePublic(exchangePublic), _config(config), _apiKey(apiKey) {}

  /// Place an order in mode fire and forget.
  virtual PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                    const TradeInfo &tradeInfo) = 0;

  /// Cancel given order id and returned its possible matched amounts
  virtual OrderInfo cancelOrder(const OrderId &orderId, const TradeInfo &tradeInfo) = 0;

  virtual OrderInfo queryOrderInfo(const OrderId &orderId, const TradeInfo &tradeInfo) = 0;

  ExchangePublic &_exchangePublic;
  const CoincenterInfo &_config;
  const APIKey &_apiKey;
};
}  // namespace api
}  // namespace cct
