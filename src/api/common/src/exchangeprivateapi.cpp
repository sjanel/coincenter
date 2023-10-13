#include "exchangeprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

#include "apikey.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "cct_log.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangebase.hpp"
#include "exchangeinfo.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "orderid.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "recentdeposit.hpp"
#include "timedef.hpp"
#include "tradedamounts.hpp"
#include "tradedefinitions.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "unreachable.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

ExchangePrivate::ExchangePrivate(const CoincenterInfo &coincenterInfo, ExchangePublic &exchangePublic,
                                 const APIKey &apiKey)
    : _exchangePublic(exchangePublic), _coincenterInfo(coincenterInfo), _apiKey(apiKey) {}

BalancePortfolio ExchangePrivate::getAccountBalance(const BalanceOptions &balanceOptions) {
  UniqueQueryHandle uniqueQueryHandle(_cachedResultVault);
  BalancePortfolio balancePortfolio = queryAccountBalance(balanceOptions);
  log::info("Retrieved {} balance for {} assets", exchangeName(), balancePortfolio.size());
  return balancePortfolio;
}

void ExchangePrivate::addBalance(BalancePortfolio &balancePortfolio, MonetaryAmount amount, CurrencyCode equiCurrency) {
  if (amount != 0) {
    ExchangeName exchangeName = this->exchangeName();
    if (equiCurrency.isNeutral()) {
      log::debug("{} Balance {}", exchangeName, amount);
      balancePortfolio.add(amount);
    } else {
      std::optional<MonetaryAmount> optConvertedAmountEquiCurrency = _exchangePublic.convert(amount, equiCurrency);
      MonetaryAmount equivalentInMainCurrency;
      if (optConvertedAmountEquiCurrency) {
        equivalentInMainCurrency = *optConvertedAmountEquiCurrency;
      } else {
        log::warn("Cannot convert {} into {} on {}", amount.currencyStr(), equiCurrency, exchangeName);
        equivalentInMainCurrency = MonetaryAmount(0, equiCurrency);
      }
      log::debug("{} Balance {} (eq. {})", exchangeName, amount, equivalentInMainCurrency);
      balancePortfolio.add(amount, equivalentInMainCurrency);
    }
  }
}

TradedAmounts ExchangePrivate::trade(MonetaryAmount from, CurrencyCode toCurrency, const TradeOptions &options,
                                     const MarketsPath &conversionPath) {
  const bool realOrderPlacedInSimulationMode = !isSimulatedOrderSupported() && exchangeInfo().placeSimulateRealOrder();
  const int nbTrades = static_cast<int>(conversionPath.size());
  const bool isMultiTradeAllowed = options.isMultiTradeAllowed(exchangeInfo().multiTradeAllowedByDefault());

  ExchangeName exchangeName = this->exchangeName();
  log::info("{}rade {} -> {} on {} requested", isMultiTradeAllowed && nbTrades > 1 ? "Multi t" : "T", from, toCurrency,
            exchangeName);
  log::debug(options.str(realOrderPlacedInSimulationMode));

  TradedAmounts tradedAmounts(from.currencyCode(), toCurrency);
  if (conversionPath.empty()) {
    log::warn("Cannot trade {} into {} on {}", from, toCurrency, exchangeName);
    return tradedAmounts;
  }
  if (nbTrades > 1 && !isMultiTradeAllowed) {
    log::error("Can only convert {} to {} in {} steps, but multi trade is not allowed, aborting", from, toCurrency,
               nbTrades);
    return tradedAmounts;
  }
  MonetaryAmount avAmount = from;
  for (int tradePos = 0; tradePos < nbTrades; ++tradePos) {
    Market mk = conversionPath[tradePos];
    log::info("Step {}/{} - trade {} into {}", tradePos + 1, nbTrades, avAmount, mk.opposite(avAmount.currencyCode()));
    TradedAmounts stepTradedAmounts = marketTrade(avAmount, options, mk);
    avAmount = stepTradedAmounts.tradedTo;
    if (avAmount == 0) {
      break;
    }
    if (tradePos == 0) {
      tradedAmounts.tradedFrom = stepTradedAmounts.tradedFrom;
    }
    if (tradePos + 1 == nbTrades) {
      tradedAmounts.tradedTo = stepTradedAmounts.tradedTo;
    }
  }
  return tradedAmounts;
}

TradedAmounts ExchangePrivate::marketTrade(MonetaryAmount from, const TradeOptions &tradeOptions, Market mk) {
  const CurrencyCode fromCurrency = from.currencyCode();
  const CurrencyCode toCurrency = mk.opposite(fromCurrency);

  const TimePoint timerStart = Clock::now();
  const UserRefInt userRef =
      static_cast<UserRefInt>(TimestampToS(timerStart) % static_cast<int64_t>(std::numeric_limits<UserRefInt>::max()));

  const TradeSide side = fromCurrency == mk.base() ? TradeSide::kSell : TradeSide::kBuy;
  TradeContext tradeContext(mk, side, userRef);
  TradeInfo tradeInfo(tradeContext, tradeOptions);
  TradeOptions &options = tradeInfo.options;
  const bool placeSimulatedRealOrder = exchangeInfo().placeSimulateRealOrder();

  enum class NextAction : int8_t { kPlaceInitialOrder, kPlaceLimitOrder, kPlaceMarketOrder, kWait };

  TimePoint lastPriceUpdateTime;
  MonetaryAmount price;
  MonetaryAmount lastPrice;

  OrderId orderId;

  TradedAmounts totalTradedAmounts(fromCurrency, toCurrency);

  NextAction nextAction = NextAction::kPlaceInitialOrder;

  while (true) {
    switch (nextAction) {
      case NextAction::kWait:
        // Do nothing
        break;
      case NextAction::kPlaceMarketOrder:
        options.switchToTakerStrategy();
        [[fallthrough]];
      case NextAction::kPlaceInitialOrder: {
        std::optional<MonetaryAmount> optAvgPrice =
            _exchangePublic.computeAvgOrderPrice(mk, from, options.priceOptions());
        if (!optAvgPrice) {
          log::error("Impossible to compute {} average price on {}", exchangeName(), mk);
          // It's fine to return from there as we don't have a pending order still opened
          return totalTradedAmounts;
        }
        price = *optAvgPrice;
        [[fallthrough]];
      }
      case NextAction::kPlaceLimitOrder:
        [[fallthrough]];
      default: {
        PlaceOrderInfo placeOrderInfo = placeOrderProcess(from, price, tradeInfo);

        orderId = std::move(placeOrderInfo.orderId);

        if (placeOrderInfo.isClosed() || tradeOptions.tradeSyncPolicy() == TradeSyncPolicy::kAsynchronous) {
          totalTradedAmounts += placeOrderInfo.tradedAmounts();
          if (placeOrderInfo.isClosed()) {
            log::debug("Order {} immediately closed with last traded amounts {}", orderId,
                       placeOrderInfo.tradedAmounts());
          } else {
            log::info("Asynchronous mode, exit with order {} placed and traded amounts {}", orderId,
                      placeOrderInfo.tradedAmounts());
          }

          return totalTradedAmounts;
        }

        lastPrice = price;
        lastPriceUpdateTime = Clock::now();
        nextAction = NextAction::kWait;
        break;
      }
    }

    OrderInfo orderInfo = queryOrderInfo(orderId, tradeContext);
    if (orderInfo.isClosed) {
      totalTradedAmounts += orderInfo.tradedAmounts;
      log::debug("Order {} closed with last traded amounts {}", orderId, orderInfo.tradedAmounts);

      break;
    }

    TimePoint nowTime = Clock::now();

    const bool reachedEmergencyTime = options.maxTradeTime() < TimeInS(1) + nowTime - timerStart;
    bool updatePriceNeeded = false;
    if (!options.isFixedPrice() && !reachedEmergencyTime &&
        options.minTimeBetweenPriceUpdates() < nowTime - lastPriceUpdateTime) {
      // Let's see if we need to change the price if limit price has changed.
      std::optional<MonetaryAmount> optLimitPrice =
          _exchangePublic.computeLimitOrderPrice(mk, fromCurrency, options.priceOptions());
      if (optLimitPrice) {
        price = *optLimitPrice;
        updatePriceNeeded =
            (side == TradeSide::kSell && price < lastPrice) || (side == TradeSide::kBuy && price > lastPrice);
      }
    }
    if (reachedEmergencyTime || updatePriceNeeded) {
      log::debug("Cancel order {}", orderId);
      OrderInfo cancelledOrderInfo = cancelOrder(orderId, tradeContext);
      totalTradedAmounts += cancelledOrderInfo.tradedAmounts;
      from -= cancelledOrderInfo.tradedAmounts.tradedFrom;
      if (from == 0) {
        log::debug("Order {} matched with last traded amounts {} while cancelling", orderId,
                   cancelledOrderInfo.tradedAmounts);
        break;
      }

      if (reachedEmergencyTime) {
        // timeout. Action depends on Strategy
        log::warn("Emergency time reached, {} trade", options.timeoutActionStr());
        if (options.placeMarketOrderAtTimeout() && !options.isTakerStrategy(placeSimulatedRealOrder)) {
          nextAction = NextAction::kPlaceMarketOrder;
        } else {
          break;
        }
      } else {
        // updatePriceNeeded
        nextAction = NextAction::kPlaceLimitOrder;
        log::warn("Limit price changed from {} to {}, update order", lastPrice, price);
      }
    }
  }

  return totalTradedAmounts;
}

DeliveredWithdrawInfo ExchangePrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate &targetExchange,
                                                const WithdrawOptions &withdrawOptions) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  InitiatedWithdrawInfo initiatedWithdrawInfo =
      launchWithdraw(grossAmount, targetExchange.queryDepositWallet(currencyCode));
  Duration withdrawRefreshTime = withdrawOptions.withdrawRefreshTime();
  log::info("Withdraw {} of {} to {} initiated from {} to {}, with a periodic refresh time of {} s",
            initiatedWithdrawInfo.withdrawId(), grossAmount, initiatedWithdrawInfo.receivingWallet(), exchangeName(),
            targetExchange.exchangeName(), std::chrono::duration_cast<TimeInS>(withdrawRefreshTime).count());
  SentWithdrawInfo sentWithdrawInfo(currencyCode);
  MonetaryAmount netDeliveredAmount;

  enum class NextAction : int8_t { kCheckSender, kCheckReceiver, kTerminate };

  NextAction nextAction;
  switch (withdrawOptions.withdrawSyncPolicy()) {
    case WithdrawSyncPolicy::kSynchronous:
      nextAction = NextAction::kCheckSender;
      break;
    case WithdrawSyncPolicy::kAsynchronous:
      log::info("Asynchronous mode, exit with withdraw {} initiated", initiatedWithdrawInfo.withdrawId());
      nextAction = NextAction::kTerminate;
      break;
    default:
      unreachable();
  }

  bool canLogAmountMismatchError = true;

  // When withdraw is in Check sender state, we alternatively check sender and then receiver.
  // It's possible that sender status is confirmed later than the receiver in some cases (this can happen for "fast"
  // coins such as Ripple for instance)
  while (nextAction != NextAction::kTerminate) {
    switch (nextAction) {
      case NextAction::kCheckSender:
        sentWithdrawInfo = isWithdrawSuccessfullySent(initiatedWithdrawInfo);
        if (canLogAmountMismatchError && sentWithdrawInfo.netEmittedAmount() + sentWithdrawInfo.fee() !=
                                             initiatedWithdrawInfo.grossEmittedAmount()) {
          canLogAmountMismatchError = false;
          log::info(
              "Net amount {} + actual fee {} != gross emitted amount {}, unharmful but may output incorrect amounts",
              sentWithdrawInfo.netEmittedAmount(), sentWithdrawInfo.fee(), initiatedWithdrawInfo.grossEmittedAmount());
          log::info("Maybe because actual withdraw fee is different");
        }
        if (sentWithdrawInfo.withdrawStatus() == Withdraw::Status::kSuccess) {
          nextAction = NextAction::kCheckReceiver;
          continue;  // to skip the sleep and immediately check receiver
        }
        std::this_thread::sleep_for(withdrawRefreshTime);
        [[fallthrough]];
      case NextAction::kCheckReceiver:
        netDeliveredAmount = targetExchange.queryWithdrawDelivery(initiatedWithdrawInfo, sentWithdrawInfo);
        if (!netDeliveredAmount.isDefault()) {
          log::info("Withdraw successfully received at {}", targetExchange.exchangeName());
          nextAction = NextAction::kTerminate;
          continue;  // to skip the sleep and immediately terminate
        }
        std::this_thread::sleep_for(withdrawRefreshTime);
        [[fallthrough]];
      case NextAction::kTerminate:
        break;
      default:
        unreachable();
    }
  }
  DeliveredWithdrawInfo deliveredWithdrawInfo(std::move(initiatedWithdrawInfo), netDeliveredAmount);
  log::info("Confirmed withdrawal {}", deliveredWithdrawInfo);
  return deliveredWithdrawInfo;
}

namespace {
bool IsAboveDustAmountThreshold(const MonetaryAmountByCurrencySet &dustThresholds, MonetaryAmount amount) {
  const auto foundIt = dustThresholds.find(amount);
  return foundIt == dustThresholds.end() || *foundIt <= amount;
}

using PenaltyPerMarketMap = std::map<Market, int>;

void IncrementPenalty(Market mk, PenaltyPerMarketMap &penaltyPerMarketMap) {
  auto insertItInsertedPair = penaltyPerMarketMap.insert({mk, 1});
  if (!insertItInsertedPair.second) {
    ++insertItInsertedPair.first->second;
  }
}

vector<Market> GetPossibleMarketsForDustThresholds(const BalancePortfolio &balance,
                                                   const MonetaryAmountByCurrencySet &dustThresholds,
                                                   CurrencyCode currencyCode, const MarketSet &markets,
                                                   const PenaltyPerMarketMap &penaltyPerMarketMap) {
  vector<Market> possibleMarkets;
  for (const BalancePortfolio::MonetaryAmountWithEquivalent &avAmountEq : balance) {
    MonetaryAmount avAmount = avAmountEq.amount;
    CurrencyCode avCur = avAmount.currencyCode();
    const auto lbAvAmount = dustThresholds.find(MonetaryAmount(0, avCur));
    if (lbAvAmount == dustThresholds.end() || *lbAvAmount < avAmount) {
      Market mk(currencyCode, avCur);
      if (markets.contains(mk)) {
        possibleMarkets.push_back(std::move(mk));
      } else if (markets.contains(mk.reverse())) {
        possibleMarkets.push_back(mk.reverse());
      }
    }
  }

  struct PenaltyMarketComparator {
    explicit PenaltyMarketComparator(const PenaltyPerMarketMap &map) : penaltyPerMarketMap(map) {}

    bool operator()(Market m1, Market m2) const {
      auto m1It = penaltyPerMarketMap.find(m1);
      auto m2It = penaltyPerMarketMap.find(m2);
      // not present is equivalent to a weight of 0
      int w1 = m1It == penaltyPerMarketMap.end() ? 0 : m1It->second;
      int w2 = m2It == penaltyPerMarketMap.end() ? 0 : m2It->second;

      if (w1 != w2) {
        return w1 < w2;
      }
      return m1 < m2;
    }

    const PenaltyPerMarketMap &penaltyPerMarketMap;
  };

  // Sort them according to the penalty (we favor markets on which we did not try any buy on them yet)
  std::ranges::sort(possibleMarkets, PenaltyMarketComparator(penaltyPerMarketMap));
  return possibleMarkets;
}
}  // namespace

std::pair<TradedAmounts, Market> ExchangePrivate::isSellingPossibleOneShotDustSweeper(
    std::span<const Market> possibleMarkets, MonetaryAmount amountBalance, const TradeOptions &tradeOptions) {
  for (Market mk : possibleMarkets) {
    log::info("Dust sweeper - attempt to sell in one shot on {}", mk);
    TradedAmounts tradedAmounts = marketTrade(amountBalance, tradeOptions, mk);
    if (tradedAmounts.tradedTo != 0) {
      return {tradedAmounts, mk};
    }
  }
  return {};
}

TradedAmounts ExchangePrivate::buySomeAmountToMakeFutureSellPossible(
    std::span<const Market> possibleMarkets, MarketPriceMap &marketPriceMap, MonetaryAmount dustThreshold,
    const BalancePortfolio &balance, const TradeOptions &tradeOptions,
    const MonetaryAmountByCurrencySet &dustThresholds) {
  CurrencyCode currencyCode = dustThreshold.currencyCode();
  static constexpr MonetaryAmount kMultiplier(15, CurrencyCode(), 1);

  if (marketPriceMap.empty()) {
    marketPriceMap = _exchangePublic.queryAllPrices();
  }

  for (MonetaryAmount mult = MonetaryAmount(1);; mult *= kMultiplier) {
    bool enoughAvAmount = false;
    for (Market mk : possibleMarkets) {
      // We will buy some amount. It should be as small as possible to limit fees
      log::debug("Dust sweeper - attempt to buy on {} with multiplier {}", mk, mult);
      auto it = marketPriceMap.find(mk);
      if (it == marketPriceMap.end()) {
        continue;
      }

      // Compute initial fromAmount from a random small amount defined from current price and dust threshold
      // (assuming it's rather small)
      MonetaryAmount price = it->second;
      MonetaryAmount fromAmount;
      if (currencyCode == mk.base()) {
        fromAmount = MonetaryAmount(dustThreshold * price.toNeutral(), mk.quote());
      } else {
        fromAmount = MonetaryAmount(dustThreshold / price.toNeutral(), mk.base());
      }

      fromAmount *= mult;

      MonetaryAmount fromAmountAv = balance.get(fromAmount.currencyCode());
      if (fromAmountAv < fromAmount || !IsAboveDustAmountThreshold(dustThresholds, fromAmountAv - fromAmount)) {
        // The resulting sell of fromAmount should not bring this other currency below the dust thresholds,
        // it's counter productive
        continue;
      }

      enoughAvAmount = true;

      log::info("Dust sweeper - attempt to buy some {} for future selling", currencyCode);
      TradedAmounts tradedAmounts = marketTrade(fromAmount, tradeOptions, mk);

      if (tradedAmounts.tradedTo != 0) {
        // Then we should have sufficient amount now on this market
        return tradedAmounts;
      }
    }
    if (!enoughAvAmount) {
      break;
    }
  }
  return TradedAmounts{};
}

TradedAmountsVectorWithFinalAmount ExchangePrivate::queryDustSweeper(CurrencyCode currencyCode) {
  const MonetaryAmountByCurrencySet &dustThresholds = exchangeInfo().dustAmountsThreshold();
  const int dustSweeperMaxNbTrades = exchangeInfo().dustSweeperMaxNbTrades();
  const auto dustThresholdLb = dustThresholds.find(MonetaryAmount(0, currencyCode));
  const auto eName = exchangeName();

  TradedAmountsVectorWithFinalAmount ret;
  if (dustThresholdLb == dustThresholds.end()) {
    log::warn("No dust threshold is configured for {} on {:n}", currencyCode, eName);
    return ret;
  }
  const MonetaryAmount dustThreshold = *dustThresholdLb;

  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  MarketSet markets = _exchangePublic.queryTradableMarkets();
  MarketPriceMap marketPriceMap;
  bool checkAmountBalanceAgainstDustThreshold = true;
  int dustSweeperTradePos;
  PenaltyPerMarketMap penaltyPerMarketMap;
  Market tradedMarket;
  for (dustSweeperTradePos = 0; dustSweeperTradePos < dustSweeperMaxNbTrades; ++dustSweeperTradePos) {
    BalancePortfolio balance = queryAccountBalance();
    ret.finalAmount = balance.get(currencyCode);
    log::info("Dust sweeper for {} - step {}/{} - {} remaining", eName, dustSweeperTradePos + 1, dustSweeperMaxNbTrades,
              ret.finalAmount);
    if (ret.finalAmount == 0) {
      if (checkAmountBalanceAgainstDustThreshold) {
        log::info("Already no {} present in {} balance", currencyCode, eName);
      } else {
        log::info("Successfully sold all {} on {}", currencyCode, eName);
      }
      return ret;
    }
    if (checkAmountBalanceAgainstDustThreshold && dustThreshold < ret.finalAmount) {
      log::warn("Initial amount balance {} is larger that dust threshold {} on {}, abort", ret.finalAmount,
                dustThreshold, eName);
      return ret;
    }
    checkAmountBalanceAgainstDustThreshold = false;

    // Pick a trade currency which has some available balance for which the market exists with 'currencyCode',
    // whose amount is higher than its dust amount threshold if it exists
    vector<Market> possibleMarkets =
        GetPossibleMarketsForDustThresholds(balance, dustThresholds, currencyCode, markets, penaltyPerMarketMap);
    if (possibleMarkets.empty()) {
      log::warn("No more market is allowed for trade in dust threshold sweeper context");
      break;
    }

    log::info("Dust sweeper for {} - exploring {} markets", eName, possibleMarkets.size());

    // First pass - check if by chance on selected markets selling is possible in one shot
    TradedAmounts tradedAmounts;
    std::tie(tradedAmounts, tradedMarket) =
        isSellingPossibleOneShotDustSweeper(possibleMarkets, ret.finalAmount, tradeOptions);
    if (tradedAmounts.tradedFrom != 0) {
      IncrementPenalty(tradedMarket, penaltyPerMarketMap);
      ret.tradedAmountsVector.push_back(std::move(tradedAmounts));
      continue;
    }

    // At this point we did not sell all amount, but it's possible that some trades have been done, with remainings.
    // Selling has not worked - so we need to buy some amount on the requested currency first
    tradedAmounts = buySomeAmountToMakeFutureSellPossible(possibleMarkets, marketPriceMap, dustThreshold, balance,
                                                          tradeOptions, dustThresholds);
    if (tradedAmounts.tradedFrom == 0) {
      break;
    }
    ret.tradedAmountsVector.push_back(std::move(tradedAmounts));
  }
  log::warn("Could not sell dust on {} after {} tries", eName, dustSweeperTradePos + 1);
  return ret;
}

PlaceOrderInfo ExchangePrivate::placeOrderProcess(MonetaryAmount &from, MonetaryAmount price,
                                                  const TradeInfo &tradeInfo) {
  const Market mk = tradeInfo.tradeContext.mk;
  const bool isSell = tradeInfo.tradeContext.side == TradeSide::kSell;
  const MonetaryAmount volume(isSell ? from : MonetaryAmount(from / price, mk.base()));

  if (tradeInfo.options.isSimulation() && !isSimulatedOrderSupported()) {
    if (exchangeInfo().placeSimulateRealOrder()) {
      log::debug("Place simulate real order - price {} will be overriden", price);
      MarketOrderBook marketOrderbook = _exchangePublic.queryOrderBook(mk);
      price = isSell ? marketOrderbook.getHighestTheoreticalPrice() : marketOrderbook.getLowestTheoreticalPrice();
    } else {
      PlaceOrderInfo placeOrderInfo = computeSimulatedMatchedPlacedOrderInfo(volume, price, tradeInfo);
      from -= placeOrderInfo.tradedAmounts().tradedFrom;
      return placeOrderInfo;
    }
  }

  log::debug("Place new order {} at price {}", volume, price);
  PlaceOrderInfo placeOrderInfo = placeOrder(from, volume, price, tradeInfo);
  if (tradeInfo.options.isSimulation() && isSimulatedOrderSupported()) {
    // Override the placeOrderInfo in simulation mode to centralize code which is same for all exchanges
    // (and remove the need to implement the matching amount computation with fees for each exchange)
    placeOrderInfo = computeSimulatedMatchedPlacedOrderInfo(volume, price, tradeInfo);
  }
  from -= placeOrderInfo.tradedAmounts().tradedFrom;
  return placeOrderInfo;
}

PlaceOrderInfo ExchangePrivate::computeSimulatedMatchedPlacedOrderInfo(MonetaryAmount volume, MonetaryAmount price,
                                                                       const TradeInfo &tradeInfo) const {
  const bool placeSimulatedRealOrder = exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const bool isSell = tradeInfo.tradeContext.side == TradeSide::kSell;

  MonetaryAmount toAmount = isSell ? volume.convertTo(price) : volume;
  ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
  toAmount = _coincenterInfo.exchangeInfo(_exchangePublic.name()).applyFee(toAmount, feeType);
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(isSell ? volume : volume.toNeutral() * price, toAmount)),
                                OrderId("SimulatedOrderId"));
  placeOrderInfo.setClosed();
  return placeOrderInfo;
}

MonetaryAmount ExchangePrivate::queryWithdrawDelivery(
    [[maybe_unused]] const InitiatedWithdrawInfo &initiatedWithdrawInfo, const SentWithdrawInfo &sentWithdrawInfo) {
  MonetaryAmount netEmittedAmount = sentWithdrawInfo.netEmittedAmount();
  const CurrencyCode currencyCode = netEmittedAmount.currencyCode();
  DepositsSet deposits = queryRecentDeposits(DepositsConstraints(currencyCode));

  ClosestRecentDepositPicker closestRecentDepositPicker;

  for (const Deposit &deposit : deposits) {
    closestRecentDepositPicker.addDeposit(RecentDeposit(deposit.amount(), deposit.time()));
  }

  RecentDeposit expectedDeposit(netEmittedAmount, Clock::now());
  return closestRecentDepositPicker.pickClosestRecentDepositOrDefault(expectedDeposit).amount();
}

SentWithdrawInfo ExchangePrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo &initiatedWithdrawInfo) {
  MonetaryAmount grossEmittedAmount = initiatedWithdrawInfo.grossEmittedAmount();
  const CurrencyCode currencyCode = grossEmittedAmount.currencyCode();
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  WithdrawsSet withdraws = queryRecentWithdraws(WithdrawsConstraints(currencyCode, withdrawId));

  if (!withdraws.empty()) {
    if (withdraws.size() > 1) {
      log::warn("Unexpected number of matching withdraws ({}) with unique ID, only most recent one will be considered",
                withdraws.size());
    }

    const Withdraw &withdraw = withdraws.back();

    MonetaryAmount netEmittedAmount = withdraw.amount();
    MonetaryAmount fee = withdraw.withdrawFee();

    log::info("Withdraw status is '{}'", withdraw.statusStr());

    return {netEmittedAmount, fee, withdraw.status()};
  }

  return {currencyCode};
}

}  // namespace cct::api
