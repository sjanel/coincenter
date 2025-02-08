#include "queryresultprinter.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "apioutputtype.hpp"
#include "balanceperexchangeportfolio.hpp"
#include "balanceportfolio.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "closed-order.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "currencycodevector.hpp"
#include "currencyexchange.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchange-name-enum.hpp"
#include "exchange.hpp"
#include "exchangepublicapitypes.hpp"
#include "logginginfo.hpp"
#include "market-timestamp.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "ordersconstraints.hpp"
#include "priceoptions.hpp"
#include "publictrade.hpp"
#include "query-result-schema.hpp"
#include "query-result-type-helpers.hpp"
#include "queryresulttypes.hpp"
#include "simpletable.hpp"
#include "stringconv.hpp"
#include "time-window.hpp"
#include "timestring.hpp"
#include "trade-range-stats.hpp"
#include "tradedamounts.hpp"
#include "tradeside.hpp"
#include "unreachable.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawsconstraints.hpp"
#include "withdrawsordepositsconstraints.hpp"

namespace cct {
namespace {

auto HealthCheckJson(const ExchangeHealthCheckStatus &healthCheckPerExchange) {
  schema::queryresult::HealthCheck obj;

  obj.out.reserve(healthCheckPerExchange.size());
  for (const auto &[exchange, healthCheckValue] : healthCheckPerExchange) {
    obj.out.emplace_back(exchange->exchangeNameEnum(), healthCheckValue);
  }

  return obj;
}

auto CurrenciesJson(const CurrenciesPerExchange &currenciesPerExchange) {
  schema::queryresult::CurrenciesPerExchange obj;

  for (const auto &[exchange, currencies] : currenciesPerExchange) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &pair = obj.out.emplace_back(exchange->exchangeNameEnum(), ExchangePartType{});

    pair.second.reserve(currencies.size());
    for (const CurrencyExchange &cur : currencies) {
      auto &currency = pair.second.emplace_back();

      currency.code = cur.standardCode();
      currency.exchangeCode = cur.exchangeCode();
      currency.altCode = cur.altCode();
      currency.canDeposit = cur.canDeposit();
      currency.canWithdraw = cur.canWithdraw();
      currency.isFiat = cur.isFiat();
    }
  }

  return obj;
}

auto MarketsJson(CurrencyCode cur1, CurrencyCode cur2, const MarketsPerExchange &marketsPerExchange) {
  schema::queryresult::Markets obj;

  if (!cur1.isNeutral()) {
    obj.in.opt.cur1 = cur1;
  }
  if (!cur2.isNeutral()) {
    obj.in.opt.cur2 = cur2;
  }
  obj.out.reserve(marketsPerExchange.size());
  for (const auto &[exchange, markets] : marketsPerExchange) {
    obj.out.emplace_back(exchange->exchangeNameEnum(), markets);
  }

  return obj;
}

auto MarketsForReplayJson(TimeWindow timeWindow, const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange) {
  schema::queryresult::MarketsForReplay obj;

  if (timeWindow != TimeWindow{}) {
    obj.in.opt.timeWindow = timeWindow;
  }

  for (const auto &[exchange, marketTimestampSets] : marketTimestampSetsPerExchange) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &exchangePart = obj.out.emplace_back(exchange->exchangeNameEnum(), ExchangePartType{}).second;

    exchangePart.orderBooks.reserve(marketTimestampSets.orderBooksMarkets.size());
    for (const MarketTimestamp &marketTimestamp : marketTimestampSets.orderBooksMarkets) {
      exchangePart.orderBooks.emplace_back(marketTimestamp.market, TimeToString(marketTimestamp.timePoint));
    }

    exchangePart.trades.reserve(marketTimestampSets.tradesMarkets.size());
    for (const MarketTimestamp &marketTimestamp : marketTimestampSets.tradesMarkets) {
      exchangePart.trades.emplace_back(marketTimestamp.market, TimeToString(marketTimestamp.timePoint));
    }
  }

  return obj;
}

auto TickerInformationJson(const ExchangeTickerMaps &exchangeTickerMaps) {
  schema::queryresult::TickerInformation obj;
  for (const auto &[exchange, marketOrderBookMap] : exchangeTickerMaps) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &exchangePart = obj.out.emplace_back(exchange->exchangeNameEnum(), ExchangePartType{}).second;

    exchangePart.reserve(marketOrderBookMap.size());
    for (const auto &[mk, marketOrderBook] : marketOrderBookMap) {
      auto &ticker = exchangePart.emplace_back();

      ticker.pair = mk;

      ticker.ask.a = marketOrderBook.amountAtAskPrice().toNeutral();
      ticker.ask.p = marketOrderBook.lowestAskPrice().toNeutral();
      ticker.bid.a = marketOrderBook.amountAtBidPrice().toNeutral();
      ticker.bid.p = marketOrderBook.highestBidPrice().toNeutral();
    }
    // Sort rows by market pair for consistent output
    std::ranges::sort(exchangePart, [](const auto &lhs, const auto &rhs) { return lhs.pair < rhs.pair; });
  }

  return obj;
}

void AppendOrderbookLine(const MarketOrderBook &marketOrderBook, int pos,
                         std::optional<MonetaryAmount> optConversionRate, auto &data) {
  auto [amount, price] = marketOrderBook[pos];
  auto &line = data.emplace_back();
  line.a = amount.toNeutral();
  line.p = price.toNeutral();

  if (optConversionRate) {
    line.eq = optConversionRate->toNeutral();
  }
}

auto MarketOrderBooksJson(Market mk, CurrencyCode equiCurrencyCode, std::optional<int> depth,
                          const MarketOrderBookConversionRates &marketOrderBooksConversionRates) {
  schema::queryresult::MarketOrderBooks obj;

  obj.in.opt.pair = mk;
  if (!equiCurrencyCode.isNeutral()) {
    obj.in.opt.equiCurrency = equiCurrencyCode;
  }
  obj.in.opt.depth = depth;

  obj.out.reserve(marketOrderBooksConversionRates.size());
  for (const auto &[exchangeNameEnum, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &exchangePart = obj.out.emplace_back(exchangeNameEnum, ExchangePartType{}).second;

    exchangePart.time.ts = marketOrderBook.time();
    for (int bidPos = 1; bidPos <= marketOrderBook.nbBidPrices(); ++bidPos) {
      AppendOrderbookLine(marketOrderBook, -bidPos, optConversionRate, exchangePart.bid);
    }
    for (int askPos = 1; askPos <= marketOrderBook.nbAskPrices(); ++askPos) {
      AppendOrderbookLine(marketOrderBook, askPos, optConversionRate, exchangePart.ask);
    }
  }

  return obj;
}

auto &GetExchangePart(const Exchange *exchange, auto &out) {
  using ExchangePart = std::remove_cvref_t<decltype(out)>::value_type::second_type;
  auto it = std::ranges::find_if(
      out, [exchange](const auto &exchangePart) { return exchangePart.first == exchange->exchangeNameEnum(); });
  if (it == out.end()) {
    return out.emplace_back(exchange->exchangeNameEnum(), ExchangePart{}).second;
  }
  return it->second;
}

auto BalanceJson(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) {
  schema::queryresult::Balance obj;

  BalancePortfolio totalBalance;

  using ExchangePart = decltype(obj.out.exchange)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;
  using CurrencyPart = ExchangeKeyPart::value_type::second_type;

  const bool hasEquiCurrency = !equiCurrency.isNeutral();

  if (hasEquiCurrency) {
    obj.in.opt.equiCurrency = equiCurrency;
  }

  for (const auto &[exchange, balance] : balancePerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out.exchange);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    for (const auto &[amount, equiAmount] : balance) {
      auto &currencyPart = exchangeKeyPart.emplace_back(amount.currencyCode(), CurrencyPart{}).second;
      currencyPart.a = amount.toNeutral();
      if (hasEquiCurrency) {
        currencyPart.eq = equiAmount.toNeutral();
      }
    }

    totalBalance += balance;
  }

  MonetaryAmount totalEq(0, equiCurrency);
  for (const auto &[amount, equiAmount] : totalBalance) {
    auto &currencyPart = obj.out.total.cur.emplace_back(amount.currencyCode(), CurrencyPart{}).second;
    currencyPart.a = amount.toNeutral();
    if (hasEquiCurrency) {
      currencyPart.eq = equiAmount.toNeutral();
      totalEq += equiAmount;
    }
  }
  if (hasEquiCurrency) {
    obj.out.total.eq = totalEq.toNeutral();
  }

  return obj;
}

auto DepositInfoJson(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange) {
  schema::queryresult::DepositInfo obj;

  obj.in.opt.cur = depositCurrencyCode;

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;

  for (const auto &[exchange, wallet] : walletPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.address = wallet.address();
    if (wallet.hasTag()) {
      exchangeKeyPart.tag = wallet.tag();
    }
  }

  return obj;
}

auto TradesJson(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount amount, bool isPercentageTrade,
                CurrencyCode toCurrency, const TradeOptions &tradeOptions, CoincenterCommandType commandType) {
  schema::queryresult::Trades obj;

  obj.in.req = commandType;

  auto &from = obj.in.opt.from.emplace();

  from.amount = amount.toNeutral();

  from.currency = amount.currencyCode();
  from.isPercentage = isPercentageTrade;

  switch (commandType) {
    case CoincenterCommandType::Buy:
      std::swap(obj.in.opt.from, obj.in.opt.to);
      break;
    case CoincenterCommandType::Sell:
      break;
    case CoincenterCommandType::Trade: {
      obj.in.opt.to.emplace().currency = toCurrency;
      break;
    }
    default:
      unreachable();
  }

  auto &opts = obj.in.opt.options;

  const auto &priceOptions = tradeOptions.priceOptions();

  opts.price.strategy = priceOptions.priceStrategy();
  if (priceOptions.isFixedPrice()) {
    opts.price.fixedPrice = priceOptions.fixedPrice();
  }
  if (priceOptions.isRelativePrice()) {
    opts.price.relativePrice = priceOptions.relativePrice();
  }
  opts.maxTradeTime.duration = tradeOptions.maxTradeTime();
  opts.minTimeBetweenPriceUpdates.duration = tradeOptions.minTimeBetweenPriceUpdates();
  opts.mode = tradeOptions.tradeMode();
  opts.syncPolicy = tradeOptions.tradeSyncPolicy();
  opts.timeoutAction = tradeOptions.timeoutAction();

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;

  for (const auto &[exchange, tradeResult] : tradeResultPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.from = tradeResult.from().toNeutral();
    exchangeKeyPart.status = tradeResult.state();
    exchangeKeyPart.tradedFrom = tradeResult.tradedAmounts().from.toNeutral();
    exchangeKeyPart.tradedTo = tradeResult.tradedAmounts().to.toNeutral();
  }

  return obj;
}

void SetOrdersConstraints(std::optional<schema::queryresult::Orders::In::Opt> &opt,
                          const OrdersConstraints &ordersConstraints) {
  auto &initializedOpt = opt.emplace();
  bool reset = true;
  if (ordersConstraints.isCurDefined()) {
    initializedOpt.cur1 = ordersConstraints.cur1();
    reset = false;
  }
  if (ordersConstraints.isCur2Defined()) {
    initializedOpt.cur2 = ordersConstraints.cur2();
    reset = false;
  }
  if (ordersConstraints.isPlacedTimeBeforeDefined()) {
    initializedOpt.placedBefore.emplace(ordersConstraints.placedBefore());
    reset = false;
  }
  if (ordersConstraints.isPlacedTimeAfterDefined()) {
    initializedOpt.placedAfter.emplace(ordersConstraints.placedAfter());
    reset = false;
  }
  if (ordersConstraints.isOrderIdDefined()) {
    initializedOpt.matchIds = ordersConstraints.orderIdSet();
    reset = false;
  }
  if (reset) {
    opt.reset();
  }
}

void SetOrder(const auto &orderData, auto &order) {
  using OrderType = std::remove_cvref_t<decltype(orderData)>;

  order.id = orderData.id();
  order.pair = orderData.market();
  order.placedTime.ts = orderData.placedTime();
  if constexpr (std::is_same_v<ClosedOrder, OrderType>) {
    order.matchedTime.emplace(orderData.matchedTime());
  }
  order.side = orderData.side();
  order.price = orderData.price().toNeutral();
  order.matched = orderData.matchedVolume().toNeutral();
  if constexpr (std::is_same_v<OpenedOrder, OrderType>) {
    order.remaining = orderData.remainingVolume().toNeutral();
  }
}

template <class OrdersPerExchangeType>
auto OrdersJson(CoincenterCommandType coincenterCommandType, const OrdersPerExchangeType &ordersPerExchange,
                const OrdersConstraints &ordersConstraints) {
  schema::queryresult::Orders obj;

  obj.in.req = coincenterCommandType;

  SetOrdersConstraints(obj.in.opt, ordersConstraints);

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;

  for (const auto &[exchange, ordersData] : ordersPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.reserve(ordersData.size());
    for (const auto &orderData : ordersData) {
      SetOrder(orderData, exchangeKeyPart.emplace_back());
    }
  }

  return obj;
}

auto OrdersCancelledJson(const NbCancelledOrdersPerExchange &nbCancelledOrdersPerExchange,
                         const OrdersConstraints &ordersConstraints) {
  schema::queryresult::OrdersCancelled obj;

  obj.in.req = CoincenterCommandType::OrdersCancel;

  SetOrdersConstraints(obj.in.opt, ordersConstraints);

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;

  for (const auto &[exchange, nbCancelledOrders] : nbCancelledOrdersPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.nb = nbCancelledOrders;
  }

  return obj;
}

enum class DepositOrWithdrawEnum : int8_t { kDeposit, kWithdraw };

void SetDepositOrWithdrawConstraints(std::optional<schema::queryresult::RecentDeposits::In::Opt> &opt,
                                     const WithdrawsOrDepositsConstraints &constraints,
                                     DepositOrWithdrawEnum depositOrWithdraw) {
  auto &initializedOpt = opt.emplace();
  bool reset = true;
  if (constraints.isCurDefined()) {
    initializedOpt.cur = constraints.currencyCode();
    reset = false;
  }
  const bool isDeposit = depositOrWithdraw == DepositOrWithdrawEnum::kDeposit;
  if (constraints.isTimeBeforeDefined()) {
    if (isDeposit) {
      initializedOpt.receivedBefore.emplace(constraints.timeBefore());
    } else {
      initializedOpt.sentBefore.emplace(constraints.timeBefore());
    }
    reset = false;
  }
  if (constraints.isTimeAfterDefined()) {
    if (isDeposit) {
      initializedOpt.receivedAfter.emplace(constraints.timeAfter());
    } else {
      initializedOpt.sentAfter.emplace(constraints.timeAfter());
    }
    reset = false;
  }
  if (constraints.isIdDefined()) {
    initializedOpt.matchIds = constraints.idSet();
    reset = false;
  }
  if (reset) {
    opt.reset();
  }
}

auto RecentDepositsJson(const DepositsPerExchange &depositsPerExchange,
                        const DepositsConstraints &depositsConstraints) {
  schema::queryresult::RecentDeposits obj;

  SetDepositOrWithdrawConstraints(obj.in.opt, depositsConstraints, DepositOrWithdrawEnum::kDeposit);

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;
  for (const auto &[exchange, deposits] : depositsPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.reserve(deposits.size());
    for (const Deposit &deposit : deposits) {
      auto &elem = exchangeKeyPart.emplace_back();
      elem.id = deposit.id();
      elem.cur = deposit.amount().currencyCode();
      elem.receivedTime.ts = deposit.time();
      elem.amount = deposit.amount().toNeutral();
      elem.status = deposit.status();
    }
  }

  return obj;
}

auto RecentWithdrawsJson(const WithdrawsPerExchange &withdrawsPerExchange,
                         const WithdrawsConstraints &withdrawsConstraints) {
  schema::queryresult::RecentWithdraws obj;

  SetDepositOrWithdrawConstraints(obj.in.opt, withdrawsConstraints, DepositOrWithdrawEnum::kWithdraw);

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;
  for (const auto &[exchange, withdraws] : withdrawsPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.reserve(withdraws.size());
    for (const Withdraw &withdraw : withdraws) {
      auto &elem = exchangeKeyPart.emplace_back();
      elem.id = withdraw.id();
      elem.cur = withdraw.amount().currencyCode();
      elem.sentTime.ts = withdraw.time();
      elem.netEmittedAmount = withdraw.amount().toNeutral();
      elem.fee = withdraw.withdrawFee().toNeutral();
      elem.status = withdraw.status();
    }
  }

  return obj;
}

auto ConversionJson(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                    const MonetaryAmountPerExchange &conversionPerExchange) {
  schema::queryresult::Conversion1 obj;

  obj.in.opt.fromAmount = amount.toNeutral();
  obj.in.opt.fromCurrency = amount.currencyCode();
  obj.in.opt.toCurrency = targetCurrencyCode;

  for (const auto &[exchange, convertedAmount] : conversionPerExchange) {
    if (convertedAmount != 0) {
      using ExchangePart = decltype(obj.out)::value_type::second_type;

      obj.out.emplace_back(exchange->exchangeNameEnum(), ExchangePart{.convertedAmount = convertedAmount.toNeutral()});
    }
  }

  return obj;
}

auto ConversionJson(std::span<const MonetaryAmount> startAmountPerExchangePos, CurrencyCode targetCurrencyCode,
                    const MonetaryAmountPerExchange &conversionPerExchange) {
  schema::queryresult::Conversion2 obj;

  obj.in.opt.toCurrency = targetCurrencyCode;

  int publicExchangePos{};
  for (MonetaryAmount startAmount : startAmountPerExchangePos) {
    if (!startAmount.isDefault()) {
      using ExchangePart = decltype(obj.in.opt.fromAmount)::value_type::second_type;

      obj.in.opt.fromAmount.emplace_back(
          static_cast<ExchangeNameEnum>(publicExchangePos),
          ExchangePart{.amount = startAmount.toNeutral(), .cur = startAmount.currencyCode()});
    }
    ++publicExchangePos;
  }

  for (const auto &[exchange, convertedAmount] : conversionPerExchange) {
    if (convertedAmount != 0) {
      using ExchangePart = decltype(obj.out)::value_type::second_type;

      obj.out.emplace_back(exchange->exchangeNameEnum(), ExchangePart{.convertedAmount = convertedAmount.toNeutral()});
    }
  }

  return obj;
}

auto ConversionPathJson(Market mk, const ConversionPathPerExchange &conversionPathsPerExchange) {
  schema::queryresult::ConversionPath obj;

  obj.in.opt.market = mk;

  for (const auto &[exchange, conversionPath] : conversionPathsPerExchange) {
    if (!conversionPath.empty()) {
      obj.out.emplace_back(exchange->exchangeNameEnum(), conversionPath);
    }
  }

  return obj;
}

auto WithdrawFeesJson(const MonetaryAmountByCurrencySetPerExchange &withdrawFeePerExchange, CurrencyCode cur) {
  schema::queryresult::WithdrawFees obj;

  if (!cur.isNeutral()) {
    obj.in.opt.cur = cur;
  }

  for (const auto &[exchange, withdrawFees] : withdrawFeePerExchange) {
    obj.out.emplace_back(exchange->exchangeNameEnum(), withdrawFees);
  }

  return obj;
}

auto Last24hTradedVolumeJson(Market mk, const MonetaryAmountPerExchange &tradedVolumePerExchange) {
  schema::queryresult::Last24hTradedVolume obj;
  obj.in.opt.market = mk;

  for (const auto &[exchange, tradedVolume] : tradedVolumePerExchange) {
    obj.out.emplace_back(exchange->exchangeNameEnum(), tradedVolume.toNeutral());
  }

  return obj;
}

auto LastTradesJson(Market mk, std::optional<int> nbLastTrades, const TradesPerExchange &lastTradesPerExchange) {
  schema::queryresult::LastTrades obj;

  obj.in.opt.market = mk;
  obj.in.opt.nb = nbLastTrades;

  for (const auto &[exchange, lastTrades] : lastTradesPerExchange) {
    using ExchangePart = decltype(obj.out)::value_type::second_type;

    auto &exchangePart = obj.out.emplace_back(exchange->exchangeNameEnum(), ExchangePart{}).second;

    exchangePart.reserve(lastTrades.size());
    for (const PublicTrade &trade : lastTrades) {
      auto &lastTrade = exchangePart.emplace_back();

      lastTrade.a = trade.amount().toNeutral();
      lastTrade.p = trade.price().toNeutral();
      lastTrade.time.ts = trade.time();
      lastTrade.side = trade.side();
    }
  }

  return obj;
}

auto LastPriceJson(Market mk, const MonetaryAmountPerExchange &pricePerExchange) {
  schema::queryresult::LastPrice obj;

  obj.in.opt.market = mk;

  for (const auto &[exchange, lastPrice] : pricePerExchange) {
    obj.out.emplace_back(exchange->exchangeNameEnum(), lastPrice.toNeutral());
  }

  return obj;
}

auto WithdrawJson(const DeliveredWithdrawInfo &deliveredWithdrawInfo, MonetaryAmount grossAmount,
                  bool isPercentageWithdraw, const Exchange &fromExchange, const Exchange &toExchange,
                  const WithdrawOptions &withdrawOptions) {
  schema::queryresult::Withdraw obj;

  obj.in.opt.cur = grossAmount.currencyCode();
  obj.in.opt.isPercentage = isPercentageWithdraw;
  obj.in.opt.syncPolicy = withdrawOptions.withdrawSyncPolicy();

  obj.out.from.exchange = fromExchange.exchangeNameEnum();
  obj.out.from.account = fromExchange.keyName();
  obj.out.from.id = deliveredWithdrawInfo.withdrawId();
  obj.out.from.amount = grossAmount.toNeutral();
  obj.out.from.time.ts = deliveredWithdrawInfo.initiatedTime();

  obj.out.to.exchange = toExchange.exchangeNameEnum();
  obj.out.to.account = toExchange.keyName();
  obj.out.to.id = deliveredWithdrawInfo.depositId();
  obj.out.to.amount = deliveredWithdrawInfo.receivedAmount().toNeutral();
  obj.out.to.address = deliveredWithdrawInfo.receivingWallet().address();
  if (deliveredWithdrawInfo.receivingWallet().hasTag()) {
    obj.out.to.tag = deliveredWithdrawInfo.receivingWallet().tag();
  }
  obj.out.to.time.ts = deliveredWithdrawInfo.receivedTime();

  return obj;
}

auto DustSweeperJson(const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
                     CurrencyCode currencyCode) {
  schema::queryresult::DustSweeper obj;

  obj.in.opt.cur = currencyCode;

  using ExchangePart = decltype(obj.out)::value_type::second_type;
  using ExchangeKeyPart = ExchangePart::value_type::second_type;

  for (const auto &[exchange, tradedAmountsVectorWithFinalAmount] : tradedAmountsVectorWithFinalAmountPerExchange) {
    auto &exchangePart = GetExchangePart(exchange, obj.out);
    auto &exchangeKeyPart = exchangePart.emplace_back(exchange->keyName(), ExchangeKeyPart{}).second;

    exchangeKeyPart.trades.reserve(tradedAmountsVectorWithFinalAmount.tradedAmountsVector.size());
    for (const auto &tradedAmounts : tradedAmountsVectorWithFinalAmount.tradedAmountsVector) {
      exchangeKeyPart.trades.emplace_back(tradedAmounts.from, tradedAmounts.to);
    }
    exchangeKeyPart.finalAmount = tradedAmountsVectorWithFinalAmount.finalAmount;
  }

  return obj;
}

auto MarketTradingResultsJson(TimeWindow inputTimeWindow, const ReplayResults &replayResults,
                              CoincenterCommandType commandType) {
  schema::queryresult::MarketTradingResults obj;

  obj.in.req = commandType;

  obj.in.opt.time.from.ts = inputTimeWindow.from();
  obj.in.opt.time.to.ts = inputTimeWindow.to();

  obj.out.reserve(replayResults.size());
  for (const auto &[algorithmName, marketTradingResultPerExchangeVector] : replayResults) {
    using AlgorithmNameResults = decltype(obj.out)::value_type::second_type;
    auto &algorithmNameResults = obj.out.emplace_back(algorithmName, AlgorithmNameResults{}).second;

    algorithmNameResults.reserve(marketTradingResultPerExchangeVector.size());
    for (const auto &marketTradingResultPerExchange : marketTradingResultPerExchangeVector) {
      using AllResults = AlgorithmNameResults::value_type;

      auto &allResults = algorithmNameResults.emplace_back();

      allResults.reserve(marketTradingResultPerExchange.size());
      for (const auto &[exchange, marketGlobalTradingResult] : marketTradingResultPerExchange) {
        const auto &marketTradingResult = marketGlobalTradingResult.result;
        const auto &stats = marketGlobalTradingResult.stats;

        auto &exchangeMarketResults = allResults.emplace_back();

        using MarketTradingResult = AllResults::value_type::value_type::second_type;

        auto &marketTradingResultPart =
            exchangeMarketResults.emplace_back(exchange->exchangeNameEnum(), MarketTradingResult{}).second;

        marketTradingResultPart.algorithm = marketTradingResult.algorithmName();
        marketTradingResultPart.market = marketTradingResult.market();
        marketTradingResultPart.startAmounts.base = marketTradingResult.startBaseAmount();
        marketTradingResultPart.startAmounts.quote = marketTradingResult.startQuoteAmount();
        marketTradingResultPart.profitAndLoss = marketTradingResult.quoteAmountDelta();
        marketTradingResultPart.stats.orderBooks.nbSuccessful = stats.marketOrderBookStats.nbSuccessful;
        marketTradingResultPart.stats.orderBooks.nbError = stats.marketOrderBookStats.nbError;
        marketTradingResultPart.stats.orderBooks.time.from.ts = stats.marketOrderBookStats.timeWindow.from();
        marketTradingResultPart.stats.orderBooks.time.to.ts = stats.marketOrderBookStats.timeWindow.to();
        marketTradingResultPart.stats.trades.nbSuccessful = stats.publicTradeStats.nbSuccessful;
        marketTradingResultPart.stats.trades.nbError = stats.publicTradeStats.nbError;
        marketTradingResultPart.stats.trades.time.from.ts = stats.publicTradeStats.timeWindow.from();
        marketTradingResultPart.stats.trades.time.to.ts = stats.publicTradeStats.timeWindow.to();

        marketTradingResultPart.matchedOrders.reserve(marketTradingResult.matchedOrders().size());
        for (const ClosedOrder &closedOrder : marketTradingResult.matchedOrders()) {
          SetOrder(closedOrder, marketTradingResultPart.matchedOrders.emplace_back());
        }
      }
    }
  }

  return obj;
}

template <class VecType>
void RemoveDuplicates(VecType &vec) {
  std::ranges::sort(vec);
  const auto [eraseIt1, eraseIt2] = std::ranges::unique(vec);
  vec.erase(eraseIt1, eraseIt2);
}

}  // namespace
QueryResultPrinter::QueryResultPrinter(ApiOutputType apiOutputType, const LoggingInfo &loggingInfo)
    : _loggingInfo(loggingInfo),
      _outputLogger(log::get(LoggingInfo::kOutputLoggerName)),
      _apiOutputType(apiOutputType) {}

QueryResultPrinter::QueryResultPrinter(std::ostream &os, ApiOutputType apiOutputType, const LoggingInfo &loggingInfo)
    : _loggingInfo(loggingInfo),
      _pOs(&os),
      _outputLogger(log::get(LoggingInfo::kOutputLoggerName)),
      _apiOutputType(apiOutputType) {}

void QueryResultPrinter::printHealthCheck(const ExchangeHealthCheckStatus &healthCheckPerExchange) const {
  auto jsonObj = HealthCheckJson(healthCheckPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.reserve(1U + healthCheckPerExchange.size());
      table.emplace_back("Exchange", "Health Check status");
      for (const auto &[exchange, healthCheckValue] : healthCheckPerExchange) {
        table.emplace_back(exchange->name(), healthCheckValue ? "OK" : "Not OK!");
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::HealthCheck, jsonObj);
}

namespace {
void AppendWithExchangeName(string &str, std::string_view value, std::string_view exchangeName) {
  if (!str.empty()) {
    str.push_back(',');
  }
  str.append(value);
  str.push_back('[');
  str.append(exchangeName);
  str.push_back(']');
}

void Append(string &str, std::string_view exchangeName) {
  if (!str.empty()) {
    str.push_back(',');
  }
  str.append(exchangeName);
}
}  // namespace

void QueryResultPrinter::printCurrencies(const CurrenciesPerExchange &currenciesPerExchange) const {
  auto jsonObj = CurrenciesJson(currenciesPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      // Compute all currencies for all exchanges
      CurrencyCodeVector allCurrencyCodes;

      for (const auto &[_, currencies] : currenciesPerExchange) {
        allCurrencyCodes.insert(allCurrencyCodes.end(), currencies.begin(), currencies.end());
      }

      RemoveDuplicates(allCurrencyCodes);
      SimpleTable table;

      table.reserve(1U + allCurrencyCodes.size());

      table.emplace_back("Currency", "Supported exchanges", "Exchange code(s)", "Alt code(s)", "Can deposit to",
                         "Can withdraw from", "Is fiat");

      for (CurrencyCode cur : allCurrencyCodes) {
        string supportedExchanges;
        string exchangeCodes;
        string altCodes;
        string canDeposit;
        string canWithdraw;
        std::optional<bool> isFiat;
        const Exchange *pPrevExchange = nullptr;
        for (const auto &[exchange, currencies] : currenciesPerExchange) {
          auto it = currencies.find(cur);
          if (it != currencies.end()) {
            // This exchange has this currency
            Append(supportedExchanges, exchange->name());
            if (cur != it->exchangeCode()) {
              AppendWithExchangeName(exchangeCodes, it->exchangeCode().str(), exchange->name());
            }
            if (cur != it->altCode()) {
              AppendWithExchangeName(altCodes, it->altCode().str(), exchange->name());
            }
            if (it->canDeposit()) {
              Append(canDeposit, exchange->name());
            }
            if (it->canWithdraw()) {
              Append(canWithdraw, exchange->name());
            }
            if (!isFiat) {
              isFiat = it->isFiat();
            } else if (*isFiat != it->isFiat()) {
              log::warn("{} and {} disagree on whether {} is a fiat - consider not fiat", pPrevExchange->name(),
                        exchange->name(), cur);
              isFiat = false;
            }
          }
          pPrevExchange = exchange;
        }

        table.emplace_back(cur.str(), std::move(supportedExchanges), std::move(exchangeCodes), std::move(altCodes),
                           std::move(canDeposit), std::move(canWithdraw), isFiat.value_or(false));
      }

      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Currencies, jsonObj);
}

void QueryResultPrinter::printMarkets(CurrencyCode cur1, CurrencyCode cur2,
                                      const MarketsPerExchange &marketsPerExchange,
                                      CoincenterCommandType coincenterCommandType) const {
  auto jsonObj = MarketsJson(cur1, cur2, marketsPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string marketsCol("Markets");
      if (!cur1.isNeutral()) {
        marketsCol.append(" with ");
        cur1.appendStrTo(marketsCol);
      }
      if (!cur2.isNeutral()) {
        marketsCol.push_back('-');
        cur2.appendStrTo(marketsCol);
      }
      SimpleTable table;
      table.emplace_back("Exchange", std::move(marketsCol));
      for (const auto &[exchange, markets] : marketsPerExchange) {
        for (Market mk : markets) {
          table.emplace_back(exchange->name(), mk.str());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(coincenterCommandType, jsonObj);
}

void QueryResultPrinter::printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const {
  auto jsonObj = TickerInformationJson(exchangeTickerMaps);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.emplace_back("Exchange", "Market", "Bid price", "Bid volume", "Ask price", "Ask volume");
      for (const auto &[exchange, marketOrderBookMap] : exchangeTickerMaps) {
        for (const auto &[mk, marketOrderBook] : marketOrderBookMap) {
          table.emplace_back(exchange->name(), mk.str(), marketOrderBook.highestBidPrice().str(),
                             marketOrderBook.amountAtBidPrice().str(), marketOrderBook.lowestAskPrice().str(),
                             marketOrderBook.amountAtAskPrice().str());
        }
        // Sort rows in lexicographical order for consistent output
        std::ranges::sort(table);
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Ticker, jsonObj);
}

void QueryResultPrinter::printMarketOrderBooks(
    Market mk, CurrencyCode equiCurrencyCode, std::optional<int> depth,
    const MarketOrderBookConversionRates &marketOrderBooksConversionRates) const {
  const auto jsonObj = MarketOrderBooksJson(mk, equiCurrencyCode, depth, marketOrderBooksConversionRates);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      for (const auto &[exchangeNameEnum, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
        printTable(marketOrderBook.getTable(exchangeNameEnum, optConversionRate));
      }
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Orderbook, jsonObj);
}

void QueryResultPrinter::printBalance(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) const {
  auto jsonObj = BalanceJson(balancePerExchange, equiCurrency);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      BalancePerExchangePortfolio totalBalance(balancePerExchange);
      printTable(totalBalance.getTable(balancePerExchange.size() > 1));
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Balance, jsonObj);
}

void QueryResultPrinter::printDepositInfo(CurrencyCode depositCurrencyCode,
                                          const WalletPerExchange &walletPerExchange) const {
  auto jsonObj = DepositInfoJson(depositCurrencyCode, walletPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string walletStr(depositCurrencyCode.str());
      walletStr.append(" address");
      SimpleTable table;
      table.reserve(1U + walletPerExchange.size());
      table.emplace_back("Exchange", "Account", std::move(walletStr), "Destination Tag");
      for (const auto &[exchangePtr, wallet] : walletPerExchange) {
        table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), wallet.address(), wallet.tag());
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::DepositInfo, jsonObj);
}

void QueryResultPrinter::printTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount amount,
                                     bool isPercentageTrade, CurrencyCode toCurrency, const TradeOptions &tradeOptions,
                                     CoincenterCommandType commandType) const {
  auto jsonObj = TradesJson(tradeResultPerExchange, amount, isPercentageTrade, toCurrency, tradeOptions, commandType);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string tradedFromStr("Traded from amount (");
      string tradeModeStr(EnumToString(tradeOptions.tradeMode()));
      tradedFromStr.append(tradeModeStr);
      tradedFromStr.push_back(')');
      string tradedToStr("Traded to amount (");
      tradedToStr.append(tradeModeStr);
      tradedToStr.push_back(')');
      SimpleTable table;

      table.reserve(1U + tradeResultPerExchange.size());
      table.emplace_back("Exchange", "Account", "From", std::move(tradedFromStr), std::move(tradedToStr), "Status");

      for (const auto &[exchangePtr, tradeResult] : tradeResultPerExchange) {
        const TradedAmounts &tradedAmounts = tradeResult.tradedAmounts();

        table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), tradeResult.from().str(),
                           tradedAmounts.from.str(), tradedAmounts.to.str(), EnumToString(tradeResult.state()));
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(commandType, jsonObj, tradeOptions.isSimulation());
}

void QueryResultPrinter::printClosedOrders(const ClosedOrdersPerExchange &closedOrdersPerExchange,
                                           const OrdersConstraints &ordersConstraints) const {
  auto jsonObj = OrdersJson(CoincenterCommandType::OrdersClosed, closedOrdersPerExchange, ordersConstraints);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.emplace_back("Exchange", "Account", "Exchange Id", "Placed time", "Matched time", "Side", "Price",
                         "Matched Amount");
      for (const auto &[exchangePtr, closedOrders] : closedOrdersPerExchange) {
        for (const ClosedOrder &closedOrder : closedOrders) {
          table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), closedOrder.id(),
                             TimeToString(closedOrder.placedTime()), TimeToString(closedOrder.matchedTime()),
                             EnumToString(closedOrder.side()), closedOrder.price().str(),
                             closedOrder.matchedVolume().str());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::OrdersClosed, jsonObj);
}

void QueryResultPrinter::printOpenedOrders(const OpenedOrdersPerExchange &openedOrdersPerExchange,
                                           const OrdersConstraints &ordersConstraints) const {
  auto jsonObj = OrdersJson(CoincenterCommandType::OrdersOpened, openedOrdersPerExchange, ordersConstraints);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.emplace_back("Exchange", "Account", "Exchange Id", "Placed time", "Side", "Price", "Matched Amount",
                         "Remaining Amount");
      for (const auto &[exchangePtr, openedOrders] : openedOrdersPerExchange) {
        for (const OpenedOrder &openedOrder : openedOrders) {
          table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), openedOrder.id(),
                             TimeToString(openedOrder.placedTime()), EnumToString(openedOrder.side()),
                             openedOrder.price().str(), openedOrder.matchedVolume().str(),
                             openedOrder.remainingVolume().str());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::OrdersOpened, jsonObj);
}

void QueryResultPrinter::printCancelledOrders(const NbCancelledOrdersPerExchange &nbCancelledOrdersPerExchange,
                                              const OrdersConstraints &ordersConstraints) const {
  auto jsonObj = OrdersCancelledJson(nbCancelledOrdersPerExchange, ordersConstraints);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.reserve(1U + nbCancelledOrdersPerExchange.size());
      table.emplace_back("Exchange", "Account", "Number of cancelled orders");
      for (const auto &[exchangePtr, nbCancelledOrders] : nbCancelledOrdersPerExchange) {
        table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), nbCancelledOrders);
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::OrdersCancel, jsonObj);
}

void QueryResultPrinter::printRecentDeposits(const DepositsPerExchange &depositsPerExchange,
                                             const DepositsConstraints &depositsConstraints) const {
  auto jsonObj = RecentDepositsJson(depositsPerExchange, depositsConstraints);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.emplace_back("Exchange", "Account", "Exchange Id", "Received time", "Amount", "Status");
      for (const auto &[exchangePtr, deposits] : depositsPerExchange) {
        for (const Deposit &deposit : deposits) {
          table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), deposit.id(), deposit.timeStr(),
                             deposit.amount().str(), deposit.statusStr());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::RecentDeposits, jsonObj);
}

void QueryResultPrinter::printRecentWithdraws(const WithdrawsPerExchange &withdrawsPerExchange,
                                              const WithdrawsConstraints &withdrawsConstraints) const {
  auto jsonObj = RecentWithdrawsJson(withdrawsPerExchange, withdrawsConstraints);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.emplace_back("Exchange", "Account", "Exchange Id", "Sent time", "Net Emitted Amount", "Fee", "Status");
      for (const auto &[exchangePtr, withdraws] : withdrawsPerExchange) {
        for (const Withdraw &withdraw : withdraws) {
          table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), withdraw.id(), withdraw.timeStr(),
                             withdraw.amount().str(), withdraw.withdrawFee().str(), withdraw.statusStr());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::RecentWithdraws, jsonObj);
}

void QueryResultPrinter::printConversion(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                                         const MonetaryAmountPerExchange &conversionPerExchange) const {
  auto jsonObj = ConversionJson(amount, targetCurrencyCode, conversionPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string conversionStrHeader = amount.str();
      conversionStrHeader.append(" converted into ");
      targetCurrencyCode.appendStrTo(conversionStrHeader);

      SimpleTable table;
      table.reserve(1U + conversionPerExchange.size());
      table.emplace_back("Exchange", std::move(conversionStrHeader));
      for (const auto &[exchange, convertedAmount] : conversionPerExchange) {
        if (convertedAmount != 0) {
          table.emplace_back(exchange->name(), convertedAmount.str());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Conversion, jsonObj);
}

void QueryResultPrinter::printConversion(std::span<const MonetaryAmount> startAmountPerExchangePos,
                                         CurrencyCode targetCurrencyCode,
                                         const MonetaryAmountPerExchange &conversionPerExchange) const {
  auto jsonObj = ConversionJson(startAmountPerExchangePos, targetCurrencyCode, conversionPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.reserve(1U + conversionPerExchange.size());
      table.emplace_back("Exchange", "From", "To");
      for (const auto &[exchange, convertedAmount] : conversionPerExchange) {
        if (convertedAmount != 0) {
          table.emplace_back(exchange->name(), startAmountPerExchangePos[exchange->publicExchangePos()].str(),
                             convertedAmount.str());
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Conversion, jsonObj);
}

void QueryResultPrinter::printConversionPath(Market mk,
                                             const ConversionPathPerExchange &conversionPathsPerExchange) const {
  auto jsonObj = ConversionPathJson(mk, conversionPathsPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string conversionPathStrHeader("Fastest conversion path for ");
      conversionPathStrHeader.append(mk.str());
      SimpleTable table;
      table.reserve(1U + conversionPathsPerExchange.size());
      table.emplace_back("Exchange", std::move(conversionPathStrHeader));
      for (const auto &[exchange, conversionPath] : conversionPathsPerExchange) {
        if (conversionPath.empty()) {
          continue;
        }
        string conversionPathStr;
        for (Market market : conversionPath) {
          if (!conversionPathStr.empty()) {
            conversionPathStr.push_back(',');
          }
          conversionPathStr.append(market.str());
        }
        table.emplace_back(exchange->name(), std::move(conversionPathStr));
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::ConversionPath, jsonObj);
}

void QueryResultPrinter::printWithdrawFees(const MonetaryAmountByCurrencySetPerExchange &withdrawFeesPerExchange,
                                           CurrencyCode currencyCode) const {
  auto jsonObj = WithdrawFeesJson(withdrawFeesPerExchange, currencyCode);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      table::Row header("Withdraw fee currency");
      CurrencyCodeVector allCurrencyCodes;
      for (const auto &[exchange, withdrawFees] : withdrawFeesPerExchange) {
        header.emplace_back(exchange->name());
        for (MonetaryAmount ma : withdrawFees) {
          allCurrencyCodes.push_back(ma.currencyCode());
        }
      }

      RemoveDuplicates(allCurrencyCodes);

      SimpleTable table;
      table.reserve(1U + allCurrencyCodes.size());

      table.emplace_back(std::move(header));
      for (CurrencyCode cur : allCurrencyCodes) {
        auto &row = table.emplace_back(cur.str());
        for (const auto &[exchange, withdrawFees] : withdrawFeesPerExchange) {
          auto it = withdrawFees.find(cur);
          if (it == withdrawFees.end()) {
            row.emplace_back("");
          } else {
            row.emplace_back(it->str());
          }
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::WithdrawFees, jsonObj);
}

void QueryResultPrinter::printLast24hTradedVolume(Market mk,
                                                  const MonetaryAmountPerExchange &tradedVolumePerExchange) const {
  auto jsonObj = Last24hTradedVolumeJson(mk, tradedVolumePerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string headerTradedVolume("Last 24h ");
      headerTradedVolume.append(mk.str());
      headerTradedVolume.append(" traded volume");
      SimpleTable table;
      table.reserve(1U + tradedVolumePerExchange.size());
      table.emplace_back("Exchange", std::move(headerTradedVolume));
      for (const auto &[exchange, tradedVolume] : tradedVolumePerExchange) {
        table.emplace_back(exchange->name(), tradedVolume.str());
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Last24hTradedVolume, jsonObj);
}

void QueryResultPrinter::printLastTrades(Market mk, std::optional<int> nbLastTrades,
                                         const TradesPerExchange &lastTradesPerExchange) const {
  auto jsonObj = LastTradesJson(mk, nbLastTrades, lastTradesPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
        string buyTitle = mk.base().str();
        string sellTitle = buyTitle;
        buyTitle.append(" buys");
        sellTitle.append(" sells");
        string priceTitle("Price in ");
        mk.quote().appendStrTo(priceTitle);

        string title(exchangePtr->name());
        title.append(" trades");

        SimpleTable table;
        table.reserve(1U + lastTrades.size() + (lastTrades.empty() ? 0U : 2U));
        table.emplace_back(std::move(title), std::move(buyTitle), std::move(priceTitle), std::move(sellTitle));
        std::array<MonetaryAmount, 2> totalAmounts{MonetaryAmount(0, mk.base()), MonetaryAmount(0, mk.base())};
        MonetaryAmount totalPrice(0, mk.quote());
        std::array<int, 2> nb{};
        for (const PublicTrade &trade : lastTrades) {
          if (trade.side() == TradeSide::buy) {
            table.emplace_back(trade.timeStr(), trade.amount().amountStr(), trade.price().amountStr(), "");
            totalAmounts[0] += trade.amount();
            ++nb[0];
          } else {
            table.emplace_back(trade.timeStr(), "", trade.price().amountStr(), trade.amount().amountStr());
            totalAmounts[1] += trade.amount();
            ++nb[1];
          }
          totalPrice += trade.price();
        }
        if (nb[0] + nb[1] > 0) {
          table.emplace_back();
          std::array<string, 2> summary;
          for (int buyOrSell = 0; buyOrSell < 2; ++buyOrSell) {
            summary[buyOrSell].append(totalAmounts[buyOrSell].str());
            summary[buyOrSell].append(" (");
            AppendIntegralToString(summary[buyOrSell], nb[buyOrSell]);
            summary[buyOrSell].push_back(' ');
            summary[buyOrSell].append(buyOrSell == 0 ? "buys" : "sells");
            summary[buyOrSell].push_back(')');
          }

          MonetaryAmount avgPrice = totalPrice / (nb[0] + nb[1]);
          table.emplace_back("Summary", std::move(summary[0]), avgPrice.str(), std::move(summary[1]));
        }

        printTable(table);
      }
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::LastTrades, jsonObj);
}

void QueryResultPrinter::printLastPrice(Market mk, const MonetaryAmountPerExchange &pricePerExchange) const {
  auto jsonObj = LastPriceJson(mk, pricePerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      string headerLastPrice(mk.str());
      headerLastPrice.append(" last price");
      SimpleTable table;
      table.reserve(1U + pricePerExchange.size());
      table.emplace_back("Exchange", std::move(headerLastPrice));
      for (const auto &[exchange, lastPrice] : pricePerExchange) {
        table.emplace_back(exchange->name(), lastPrice.str());
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::LastPrice, jsonObj);
}

void QueryResultPrinter::printWithdraw(const DeliveredWithdrawInfoWithExchanges &deliveredWithdrawInfoWithExchanges,
                                       bool isPercentageWithdraw, const WithdrawOptions &withdrawOptions) const {
  const DeliveredWithdrawInfo &deliveredWithdrawInfo = deliveredWithdrawInfoWithExchanges.second;
  MonetaryAmount grossAmount = deliveredWithdrawInfo.grossAmount();
  const Exchange &fromExchange = *deliveredWithdrawInfoWithExchanges.first.front();
  const Exchange &toExchange = *deliveredWithdrawInfoWithExchanges.first.back();
  auto jsonObj =
      WithdrawJson(deliveredWithdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange, withdrawOptions);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.reserve(3U);

      table.emplace_back("Exchange", "Account", "Sent -> Received amount", "Sent -> Received time",
                         "Withdrawal -> Deposit id");

      table.emplace_back(fromExchange.name(), fromExchange.keyName(), grossAmount.str(),
                         TimeToString(deliveredWithdrawInfo.initiatedTime()), deliveredWithdrawInfo.withdrawId());

      table.emplace_back(toExchange.name(), toExchange.keyName(), deliveredWithdrawInfo.receivedAmount().str(),
                         TimeToString(deliveredWithdrawInfo.receivedTime()), deliveredWithdrawInfo.depositId());
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::Withdraw, jsonObj, withdrawOptions.mode() == WithdrawOptions::Mode::kSimulation);
}

void QueryResultPrinter::printDustSweeper(
    const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
    CurrencyCode currencyCode) const {
  auto jsonObj = DustSweeperJson(tradedAmountsVectorWithFinalAmountPerExchange, currencyCode);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.reserve(1U + tradedAmountsVectorWithFinalAmountPerExchange.size());

      table.emplace_back("Exchange", "Account", "Trades", "Final Amount");

      for (const auto &[exchangePtr, tradedAmountsVectorWithFinalAmount] :
           tradedAmountsVectorWithFinalAmountPerExchange) {
        table::Cell tradesCell;
        const auto &tradedAmountsVector = tradedAmountsVectorWithFinalAmount.tradedAmountsVector;
        tradesCell.reserve(tradedAmountsVector.size());
        for (const auto &tradedAmounts : tradedAmountsVector) {
          tradesCell.emplace_back(tradedAmounts.str());
        }
        table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), std::move(tradesCell),
                           tradedAmountsVectorWithFinalAmount.finalAmount.str());
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::DustSweeper, jsonObj);
}

void QueryResultPrinter::printMarketsForReplay(TimeWindow timeWindow,
                                               const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange) {
  auto jsonObj = MarketsForReplayJson(timeWindow, marketTimestampSetsPerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      MarketSet allMarkets = ComputeAllMarkets(marketTimestampSetsPerExchange);

      SimpleTable table;
      table.reserve(allMarkets.size() + 1U);
      table.emplace_back("Markets", "Last order books timestamp", "Last trades timestamp");

      for (const Market market : allMarkets) {
        table::Cell orderBookCell;
        table::Cell tradesCell;
        for (const auto &[exchange, marketTimestamps] : marketTimestampSetsPerExchange) {
          const auto &orderBooksMarkets = marketTimestamps.orderBooksMarkets;
          const auto &tradesMarkets = marketTimestamps.tradesMarkets;
          const auto marketPartitionPred = [market](const auto &marketTimestamp) {
            return marketTimestamp.market < market;
          };
          const auto orderBooksIt = std::ranges::partition_point(orderBooksMarkets, marketPartitionPred);
          const auto tradesIt = std::ranges::partition_point(tradesMarkets, marketPartitionPred);

          if (orderBooksIt != orderBooksMarkets.end() && orderBooksIt->market == market) {
            string str = TimeToString(orderBooksIt->timePoint);
            str.append(" @ ");
            str.append(exchange->name());

            orderBookCell.emplace_back(std::move(str));
          }

          if (tradesIt != tradesMarkets.end() && tradesIt->market == market) {
            string str = TimeToString(tradesIt->timePoint);
            str.append(" @ ");
            str.append(exchange->name());

            tradesCell.emplace_back(std::move(str));
          }
        }

        table.emplace_back(market.str(), std::move(orderBookCell), std::move(tradesCell));
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(CoincenterCommandType::ReplayMarkets, jsonObj);
}

void QueryResultPrinter::printMarketTradingResults(TimeWindow inputTimeWindow, const ReplayResults &replayResults,
                                                   CoincenterCommandType commandType) const {
  auto jsonObj = MarketTradingResultsJson(inputTimeWindow, replayResults, commandType);
  switch (_apiOutputType) {
    case ApiOutputType::table: {
      SimpleTable table;
      table.emplace_back("Algorithm", "Exchange", "Time window", "Market", "Start amounts", "Profit / Loss",
                         "Matched orders", "Stats");
      for (const auto &[algorithmName, marketTradingResultPerExchangeVector] : replayResults) {
        for (const auto &marketTradingResultPerExchange : marketTradingResultPerExchangeVector) {
          for (const auto &[exchangePtr, marketGlobalTradingResults] : marketTradingResultPerExchange) {
            const auto &marketTradingResults = marketGlobalTradingResults.result;
            const auto &stats = marketGlobalTradingResults.stats;

            table::Cell trades;
            for (const ClosedOrder &closedOrder : marketTradingResults.matchedOrders()) {
              string orderStr = TimeToString(closedOrder.placedTime());
              orderStr.append(" - ");
              orderStr.append(EnumToString(closedOrder.side()));
              orderStr.append(" - ");
              orderStr.append(closedOrder.matchedVolume().str());
              orderStr.append(" @ ");
              orderStr.append(closedOrder.price().str());
              trades.emplace_back(std::move(orderStr));
            }

            string orderBookStats("order books: ");
            orderBookStats.append(std::string_view(IntegralToCharVector(stats.marketOrderBookStats.nbSuccessful)));
            orderBookStats.append(" OK");
            if (stats.marketOrderBookStats.nbError != 0) {
              orderBookStats.append(", ");
              orderBookStats.append(std::string_view(IntegralToCharVector(stats.marketOrderBookStats.nbError)));
              orderBookStats.append(" KO");
            }

            string tradesStats("trades: ");
            tradesStats.append(std::string_view(IntegralToCharVector(stats.publicTradeStats.nbSuccessful)));
            tradesStats.append(" OK");
            if (stats.publicTradeStats.nbError != 0) {
              tradesStats.append(", ");
              tradesStats.append(std::string_view(IntegralToCharVector(stats.publicTradeStats.nbError)));
              tradesStats.append(" KO");
            }

            const TimeWindow marketTimeWindow = stats.marketOrderBookStats.timeWindow;

            table.emplace_back(marketTradingResults.algorithmName(), exchangePtr->name(),
                               table::Cell{TimeToString(marketTimeWindow.from()), TimeToString(marketTimeWindow.to())},
                               marketTradingResults.market().str(),
                               table::Cell{marketTradingResults.startBaseAmount().str(),
                                           marketTradingResults.startQuoteAmount().str()},
                               marketTradingResults.quoteAmountDelta().str(), std::move(trades),
                               table::Cell{std::move(orderBookStats), std::move(tradesStats)});
          }
        }
      }
      printTable(table);
      break;
    }
    case ApiOutputType::json:
      printJson(jsonObj);
      break;
    case ApiOutputType::off:
      break;
  }
  logActivity(commandType, jsonObj);
}

void QueryResultPrinter::printTable(const SimpleTable &table) const {
  std::ostringstream ss;
  std::ostream &os = _pOs != nullptr ? *_pOs : ss;

  os << table;

  if (_pOs != nullptr) {
    *_pOs << '\n';
  } else {
    // logger library automatically adds a newline as suffix
    _outputLogger->info(ss.view());
  }
}

}  // namespace cct
