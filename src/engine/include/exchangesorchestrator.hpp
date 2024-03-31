#pragma once

#include <optional>
#include <span>

#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "exchangeretriever.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"
#include "threadpool.hpp"
#include "withdrawoptions.hpp"

namespace cct {

class RequestsConfig;
class ExchangesOrchestrator {
 public:
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

  explicit ExchangesOrchestrator(const RequestsConfig &requestsConfig, std::span<Exchange> exchangesSpan);

  ExchangeHealthCheckStatus healthCheck(ExchangeNameSpan exchangeNames);

  ExchangeTickerMaps getTickerInformation(ExchangeNameSpan exchangeNames);

  MarketOrderBookConversionRates getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                     CurrencyCode equiCurrencyCode, std::optional<int> depth);

  BalancePerExchange getBalance(ExchangeNameSpan privateExchangeNames,
                                const BalanceOptions &balanceOptions = BalanceOptions());

  WalletPerExchange getDepositInfo(ExchangeNameSpan privateExchangeNames, CurrencyCode depositCurrency);

  ClosedOrdersPerExchange getClosedOrders(ExchangeNameSpan privateExchangeNames,
                                          const OrdersConstraints &closedOrdersConstraints);

  OpenedOrdersPerExchange getOpenedOrders(ExchangeNameSpan privateExchangeNames,
                                          const OrdersConstraints &openedOrdersConstraints);

  NbCancelledOrdersPerExchange cancelOrders(ExchangeNameSpan privateExchangeNames,
                                            const OrdersConstraints &ordersConstraints);

  DepositsPerExchange getRecentDeposits(ExchangeNameSpan privateExchangeNames,
                                        const DepositsConstraints &depositsConstraints);

  WithdrawsPerExchange getRecentWithdraws(ExchangeNameSpan privateExchangeNames,
                                          const WithdrawsConstraints &withdrawsConstraints);

  MonetaryAmountPerExchange getConversion(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                                          ExchangeNameSpan exchangeNames);

  MonetaryAmountPerExchange getConversion(std::span<const MonetaryAmount> monetaryAmountPerExchangeToConvert,
                                          CurrencyCode targetCurrencyCode, ExchangeNameSpan exchangeNames);

  ConversionPathPerExchange getConversionPaths(Market mk, ExchangeNameSpan exchangeNames);

  CurrenciesPerExchange getCurrenciesPerExchange(ExchangeNameSpan exchangeNames);

  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames,
                                                            bool shouldBeWithdrawable);

  UniquePublicSelectedExchanges getExchangesTradingMarket(Market mk, ExchangeNameSpan exchangeNames);

  TradeResultPerExchange trade(MonetaryAmount from, bool isPercentageTrade, CurrencyCode toCurrency,
                               ExchangeNameSpan privateExchangeNames, const TradeOptions &tradeOptions);

  TradeResultPerExchange smartBuy(MonetaryAmount endAmount, ExchangeNameSpan privateExchangeNames,
                                  const TradeOptions &tradeOptions);

  TradeResultPerExchange smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                   ExchangeNameSpan privateExchangeNames, const TradeOptions &tradeOptions);

  TradedAmountsVectorWithFinalAmountPerExchange dustSweeper(ExchangeNameSpan privateExchangeNames,
                                                            CurrencyCode currencyCode);

  DeliveredWithdrawInfoWithExchanges withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                              const ExchangeName &fromPrivateExchangeName,
                                              const ExchangeName &toPrivateExchangeName,
                                              const WithdrawOptions &withdrawOptions);

  MonetaryAmountByCurrencySetPerExchange getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames);

  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market mk, ExchangeNameSpan exchangeNames);

  TradesPerExchange getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames, std::optional<int> depth);

  MonetaryAmountPerExchange getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames);

 private:
  ExchangeRetriever _exchangeRetriever;
  ThreadPool _threadPool;
};
}  // namespace cct
