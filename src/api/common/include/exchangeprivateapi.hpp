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
#include "openedorder.hpp"
#include "openedordersconstraints.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "wallet.hpp"
#include "withdrawinfo.hpp"

namespace cct {

class CoincenterInfo;
class TradeOptions;

namespace api {
class APIKey;

class ExchangePrivate : public ExchangeBase {
 public:
  using WithdrawalFeeMap = ExchangePublic::WithdrawalFeeMap;
  using OpenedOrders = FlatSet<OpenedOrder>;

  ExchangePrivate(const ExchangePrivate &) = delete;
  ExchangePrivate &operator=(const ExchangePrivate &) = delete;

  ExchangePrivate(ExchangePrivate &&) = default;
  ExchangePrivate &operator=(ExchangePrivate &&) = delete;

  virtual ~ExchangePrivate() {}

  std::string_view keyName() const { return _apiKey.name(); }

  /// Retrieve the possible currencies known by current exchange.
  /// Information should be fully set with private key.
  virtual CurrencyExchangeFlatSet queryTradableCurrencies() = 0;

  /// Get a fast overview of the available assets on this exchange.
  /// @param equiCurrency (optional) if provided, attempt to convert each asset to given currency as an
  ///                     additional value information
  BalancePortfolio getAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral);

  /// Get the deposit wallet of given currency associated to this exchange.
  virtual Wallet queryDepositWallet(CurrencyCode currencyCode) = 0;

  /// Get opened orders filtered according to given constraints
  virtual OpenedOrders queryOpenedOrders(
      const OpenedOrdersConstraints &openedOrdersConstraints = OpenedOrdersConstraints()) = 0;

  /// Convert given amount on one market determined by the currencies of start amount and the destination one.
  /// Returned MonetaryAmount is a net amount (fees deduced) in the other currency.
  /// This function is necessarily a blocking call (synchronous) as it returns the converted amount.
  /// Because of this, it needs to expire at some point (and thus returning a non fully converted amount, or even 0
  /// if nothing was traded).
  /// @param from the starting amount from which conversion will be done
  /// @param toCurrencyCode the destination currency
  /// @return trade amounts (fees deduced)
  TradedAmounts trade(MonetaryAmount from, CurrencyCode toCurrencyCode, const TradeOptions &options);

  /// The waiting time between each query of withdraw info to check withdraw status from an exchange.
  /// A very small value is not relevant as withdraw time order of magnitude are minutes (or hours with Bitcoin)
  static constexpr auto kWithdrawInfoRefreshTime = std::chrono::seconds(5);

  /// Withdraws an amount from 'this' exchange to 'targetExchange'.
  /// This method is synchronous:
  ///   - It first waits that withdrawal has been successfully sent from 'this'
  ///   - It then waits that deposit has arrived to 'targetExchange'
  /// @param grossAmount gross amount to withdraw, including the fee that will be deduced.
  /// @param targetExchange private exchange to which we should deliver the transfer
  /// @return information about the withdraw
  WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate &targetExchange);

  /// Retrieve the fixed withdrawal fees per currency.
  /// Some exchanges provide this service in the public REST API but not all, hence this private API flavor.
  virtual WithdrawalFeeMap queryWithdrawalFees() { return _exchangePublic.queryWithdrawalFees(); }

  /// Retrieve the withdrawal fee of a Currency only
  /// Some exchanges provide this service in the public REST API but not all, hence this private API flavor.
  virtual MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) {
    return _exchangePublic.queryWithdrawalFee(currencyCode);
  }

  PrivateExchangeName createPrivateExchangeName() const {
    return PrivateExchangeName(_exchangePublic.name(), _apiKey.name());
  }

  const ExchangeInfo &exchangeInfo() const { return _exchangePublic.exchangeInfo(); }

 protected:
  ExchangePrivate(const CoincenterInfo &coincenterInfo, ExchangePublic &exchangePublic, const APIKey &apiKey)
      : ExchangeBase(),
        _exchangePublic(exchangePublic),
        _cachedResultVault(exchangePublic._cachedResultVault),
        _coincenterInfo(coincenterInfo),
        _apiKey(apiKey) {}

  virtual BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) = 0;

  /// Adds an amount to given BalancePortfolio.
  /// @param equiCurrency Asks conversion of given amount into this currency as well
  void addBalance(BalancePortfolio &balancePortfolio, MonetaryAmount amount, CurrencyCode equiCurrency);

  /// Return true if exchange supports simulated order (some exchanges such as Kraken or Binance for instance support
  /// this query parameter)
  virtual bool isSimulatedOrderSupported() const = 0;

  /// Place an order in mode fire and forget.
  /// When this method ends, order should be successfully placed in the exchange, or if not possible (for instance, too
  /// small volume) return a closed PlaceOrderInfo.
  /// This method will not be called in simulation mode if the exchange does not support it (ie: when
  /// isSimulatedOrderSupported == false)
  /// @param from the remaining from amount to trade
  virtual PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                    const TradeInfo &tradeInfo) = 0;

  /// Cancel given order id and return its possible matched amounts.
  /// When this methods ends, order should be successfully cancelled and its matched parts returned definitely (trade
  /// automaton will not come back on this order later on)
  virtual OrderInfo cancelOrder(const OrderId &orderId, const TradeInfo &tradeInfo) = 0;

  /// Query an order and return and 'OrderInfo' with its matched parts and if it is closed or not (closed means that its
  /// status and matched parts will not evolve in the future).
  virtual OrderInfo queryOrderInfo(const OrderId &orderId, const TradeInfo &tradeInfo) = 0;

  /// Orders a withdraw in mode fire and forget.
  virtual InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet &&wallet) = 0;

  /// Check if withdraw has been confirmed and successful from 'this' exchange
  virtual SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo &initiatedWithdrawInfo) = 0;

  /// Check if withdraw has been received by 'this' exchange
  virtual bool isWithdrawReceived(const InitiatedWithdrawInfo &initiatedWithdrawInfo,
                                  const SentWithdrawInfo &sentWithdrawInfo) = 0;

  TradedAmounts singleTrade(MonetaryAmount from, CurrencyCode toCurrencyCode, const TradeOptions &options, Market m);

  TradedAmounts multiTrade(MonetaryAmount from, CurrencyCode toCurrencyCode, const TradeOptions &options);

  ExchangePublic &_exchangePublic;
  CachedResultVault &_cachedResultVault;
  const CoincenterInfo &_coincenterInfo;
  const APIKey &_apiKey;

 private:
  PlaceOrderInfo placeOrderProcess(MonetaryAmount &from, MonetaryAmount price, const TradeInfo &tradeInfo);

  PlaceOrderInfo computeSimulatedMatchedPlacedOrderInfo(MonetaryAmount volume, MonetaryAmount price,
                                                        const TradeInfo &tradeInfo) const;
};
}  // namespace api
}  // namespace cct
