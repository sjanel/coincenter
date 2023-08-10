#pragma once

#include <optional>
#include <span>

#include "exchangeretriever.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"
#include "threadpool.hpp"
#include "tradedamounts.hpp"
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

  BalancePerExchange getBalance(std::span<const ExchangeName> privateExchangeNames,
                                const BalanceOptions &balanceOptions = BalanceOptions());

  WalletPerExchange getDepositInfo(std::span<const ExchangeName> privateExchangeNames, CurrencyCode depositCurrency);

  OpenedOrdersPerExchange getOpenedOrders(std::span<const ExchangeName> privateExchangeNames,
                                          const OrdersConstraints &openedOrdersConstraints);

  NbCancelledOrdersPerExchange cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                                            const OrdersConstraints &ordersConstraints);

  DepositsPerExchange getRecentDeposits(std::span<const ExchangeName> privateExchangeNames,
                                        const DepositsConstraints &depositsConstraints);

  WithdrawsPerExchange getRecentWithdraws(std::span<const ExchangeName> privateExchangeNames,
                                          const WithdrawsConstraints &withdrawsConstraints);

  ConversionPathPerExchange getConversionPaths(Market mk, ExchangeNameSpan exchangeNames);

  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames,
                                                            bool shouldBeWithdrawable);

  UniquePublicSelectedExchanges getExchangesTradingMarket(Market mk, ExchangeNameSpan exchangeNames);

  TradedAmountsPerExchange trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                                 std::span<const ExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  TradedAmountsPerExchange smartBuy(MonetaryAmount endAmount, std::span<const ExchangeName> privateExchangeNames,
                                    const TradeOptions &tradeOptions);

  TradedAmountsPerExchange smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                     std::span<const ExchangeName> privateExchangeNames,
                                     const TradeOptions &tradeOptions);

  TradedAmountsVectorWithFinalAmountPerExchange dustSweeper(std::span<const ExchangeName> privateExchangeNames,
                                                            CurrencyCode currencyCode);

  DeliveredWithdrawInfoWithExchanges withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                              const ExchangeName &fromPrivateExchangeName,
                                              const ExchangeName &toPrivateExchangeName,
                                              const WithdrawOptions &withdrawOptions);

  MonetaryAmountPerExchange getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames);

  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market mk, ExchangeNameSpan exchangeNames);

  LastTradesPerExchange getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames, int nbLastTrades);

  MonetaryAmountPerExchange getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames);

 private:
  ExchangeRetriever _exchangeRetriever;
  ThreadPool _threadPool;
};
}  // namespace cct
