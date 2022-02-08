#pragma once

#include <optional>
#include <span>

#include "cct_smallvector.hpp"
#include "exchangeretriever.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"
#include "tradedamounts.hpp"

namespace cct {
class ExchangesOrchestrator {
 public:
  using TradedAmountsVector = SmallVector<TradedAmounts, kTypicalNbPrivateAccounts>;

  explicit ExchangesOrchestrator(std::span<Exchange> exchangesSpan) : _exchangeRetriever(exchangesSpan) {}

  ExchangeTickerMaps getTickerInformation(ExchangeNameSpan exchangeNames);

  MarketOrderBookConversionRates getMarketOrderBooks(Market m, ExchangeNameSpan exchangeNames,
                                                     CurrencyCode equiCurrencyCode, std::optional<int> depth);

  BalancePerExchange getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                CurrencyCode equiCurrency = CurrencyCode());

  WalletPerExchange getDepositInfo(std::span<const PrivateExchangeName> privateExchangeNames,
                                   CurrencyCode depositCurrency);

  OpenedOrdersPerExchange getOpenedOrders(std::span<const PrivateExchangeName> privateExchangeNames,
                                          const OrdersConstraints &openedOrdersConstraints);

  void cancelOrders(std::span<const PrivateExchangeName> privateExchangeNames,
                    const OrdersConstraints &ordersConstraints);

  ConversionPathPerExchange getConversionPaths(Market m, ExchangeNameSpan exchangeNames);

  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames,
                                                            bool shouldBeWithdrawable);

  UniquePublicSelectedExchanges getExchangesTradingMarket(Market m, ExchangeNameSpan exchangeNames);

  TradedAmounts trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                      std::span<const PrivateExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  TradedAmounts tradeAll(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                         std::span<const PrivateExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  WithdrawInfo withdraw(MonetaryAmount grossAmount, const PrivateExchangeName &fromPrivateExchangeName,
                        const PrivateExchangeName &toPrivateExchangeName);

  WithdrawFeePerExchange getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames);

  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market m, ExchangeNameSpan exchangeNames);

  LastTradesPerExchange getLastTradesPerExchange(Market m, ExchangeNameSpan exchangeNames, int nbLastTrades);

  MonetaryAmountPerExchange getLastPricePerExchange(Market m, ExchangeNameSpan exchangeNames);

 private:
  ExchangeRetriever _exchangeRetriever;
};
}  // namespace cct