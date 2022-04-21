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
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

  explicit ExchangesOrchestrator(std::span<Exchange> exchangesSpan) : _exchangeRetriever(exchangesSpan) {}

  ExchangeTickerMaps getTickerInformation(ExchangeNameSpan exchangeNames);

  MarketOrderBookConversionRates getMarketOrderBooks(Market m, ExchangeNameSpan exchangeNames,
                                                     CurrencyCode equiCurrencyCode, std::optional<int> depth);

  BalancePerExchange getBalance(std::span<const ExchangeName> privateExchangeNames,
                                CurrencyCode equiCurrency = CurrencyCode());

  WalletPerExchange getDepositInfo(std::span<const ExchangeName> privateExchangeNames, CurrencyCode depositCurrency);

  OpenedOrdersPerExchange getOpenedOrders(std::span<const ExchangeName> privateExchangeNames,
                                          const OrdersConstraints &openedOrdersConstraints);

  NbCancelledOrdersPerExchange cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                                            const OrdersConstraints &ordersConstraints);

  ConversionPathPerExchange getConversionPaths(Market m, ExchangeNameSpan exchangeNames);

  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames,
                                                            bool shouldBeWithdrawable);

  UniquePublicSelectedExchanges getExchangesTradingMarket(Market m, ExchangeNameSpan exchangeNames);

  TradedAmounts trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                      std::span<const ExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  TradedAmountsVector smartBuy(MonetaryAmount endAmount, std::span<const ExchangeName> privateExchangeNames,
                               const TradeOptions &tradeOptions);

  TradedAmountsVector smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                std::span<const ExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  WithdrawInfo withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                        const ExchangeName &fromPrivateExchangeName, const ExchangeName &toPrivateExchangeName,
                        Duration withdrawRefreshTime = api::ExchangePrivate::kWithdrawInfoRefreshTime);

  WithdrawFeePerExchange getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames);

  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market m, ExchangeNameSpan exchangeNames);

  LastTradesPerExchange getLastTradesPerExchange(Market m, ExchangeNameSpan exchangeNames, int nbLastTrades);

  MonetaryAmountPerExchange getLastPricePerExchange(Market m, ExchangeNameSpan exchangeNames);

 private:
  ExchangeRetriever _exchangeRetriever;
};
}  // namespace cct
