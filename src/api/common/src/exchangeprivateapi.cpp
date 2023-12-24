#include "exchangeprivateapi.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
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
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "durationstring.hpp"
#include "exchangeconfig.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "market-vector.hpp"
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
#include "tradeoptions.hpp"
#include "tradeside.hpp"
#include "unreachable.hpp"
#include "wallet.hpp"
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
  BalancePortfolio balancePortfolio = queryAccountBalance(balanceOptions);

  const auto equiCurrency = balanceOptions.equiCurrency();
  if (equiCurrency.isDefined()) {
    computeEquiCurrencyAmounts(balancePortfolio, equiCurrency);
  }

  log::info("Retrieved {} balance for {} assets", exchangeName(), balancePortfolio.size());

  return balancePortfolio;
}

void ExchangePrivate::computeEquiCurrencyAmounts(BalancePortfolio &balancePortfolio, CurrencyCode equiCurrency) {
  MarketOrderBookMap marketOrderBookMap;
  auto fiats = _exchangePublic.queryFiats();
  MarketSet markets;
  ExchangeName exchangeName = this->exchangeName();

  for (auto &[amount, equi] : balancePortfolio) {
    MarketsPath conversionPath =
        _exchangePublic.findMarketsPath(amount.currencyCode(), equiCurrency, markets, fiats,
                                        ExchangePublic::MarketPathMode::kWithPossibleFiatConversionAtExtremity);
    equi = _exchangePublic.convert(amount, equiCurrency, conversionPath, fiats, marketOrderBookMap)
               .value_or(MonetaryAmount(0, equiCurrency));
    log::trace("{} Balance {} (eq. {})", exchangeName, amount, equi);
  }
}

TradedAmounts ExchangePrivate::trade(MonetaryAmount from, CurrencyCode toCurrency, const TradeOptions &options,
                                     const MarketsPath &conversionPath) {
  // Use exchange config settings for un-overriden trade options
  const auto &exchangeConfig = this->exchangeConfig();
  const TradeOptions actualOptions(options, exchangeConfig);
  const bool realOrderPlacedInSimulationMode = !isSimulatedOrderSupported() && exchangeConfig.placeSimulateRealOrder();
  const int nbTrades = static_cast<int>(conversionPath.size());
  const bool isMultiTradeAllowed = actualOptions.isMultiTradeAllowed(exchangeConfig.multiTradeAllowedByDefault());

  ExchangeName exchangeName = this->exchangeName();
  log::info("{}rade {} -> {} on {} requested", isMultiTradeAllowed && nbTrades > 1 ? "Multi t" : "T", from, toCurrency,
            exchangeName);
  log::debug(actualOptions.str(realOrderPlacedInSimulationMode));

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
    TradedAmounts stepTradedAmounts = marketTrade(avAmount, actualOptions, mk);
    avAmount = stepTradedAmounts.to;
    if (avAmount == 0) {
      break;
    }
    if (tradePos == 0) {
      tradedAmounts.from = stepTradedAmounts.from;
    }
    if (tradePos + 1 == nbTrades) {
      tradedAmounts.to = stepTradedAmounts.to;
    }
  }
  return tradedAmounts;
}

TradedAmounts ExchangePrivate::marketTrade(MonetaryAmount from, const TradeOptions &tradeOptions, Market mk) {
  const CurrencyCode fromCurrency = from.currencyCode();
  const CurrencyCode toCurrency = mk.opposite(fromCurrency);

  const TimePoint timerStart = Clock::now();
  const UserRefInt userRef = static_cast<UserRefInt>(TimestampToSecondsSinceEpoch(timerStart) %
                                                     static_cast<int64_t>(std::numeric_limits<UserRefInt>::max()));

  const TradeSide side = fromCurrency == mk.base() ? TradeSide::kSell : TradeSide::kBuy;
  TradeContext tradeContext(mk, side, userRef);
  TradeInfo tradeInfo(tradeContext, tradeOptions);
  TradeOptions &options = tradeInfo.options;
  const bool placeSimulatedRealOrder = exchangeConfig().placeSimulateRealOrder();

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

    const bool reachedEmergencyTime = options.maxTradeTime() < seconds(1) + nowTime - timerStart;
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
      from -= cancelledOrderInfo.tradedAmounts.from;
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

namespace {

enum class NextAction : int8_t { kCheckSender, kCheckReceiver, kTerminate };

NextAction InitializeNextAction(WithdrawSyncPolicy withdrawSyncPolicy) {
  switch (withdrawSyncPolicy) {
    case WithdrawSyncPolicy::kSynchronous:
      return NextAction::kCheckSender;
    case WithdrawSyncPolicy::kAsynchronous:
      log::info("Asynchronous mode, exit after withdraw initiated");
      return NextAction::kTerminate;
    default:
      throw exception("Unexpected withdraw sync policy {}", static_cast<int>(withdrawSyncPolicy));
  }
}

}  // namespace

DeliveredWithdrawInfo ExchangePrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate &targetExchange,
                                                const WithdrawOptions &withdrawOptions) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  const Duration withdrawRefreshTime = withdrawOptions.withdrawRefreshTime();
  const WithdrawOptions::Mode mode = withdrawOptions.mode();

  Wallet destinationWallet = targetExchange.queryDepositWallet(currencyCode);

  InitiatedWithdrawInfo initiatedWithdrawInfo;

  switch (mode) {
    case WithdrawOptions::Mode::kReal:
      initiatedWithdrawInfo = launchWithdraw(grossAmount, std::move(destinationWallet));
      break;
    case WithdrawOptions::Mode::kSimulation:
      initiatedWithdrawInfo = InitiatedWithdrawInfo(std::move(destinationWallet), "<Simulated>", grossAmount);
      break;
    default:
      throw exception("Unknown withdrawal mode {}", static_cast<int>(mode));
  }

  log::info("Withdraw '{}' of {} to {} initiated from {} to {}, with a periodic refresh time of {}",
            initiatedWithdrawInfo.withdrawId(), grossAmount, initiatedWithdrawInfo.receivingWallet(), exchangeName(),
            targetExchange.exchangeName(), DurationToString(withdrawRefreshTime));

  const auto isSimulatedMode = mode == WithdrawOptions::Mode::kSimulation;

  bool canLogAmountMismatchError = true;
  SentWithdrawInfo sentWithdrawInfo(currencyCode);
  ReceivedWithdrawInfo receivedWithdrawInfo;

  // When withdraw is in Check sender state, we alternatively check sender and then receiver.
  // It's possible that sender status is confirmed later than the receiver in some cases (this can happen for "fast"
  // coins such as Ripple for instance)
  NextAction nextAction = InitializeNextAction(withdrawOptions.withdrawSyncPolicy());
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
        if (sentWithdrawInfo.withdrawStatus() == Withdraw::Status::kSuccess || isSimulatedMode) {
          nextAction = NextAction::kCheckReceiver;
          continue;  // to skip the sleep and immediately check receiver
        }
        std::this_thread::sleep_for(withdrawRefreshTime);
        [[fallthrough]];
      case NextAction::kCheckReceiver:
        receivedWithdrawInfo = targetExchange.queryWithdrawDelivery(initiatedWithdrawInfo, sentWithdrawInfo);
        if (isSimulatedMode) {
          // we override the received withdraw info for simulation mode (we call anyway the queryWithdrawDelivery for
          // test purposes)
          receivedWithdrawInfo = ReceivedWithdrawInfo("<Simulated>", grossAmount);
        }
        if (!receivedWithdrawInfo.receivedAmount().isDefault()) {
          log::info("Withdraw successfully received at {}", targetExchange.exchangeName());
          nextAction = NextAction::kTerminate;
          continue;  // to skip the sleep and immediately terminate
        }
        std::this_thread::sleep_for(withdrawRefreshTime);
        break;
      case NextAction::kTerminate:
        break;
      default:
        unreachable();
    }
  }
  DeliveredWithdrawInfo deliveredWithdrawInfo(std::move(initiatedWithdrawInfo), std::move(receivedWithdrawInfo));
  log::info("Confirmed {} withdrawal {}", isSimulatedMode ? "simulated" : "real", deliveredWithdrawInfo);
  return deliveredWithdrawInfo;
}

namespace {
bool IsAboveDustAmountThreshold(const MonetaryAmountByCurrencySet &dustThresholds, MonetaryAmount amount) {
  const auto foundIt = dustThresholds.find(amount);
  return foundIt == dustThresholds.end() || *foundIt <= amount;
}

using PenaltyPerMarketMap = std::map<Market, int>;

MarketVector GetPossibleMarketsForDustThresholds(const BalancePortfolio &balance,
                                                 const MonetaryAmountByCurrencySet &dustThresholds,
                                                 CurrencyCode currencyCode, const MarketSet &markets,
                                                 const PenaltyPerMarketMap &penaltyPerMarketMap) {
  MarketVector possibleMarkets;
  for (const auto [avAmount, _] : balance) {
    const CurrencyCode avCur = avAmount.currencyCode();
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

  class PenaltyMarketComparator {
   public:
    explicit PenaltyMarketComparator(const PenaltyPerMarketMap &map) : _penaltyPerMarketMap(map) {}

    bool operator()(Market m1, Market m2) const {
      const int w1 = weight(m1);
      const int w2 = weight(m2);

      if (w1 != w2) {
        return w1 < w2;
      }
      return m1 < m2;
    }

   private:
    [[nodiscard]] int weight(Market mk) const {
      // not present is equivalent to a weight of 0
      const auto it = _penaltyPerMarketMap.find(mk);
      return it == _penaltyPerMarketMap.end() ? 0 : it->second;
    }

    const PenaltyPerMarketMap &_penaltyPerMarketMap;
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
    if (tradedAmounts.to != 0) {
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

      if (tradedAmounts.to != 0) {
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
  const auto &exchangeConfig = this->exchangeConfig();
  const MonetaryAmountByCurrencySet &dustThresholds = exchangeConfig.dustAmountsThreshold();
  const int dustSweeperMaxNbTrades = exchangeConfig.dustSweeperMaxNbTrades();
  const auto dustThresholdLb = dustThresholds.find(MonetaryAmount(0, currencyCode));
  const auto eName = exchangeName();

  TradedAmountsVectorWithFinalAmount ret;
  if (dustThresholdLb == dustThresholds.end()) {
    log::warn("No dust threshold is configured for {} on {:n}", currencyCode, eName);
    return ret;
  }
  const MonetaryAmount dustThreshold = *dustThresholdLb;

  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(TradeOptions{priceOptions}, exchangeConfig);
  MarketSet markets = _exchangePublic.queryTradableMarkets();
  MarketPriceMap marketPriceMap;
  bool checkAmountBalanceAgainstDustThreshold = true;
  int dustSweeperTradePos;
  PenaltyPerMarketMap penaltyPerMarketMap;
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
    MarketVector possibleMarkets =
        GetPossibleMarketsForDustThresholds(balance, dustThresholds, currencyCode, markets, penaltyPerMarketMap);
    if (possibleMarkets.empty()) {
      log::warn("No more market is allowed for trade in dust threshold sweeper context");
      break;
    }

    log::info("Dust sweeper for {} - exploring {} markets", eName, possibleMarkets.size());

    // First pass - check if by chance on selected markets selling is possible in one shot
    auto [tradedAmounts, tradedMarket] =
        isSellingPossibleOneShotDustSweeper(possibleMarkets, ret.finalAmount, tradeOptions);
    if (tradedAmounts.from != 0) {
      ++penaltyPerMarketMap[tradedMarket];
      ret.tradedAmountsVector.push_back(std::move(tradedAmounts));
      continue;
    }

    // At this point we did not sell all amount, but it's possible that some trades have been done, with remaining.
    // Selling has not worked - so we need to buy some amount on the requested currency first
    tradedAmounts = buySomeAmountToMakeFutureSellPossible(possibleMarkets, marketPriceMap, dustThreshold, balance,
                                                          tradeOptions, dustThresholds);
    if (tradedAmounts.from == 0) {
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
    if (exchangeConfig().placeSimulateRealOrder()) {
      log::debug("Place simulate real order - price {} will be overriden", price);
      MarketOrderBook marketOrderbook = _exchangePublic.getOrderBook(mk);
      price = isSell ? marketOrderbook.getHighestTheoreticalPrice() : marketOrderbook.getLowestTheoreticalPrice();
    } else {
      PlaceOrderInfo placeOrderInfo = computeSimulatedMatchedPlacedOrderInfo(volume, price, tradeInfo);
      from -= placeOrderInfo.tradedAmounts().from;
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
  from -= placeOrderInfo.tradedAmounts().from;
  return placeOrderInfo;
}

PlaceOrderInfo ExchangePrivate::computeSimulatedMatchedPlacedOrderInfo(MonetaryAmount volume, MonetaryAmount price,
                                                                       const TradeInfo &tradeInfo) const {
  const bool placeSimulatedRealOrder = exchangeConfig().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const bool isSell = tradeInfo.tradeContext.side == TradeSide::kSell;

  MonetaryAmount toAmount = isSell ? volume.convertTo(price) : volume;
  ExchangeConfig::FeeType feeType = isTakerStrategy ? ExchangeConfig::FeeType::kTaker : ExchangeConfig::FeeType::kMaker;
  toAmount = _coincenterInfo.exchangeConfig(_exchangePublic.name()).applyFee(toAmount, feeType);
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(isSell ? volume : volume.toNeutral() * price, toAmount)),
                                OrderId("SimulatedOrderId"));
  placeOrderInfo.setClosed();
  return placeOrderInfo;
}

ReceivedWithdrawInfo ExchangePrivate::queryWithdrawDelivery(
    [[maybe_unused]] const InitiatedWithdrawInfo &initiatedWithdrawInfo, const SentWithdrawInfo &sentWithdrawInfo) {
  MonetaryAmount netEmittedAmount = sentWithdrawInfo.netEmittedAmount();
  const CurrencyCode currencyCode = netEmittedAmount.currencyCode();
  DepositsSet deposits = queryRecentDeposits(DepositsConstraints(currencyCode));

  ClosestRecentDepositPicker closestRecentDepositPicker;
  closestRecentDepositPicker.reserve(static_cast<ClosestRecentDepositPicker::size_type>(deposits.size()));
  std::ranges::transform(deposits, std::back_inserter(closestRecentDepositPicker),
                         [](const Deposit &deposit) { return RecentDeposit(deposit.amount(), deposit.time()); });

  RecentDeposit expectedDeposit(netEmittedAmount, Clock::now());

  int closestDepositPos = closestRecentDepositPicker.pickClosestRecentDepositPos(expectedDeposit);
  if (closestDepositPos == -1) {
    return {};
  }
  const Deposit &deposit = deposits[closestDepositPos];
  return {string(deposit.id()), deposit.amount(), deposit.time()};
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
