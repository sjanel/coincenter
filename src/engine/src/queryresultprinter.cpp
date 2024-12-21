#include "queryresultprinter.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <string_view>
#include <utility>

#include "apioutputtype.hpp"
#include "balanceperexchangeportfolio.hpp"
#include "cct_const.hpp"
#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "closed-order.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "currencycodevector.hpp"
#include "currencyexchange.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "durationstring.hpp"
#include "exchange.hpp"
#include "exchangepublicapitypes.hpp"
#include "file.hpp"
#include "logginginfo.hpp"
#include "market-timestamp.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
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
#include "tradedefinitions.hpp"
#include "tradeside.hpp"
#include "unreachable.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawsconstraints.hpp"
#include "withdrawsordepositsconstraints.hpp"
#include "writer.hpp"

namespace cct {
namespace {

json::container ToJson(CoincenterCommandType commandType, json::container &&in, json::container &&out) {
  in.emplace("req", CoincenterCommandTypeToString(commandType));

  json::container ret;

  ret.emplace("in", std::move(in));
  ret.emplace("out", std::move(out));

  return ret;
}

auto HealthCheckJson(const ExchangeHealthCheckStatus &healthCheckPerExchange) {
  schema::queryresult::HealthCheck obj;

  obj.out.reserve(healthCheckPerExchange.size());
  for (const auto &[e, healthCheckValue] : healthCheckPerExchange) {
    obj.out.emplace_back(e->exchangeNameEnum(), healthCheckValue);
  }

  return obj;
}

auto CurrenciesJson(const CurrenciesPerExchange &currenciesPerExchange) {
  schema::queryresult::CurrenciesPerExchange obj;

  for (const auto &[e, currencies] : currenciesPerExchange) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &p = obj.out.emplace_back(e->exchangeNameEnum(), ExchangePartType{});

    p.second.reserve(currencies.size());
    for (const CurrencyExchange &cur : currencies) {
      auto &currency = p.second.emplace_back();

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
  for (const auto &[e, markets] : marketsPerExchange) {
    obj.out.emplace_back(e->exchangeNameEnum(), markets);
  }

  return obj;
}

auto MarketsForReplayJson(TimeWindow timeWindow, const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange) {
  schema::queryresult::MarketsForReplay obj;

  if (timeWindow != TimeWindow{}) {
    obj.in.opt.timeWindow = timeWindow;
  }

  for (const auto &[e, marketTimestampSets] : marketTimestampSetsPerExchange) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &exchangePart = obj.out.emplace_back(e->exchangeNameEnum(), ExchangePartType{}).second;

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
  for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
    using ExchangePartType = decltype(obj.out)::value_type::second_type;
    auto &exchangePart = obj.out.emplace_back(e->exchangeNameEnum(), ExchangePartType{}).second;

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

  for (const auto &[e, balance] : balancePerExchange) {
    size_t publicExchangeIdx;
    auto it = std::ranges::find_if(
        obj.out.exchange, [&e](const auto &exchangePart) { return exchangePart.first == e->exchangeNameEnum(); });
    if (it == obj.out.exchange.end()) {
      obj.out.exchange.emplace_back(e->exchangeNameEnum(), ExchangePart{});
      publicExchangeIdx = obj.out.exchange.size() - 1;
    } else {
      publicExchangeIdx = std::distance(obj.out.exchange.begin(), it);
    }

    auto &exchangePart = obj.out.exchange[publicExchangeIdx].second;
    auto &exchangeKeyPart = exchangePart.emplace_back(e->keyName(), ExchangeKeyPart{}).second;

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
  json::container in;
  json::container inOpt;
  inOpt.emplace("cur", depositCurrencyCode.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[exchangePtr, wallet] : walletPerExchange) {
    json::container depositPerExchangeData;

    depositPerExchangeData.emplace("address", wallet.address());
    if (wallet.hasTag()) {
      depositPerExchangeData.emplace("tag", wallet.tag());
    }

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container depositInfoForExchangeUser;
      depositInfoForExchangeUser.emplace(exchangePtr->keyName(), std::move(depositPerExchangeData));
      out.emplace(exchangePtr->name(), std::move(depositInfoForExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(depositPerExchangeData));
    }
  }
  return ToJson(CoincenterCommandType::DepositInfo, std::move(in), std::move(out));
}

inline const char *TradeModeToStr(TradeMode tradeMode) { return tradeMode == TradeMode::kReal ? "real" : "simulation"; }

auto TradeOptionsToJson(const TradeOptions &tradeOptions) {
  json::container priceOptionsJson;
  const PriceOptions &priceOptions = tradeOptions.priceOptions();
  priceOptionsJson.emplace("strategy", PriceStrategyStr(priceOptions.priceStrategy(), false));
  if (priceOptions.isFixedPrice()) {
    priceOptionsJson.emplace("fixedPrice", priceOptions.fixedPrice().str());
  }
  if (priceOptions.isRelativePrice()) {
    priceOptionsJson.emplace("relativePrice", priceOptions.relativePrice());
  }
  json::container ret;
  ret.emplace("price", std::move(priceOptionsJson));
  ret.emplace("maxTradeTime", DurationToString(tradeOptions.maxTradeTime()));
  ret.emplace("minTimeBetweenPriceUpdates", DurationToString(tradeOptions.minTimeBetweenPriceUpdates()));
  ret.emplace("mode", TradeModeToStr(tradeOptions.tradeMode()));
  ret.emplace("timeoutAction", tradeOptions.timeoutActionStr());
  ret.emplace("syncPolicy", tradeOptions.tradeSyncPolicyStr());
  return ret;
}

auto TradesJson(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount amount, bool isPercentageTrade,
                CurrencyCode toCurrency, const TradeOptions &tradeOptions, CoincenterCommandType commandType) {
  json::container in;
  json::container fromJson;
  fromJson.emplace("amount", amount.amountStr());
  fromJson.emplace("currency", amount.currencyStr());
  fromJson.emplace("isPercentage", isPercentageTrade);

  json::container inOpt;
  switch (commandType) {
    case CoincenterCommandType::Buy:
      inOpt.emplace("to", std::move(fromJson));
      break;
    case CoincenterCommandType::Sell:
      inOpt.emplace("from", std::move(fromJson));
      break;
    case CoincenterCommandType::Trade: {
      json::container toJson;
      toJson.emplace("currency", toCurrency.str());

      inOpt.emplace("from", std::move(fromJson));
      inOpt.emplace("to", std::move(toJson));
      break;
    }
    default:
      unreachable();
  }

  inOpt.emplace("options", TradeOptionsToJson(tradeOptions));
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[exchangePtr, tradeResult] : tradeResultPerExchange) {
    json::container tradedAmountPerExchangeJson;
    tradedAmountPerExchangeJson.emplace("from", tradeResult.from().amountStr());
    tradedAmountPerExchangeJson.emplace("status", tradeResult.stateStr());
    tradedAmountPerExchangeJson.emplace("tradedFrom", tradeResult.tradedAmounts().from.amountStr());
    tradedAmountPerExchangeJson.emplace("tradedTo", tradeResult.tradedAmounts().to.amountStr());

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container tradedAmountPerExchangeUser;
      tradedAmountPerExchangeUser.emplace(exchangePtr->keyName(), std::move(tradedAmountPerExchangeJson));
      out.emplace(exchangePtr->name(), std::move(tradedAmountPerExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(tradedAmountPerExchangeJson));
    }
  }

  return ToJson(commandType, std::move(in), std::move(out));
}

auto OrdersConstraintsToJson(const OrdersConstraints &ordersConstraints) {
  json::container ret;
  if (ordersConstraints.isCurDefined()) {
    ret.emplace("cur1", ordersConstraints.curStr1());
  }
  if (ordersConstraints.isCur2Defined()) {
    ret.emplace("cur2", ordersConstraints.curStr2());
  }
  if (ordersConstraints.isPlacedTimeBeforeDefined()) {
    ret.emplace("placedBefore", TimeToString(ordersConstraints.placedBefore()));
  }
  if (ordersConstraints.isPlacedTimeAfterDefined()) {
    ret.emplace("placedAfter", TimeToString(ordersConstraints.placedAfter()));
  }
  if (ordersConstraints.isOrderIdDefined()) {
    json::container orderIds = json::container::array();
    for (const OrderId &orderId : ordersConstraints.orderIdSet()) {
      orderIds.emplace_back(orderId);
    }
    ret.emplace("matchIds", std::move(orderIds));
  }
  return ret;
}

template <class OrderType>
auto OrderJson(const OrderType &orderData) {
  static_assert(std::is_same_v<ClosedOrder, OrderType> || std::is_same_v<OpenedOrder, OrderType>);

  json::container order;
  order.emplace("id", orderData.id());
  order.emplace("pair", orderData.market().str());
  order.emplace("placedTime", orderData.placedTimeStr());
  if constexpr (std::is_same_v<ClosedOrder, OrderType>) {
    order.emplace("matchedTime", orderData.matchedTimeStr());
  }
  order.emplace("side", orderData.sideStr());
  order.emplace("price", orderData.price().amountStr());
  order.emplace("matched", orderData.matchedVolume().amountStr());
  if constexpr (std::is_same_v<OpenedOrder, OrderType>) {
    order.emplace("remaining", orderData.remainingVolume().amountStr());
  }
  return order;
}

template <class OrdersPerExchangeType>
auto OrdersJson(CoincenterCommandType coincenterCommandType, const OrdersPerExchangeType &ordersPerExchange,
                const OrdersConstraints &ordersConstraints) {
  json::container in;
  json::container inOpt = OrdersConstraintsToJson(ordersConstraints);

  if (!inOpt.empty()) {
    in.emplace("opt", std::move(inOpt));
  }

  json::container out = json::container::object();
  for (const auto &[exchangePtr, ordersData] : ordersPerExchange) {
    json::container orders = json::container::array();
    for (const auto &orderData : ordersData) {
      orders.emplace_back(OrderJson(orderData));
    }

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container ordersPerExchangeUser;
      ordersPerExchangeUser.emplace(exchangePtr->keyName(), std::move(orders));
      out.emplace(exchangePtr->name(), std::move(ordersPerExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(orders));
    }
  }

  return ToJson(coincenterCommandType, std::move(in), std::move(out));
}

auto OrdersCancelledJson(const NbCancelledOrdersPerExchange &nbCancelledOrdersPerExchange,
                         const OrdersConstraints &ordersConstraints) {
  json::container in;
  json::container inOpt = OrdersConstraintsToJson(ordersConstraints);

  if (!inOpt.empty()) {
    in.emplace("opt", std::move(inOpt));
  }

  json::container out = json::container::object();
  for (const auto &[exchangePtr, nbCancelledOrders] : nbCancelledOrdersPerExchange) {
    json::container cancelledOrdersForAccount;
    cancelledOrdersForAccount.emplace("nb", nbCancelledOrders);

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container cancelledOrdersForExchangeUser;
      cancelledOrdersForExchangeUser.emplace(exchangePtr->keyName(), std::move(cancelledOrdersForAccount));
      out.emplace(exchangePtr->name(), std::move(cancelledOrdersForExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(cancelledOrdersForAccount));
    }
  }

  return ToJson(CoincenterCommandType::OrdersCancel, std::move(in), std::move(out));
}

enum class DepositOrWithdrawEnum : int8_t { kDeposit, kWithdraw };

auto DepositsConstraintsToJson(const WithdrawsOrDepositsConstraints &constraints,
                               DepositOrWithdrawEnum depositOrWithdraw) {
  json::container ret;
  if (constraints.isCurDefined()) {
    ret.emplace("cur", constraints.currencyCode().str());
  }
  if (constraints.isTimeBeforeDefined()) {
    ret.emplace(depositOrWithdraw == DepositOrWithdrawEnum::kDeposit ? "receivedBefore" : "sentBefore",
                TimeToString(constraints.timeBefore()));
  }
  if (constraints.isTimeAfterDefined()) {
    ret.emplace(depositOrWithdraw == DepositOrWithdrawEnum::kDeposit ? "receivedAfter" : "sentAfter",
                TimeToString(constraints.timeAfter()));
  }
  if (constraints.isIdDefined()) {
    json::container depositIds = json::container::array();
    for (const string &depositId : constraints.idSet()) {
      depositIds.emplace_back(depositId);
    }
    ret.emplace("matchIds", std::move(depositIds));
  }
  return ret;
}

auto RecentDepositsJson(const DepositsPerExchange &depositsPerExchange,
                        const DepositsConstraints &depositsConstraints) {
  json::container in;
  json::container inOpt = DepositsConstraintsToJson(depositsConstraints, DepositOrWithdrawEnum::kDeposit);

  if (!inOpt.empty()) {
    in.emplace("opt", std::move(inOpt));
  }

  json::container out = json::container::object();
  for (const auto &[exchangePtr, deposits] : depositsPerExchange) {
    json::container depositsJson = json::container::array();
    for (const Deposit &deposit : deposits) {
      json::container &depositJson = depositsJson.emplace_back();
      depositJson.emplace("id", deposit.id());
      depositJson.emplace("cur", deposit.amount().currencyStr());
      depositJson.emplace("receivedTime", deposit.timeStr());
      depositJson.emplace("amount", deposit.amount().amountStr());
      depositJson.emplace("status", deposit.statusStr());
    }

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container depositsPerExchangeUser;
      depositsPerExchangeUser.emplace(exchangePtr->keyName(), std::move(depositsJson));
      out.emplace(exchangePtr->name(), std::move(depositsPerExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(depositsJson));
    }
  }

  return ToJson(CoincenterCommandType::RecentDeposits, std::move(in), std::move(out));
}

auto RecentWithdrawsJson(const WithdrawsPerExchange &withdrawsPerExchange,
                         const WithdrawsConstraints &withdrawsConstraints) {
  json::container in;
  json::container inOpt = DepositsConstraintsToJson(withdrawsConstraints, DepositOrWithdrawEnum::kWithdraw);

  if (!inOpt.empty()) {
    in.emplace("opt", std::move(inOpt));
  }

  json::container out = json::container::object();
  for (const auto &[exchangePtr, withdraws] : withdrawsPerExchange) {
    json::container withdrawsJson = json::container::array();
    for (const Withdraw &withdraw : withdraws) {
      json::container &withdrawJson = withdrawsJson.emplace_back();
      withdrawJson.emplace("id", withdraw.id());
      withdrawJson.emplace("cur", withdraw.amount().currencyStr());
      withdrawJson.emplace("sentTime", withdraw.timeStr());
      withdrawJson.emplace("netEmittedAmount", withdraw.amount().amountStr());
      withdrawJson.emplace("fee", withdraw.withdrawFee().amountStr());
      withdrawJson.emplace("status", withdraw.statusStr());
    }

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container withdrawsPerExchangeUser;
      withdrawsPerExchangeUser.emplace(exchangePtr->keyName(), std::move(withdrawsJson));
      out.emplace(exchangePtr->name(), std::move(withdrawsPerExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(withdrawsJson));
    }
  }

  return ToJson(CoincenterCommandType::RecentWithdraws, std::move(in), std::move(out));
}

auto ConversionJson(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                    const MonetaryAmountPerExchange &conversionPerExchange) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("amount", amount.str());
  inOpt.emplace("targetCurrency", targetCurrencyCode.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[e, convertedAmount] : conversionPerExchange) {
    if (convertedAmount != 0) {
      json::container conversionForExchange;
      conversionForExchange.emplace("convertedAmount", convertedAmount.str());
      out.emplace(e->name(), std::move(conversionForExchange));
    }
  }

  return ToJson(CoincenterCommandType::Conversion, std::move(in), std::move(out));
}

auto ConversionJson(std::span<const MonetaryAmount> startAmountPerExchangePos, CurrencyCode targetCurrencyCode,
                    const MonetaryAmountPerExchange &conversionPerExchange) {
  json::container in;
  json::container inOpt;

  json::container fromAmounts;

  int publicExchangePos{};
  for (MonetaryAmount startAmount : startAmountPerExchangePos) {
    if (!startAmount.isDefault()) {
      fromAmounts.emplace(kSupportedExchanges[publicExchangePos], startAmount.str());
    }
    ++publicExchangePos;
  }
  inOpt.emplace("sourceAmount", std::move(fromAmounts));

  inOpt.emplace("targetCurrency", targetCurrencyCode.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[e, convertedAmount] : conversionPerExchange) {
    if (convertedAmount != 0) {
      json::container conversionForExchange;
      conversionForExchange.emplace("convertedAmount", convertedAmount.str());
      out.emplace(e->name(), std::move(conversionForExchange));
    }
  }

  return ToJson(CoincenterCommandType::Conversion, std::move(in), std::move(out));
}

auto ConversionPathJson(Market mk, const ConversionPathPerExchange &conversionPathsPerExchange) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("market", mk.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[e, conversionPath] : conversionPathsPerExchange) {
    if (!conversionPath.empty()) {
      json::container conversionPathForExchange;
      for (Market market : conversionPath) {
        conversionPathForExchange.emplace_back(market.str());
      }
      out.emplace(e->name(), std::move(conversionPathForExchange));
    }
  }

  return ToJson(CoincenterCommandType::ConversionPath, std::move(in), std::move(out));
}

auto WithdrawFeesJson(const MonetaryAmountByCurrencySetPerExchange &withdrawFeePerExchange, CurrencyCode cur) {
  json::container in;
  json::container inOpt = json::container::object();
  if (!cur.isNeutral()) {
    inOpt.emplace("cur", cur.str());
  }
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[e, withdrawFees] : withdrawFeePerExchange) {
    json::container amountsPerExchange = json::container::array();
    for (MonetaryAmount ma : withdrawFees) {
      amountsPerExchange.emplace_back(ma.str());
    }
    out.emplace(e->name(), std::move(amountsPerExchange));
  }

  return ToJson(CoincenterCommandType::WithdrawFees, std::move(in), std::move(out));
}

auto Last24hTradedVolumeJson(Market mk, const MonetaryAmountPerExchange &tradedVolumePerExchange) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("market", mk.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
    out.emplace(e->name(), tradedVolume.amountStr());
  }

  return ToJson(CoincenterCommandType::Last24hTradedVolume, std::move(in), std::move(out));
}

auto LastTradesJson(Market mk, std::optional<int> nbLastTrades, const TradesPerExchange &lastTradesPerExchange) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("market", mk.str());
  if (nbLastTrades) {
    inOpt.emplace("nb", *nbLastTrades);
  }
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
    json::container lastTradesJson = json::container::array();
    for (const PublicTrade &trade : lastTrades) {
      json::container &lastTrade = lastTradesJson.emplace_back();
      lastTrade.emplace("a", trade.amount().amountStr());
      lastTrade.emplace("p", trade.price().amountStr());
      lastTrade.emplace("time", trade.timeStr());
      lastTrade.emplace("side", SideStr(trade.side()));
    }
    out.emplace(exchangePtr->name(), std::move(lastTradesJson));
  }

  return ToJson(CoincenterCommandType::LastTrades, std::move(in), std::move(out));
}

auto LastPriceJson(Market mk, const MonetaryAmountPerExchange &pricePerExchange) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("market", mk.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[e, lastPrice] : pricePerExchange) {
    out.emplace(e->name(), lastPrice.amountStr());
  }

  return ToJson(CoincenterCommandType::LastPrice, std::move(in), std::move(out));
}

auto WithdrawJson(const DeliveredWithdrawInfo &deliveredWithdrawInfo, MonetaryAmount grossAmount,
                  bool isPercentageWithdraw, const Exchange &fromExchange, const Exchange &toExchange,
                  const WithdrawOptions &withdrawOptions) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("cur", grossAmount.currencyStr());
  inOpt.emplace("isPercentage", isPercentageWithdraw);
  inOpt.emplace("syncPolicy", withdrawOptions.withdrawSyncPolicyStr());
  in.emplace("opt", std::move(inOpt));

  json::container from;
  from.emplace("exchange", fromExchange.name());
  from.emplace("account", fromExchange.keyName());
  from.emplace("id", deliveredWithdrawInfo.withdrawId());
  from.emplace("amount", grossAmount.amountStr());
  from.emplace("time", TimeToString(deliveredWithdrawInfo.initiatedTime()));

  json::container to;
  to.emplace("exchange", toExchange.name());
  to.emplace("account", toExchange.keyName());
  to.emplace("id", deliveredWithdrawInfo.depositId());
  to.emplace("amount", deliveredWithdrawInfo.receivedAmount().amountStr());
  to.emplace("address", deliveredWithdrawInfo.receivingWallet().address());
  if (deliveredWithdrawInfo.receivingWallet().hasTag()) {
    to.emplace("tag", deliveredWithdrawInfo.receivingWallet().tag());
  }
  to.emplace("time", TimeToString(deliveredWithdrawInfo.receivedTime()));

  json::container out;
  out.emplace("from", std::move(from));
  out.emplace("to", std::move(to));

  return ToJson(CoincenterCommandType::Withdraw, std::move(in), std::move(out));
}

auto DustSweeperJson(const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
                     CurrencyCode currencyCode) {
  json::container in;
  json::container inOpt;
  inOpt.emplace("cur", currencyCode.str());
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();
  for (const auto &[exchangePtr, tradedAmountsVectorWithFinalAmount] : tradedAmountsVectorWithFinalAmountPerExchange) {
    json::container tradedAmountsArray = json::container::array_t();
    for (const auto &tradedAmounts : tradedAmountsVectorWithFinalAmount.tradedAmountsVector) {
      json::container tradedAmountsData;
      tradedAmountsData.emplace("from", tradedAmounts.from.str());
      tradedAmountsData.emplace("to", tradedAmounts.to.str());
      tradedAmountsArray.push_back(std::move(tradedAmountsData));
    }

    json::container tradedInfoPerExchangeData;
    tradedInfoPerExchangeData.emplace("trades", std::move(tradedAmountsArray));
    tradedInfoPerExchangeData.emplace("finalAmount", tradedAmountsVectorWithFinalAmount.finalAmount.str());

    auto it = out.find(exchangePtr->name());
    if (it == out.end()) {
      json::container dataForExchangeUser;
      dataForExchangeUser.emplace(exchangePtr->keyName(), std::move(tradedInfoPerExchangeData));
      out.emplace(exchangePtr->name(), std::move(dataForExchangeUser));
    } else {
      it->emplace(exchangePtr->keyName(), std::move(tradedInfoPerExchangeData));
    }
  }

  return ToJson(CoincenterCommandType::DustSweeper, std::move(in), std::move(out));
}

auto MarketTradingResultsJson(TimeWindow inputTimeWindow, const ReplayResults &replayResults,
                              CoincenterCommandType commandType) {
  json::container inOpt;

  const auto createTimeStats = [](TimeWindow timeWindow) {
    json::container ret;
    ret.emplace("from", TimeToString(timeWindow.from()));
    ret.emplace("to", TimeToString(timeWindow.to()));
    return ret;
  };

  inOpt.emplace("time", createTimeStats(inputTimeWindow));

  json::container in;
  in.emplace("opt", std::move(inOpt));

  json::container out = json::container::object();

  for (const auto &[algorithmName, marketTradingResultPerExchangeVector] : replayResults) {
    json::container algorithmNameResults = json::container::array_t();
    for (const auto &marketTradingResultPerExchange : marketTradingResultPerExchangeVector) {
      json::container allResults = json::container::array_t();
      for (const auto &[exchangePtr, marketGlobalTradingResult] : marketTradingResultPerExchange) {
        const auto &marketTradingResult = marketGlobalTradingResult.result;
        const auto &stats = marketGlobalTradingResult.stats;

        const auto computeTradeRangeResultsStats =
            [&createTimeStats](const TradeRangeResultsStats &tradeRangeResultsStats) {
              json::container ret;
              ret.emplace("nb-successful", tradeRangeResultsStats.nbSuccessful);
              ret.emplace("nb-error", tradeRangeResultsStats.nbError);

              ret.emplace("time", createTimeStats(tradeRangeResultsStats.timeWindow));
              return ret;
            };

        json::container startAmounts;
        startAmounts.emplace("base", marketTradingResult.startBaseAmount().str());
        startAmounts.emplace("quote", marketTradingResult.startQuoteAmount().str());

        json::container orderBookStats = computeTradeRangeResultsStats(stats.marketOrderBookStats);

        json::container tradeStats = computeTradeRangeResultsStats(stats.publicTradeStats);

        json::container jsonStats;
        jsonStats.emplace("order-books", std::move(orderBookStats));
        jsonStats.emplace("trades", std::move(tradeStats));

        json::container marketTradingResultJson;
        marketTradingResultJson.emplace("algorithm", marketTradingResult.algorithmName());
        marketTradingResultJson.emplace("market", marketTradingResult.market().str());
        marketTradingResultJson.emplace("start-amounts", std::move(startAmounts));
        marketTradingResultJson.emplace("profit-and-loss", marketTradingResult.quoteAmountDelta().str());
        marketTradingResultJson.emplace("stats", std::move(jsonStats));

        json::container closedOrdersArray = json::container::array_t();

        for (const ClosedOrder &closedOrder : marketTradingResult.matchedOrders()) {
          closedOrdersArray.push_back(OrderJson(closedOrder));
        }

        marketTradingResultJson.emplace("matched-orders", std::move(closedOrdersArray));

        json::container exchangeMarketResults;
        exchangeMarketResults.emplace(exchangePtr->name(), std::move(marketTradingResultJson));

        allResults.push_back(std::move(exchangeMarketResults));
      }
      algorithmNameResults.push_back(std::move(allResults));
    }

    out.emplace(algorithmName, std::move(algorithmNameResults));
  }

  return ToJson(commandType, std::move(in), std::move(out));
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
      for (const auto &[e, healthCheckValue] : healthCheckPerExchange) {
        table.emplace_back(e->name(), healthCheckValue ? "OK" : "Not OK!");
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
      for (const auto &[e, markets] : marketsPerExchange) {
        for (Market mk : markets) {
          table.emplace_back(e->name(), mk.str());
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
      for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
        for (const auto &[mk, marketOrderBook] : marketOrderBookMap) {
          table.emplace_back(e->name(), mk.str(), marketOrderBook.highestBidPrice().str(),
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
      tradedFromStr.append(TradeModeToStr(tradeOptions.tradeMode()));
      tradedFromStr.push_back(')');
      string tradedToStr("Traded to amount (");
      tradedToStr.append(TradeModeToStr(tradeOptions.tradeMode()));
      tradedToStr.push_back(')');
      SimpleTable table;

      table.reserve(1U + tradeResultPerExchange.size());
      table.emplace_back("Exchange", "Account", "From", std::move(tradedFromStr), std::move(tradedToStr), "Status");

      for (const auto &[exchangePtr, tradeResult] : tradeResultPerExchange) {
        const TradedAmounts &tradedAmounts = tradeResult.tradedAmounts();

        table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), tradeResult.from().str(),
                           tradedAmounts.from.str(), tradedAmounts.to.str(), tradeResult.stateStr());
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
          table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), closedOrder.id(), closedOrder.placedTimeStr(),
                             closedOrder.matchedTimeStr(), closedOrder.sideStr(), closedOrder.price().str(),
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
          table.emplace_back(exchangePtr->name(), exchangePtr->keyName(), openedOrder.id(), openedOrder.placedTimeStr(),
                             openedOrder.sideStr(), openedOrder.price().str(), openedOrder.matchedVolume().str(),
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
      for (const auto &[e, convertedAmount] : conversionPerExchange) {
        if (convertedAmount != 0) {
          table.emplace_back(e->name(), convertedAmount.str());
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
      for (const auto &[e, convertedAmount] : conversionPerExchange) {
        if (convertedAmount != 0) {
          table.emplace_back(e->name(), startAmountPerExchangePos[e->publicExchangePos()].str(), convertedAmount.str());
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
      for (const auto &[e, conversionPath] : conversionPathsPerExchange) {
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
        table.emplace_back(e->name(), std::move(conversionPathStr));
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
      for (const auto &[e, withdrawFees] : withdrawFeesPerExchange) {
        header.emplace_back(e->name());
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
        for (const auto &[e, withdrawFees] : withdrawFeesPerExchange) {
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
      for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
        table.emplace_back(e->name(), tradedVolume.str());
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
          if (trade.side() == TradeSide::kBuy) {
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
      for (const auto &[e, lastPrice] : pricePerExchange) {
        table.emplace_back(e->name(), lastPrice.str());
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
        for (const auto &[e, marketTimestamps] : marketTimestampSetsPerExchange) {
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
            str.append(e->name());

            orderBookCell.emplace_back(std::move(str));
          }

          if (tradesIt != tradesMarkets.end() && tradesIt->market == market) {
            string str = TimeToString(tradesIt->timePoint);
            str.append(" @ ");
            str.append(e->name());

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
              string orderStr = closedOrder.placedTimeStr();
              orderStr.append(" - ");
              orderStr.append(closedOrder.sideStr());
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
