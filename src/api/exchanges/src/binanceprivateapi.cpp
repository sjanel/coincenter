#include "binanceprivateapi.hpp"

#include <thread>
#include <unordered_map>

#include "apikey.hpp"
#include "binancepublicapi.hpp"
#include "cct_smallvector.hpp"
#include "coincenterinfo.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timestring.hpp"
#include "tradeinfo.hpp"

namespace cct::api {

namespace {

/// Binance is often slow to update its databases of open / closed orders once it gives us a new order.
/// The number of retries should be sufficiently high to avoid program to crash because of this.
/// It can happen to retry 10 times
constexpr int kNbOrderRequestsRetries = 15;

void SetNonceAndSignature(const APIKey& apiKey, CurlPostData& postData) {
  Nonce nonce = Nonce_TimeSinceEpochInMs();
  postData.set("timestamp", nonce);
  // Erase + append signature as it should be computed without the old signature itself
  postData.erase("signature");
  postData.append("signature", ssl::ShaHex(ssl::ShaType::kSha256, postData.str(), apiKey.privateKey()));
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData), BinancePublic::kUserAgent);
  opts.appendHttpHeader("X-MBX-APIKEY", apiKey.key());
  SetNonceAndSignature(apiKey, opts.getPostData());

  json ret = json::parse(curlHandle.query(endpoint, opts));
  auto binanceError = [](const json& data) { return data.contains("code") && data.contains("msg"); };
  if (binanceError(ret)) {
    int statusCode = ret["code"];  // "1100" for instance
    int nbRetries = 0;
    Duration sleepingTime = curlHandle.minDurationBetweenQueries();
    while (++nbRetries < kNbOrderRequestsRetries && (statusCode == -2013 || statusCode == -2011)) {
      // Order does not exist : this may be possible when we query an order info too fast
      log::warn("Binance cannot find order yet");
      sleepingTime = (3 * sleepingTime) / 2;
      log::trace("Wait {} ms...", std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
      SetNonceAndSignature(apiKey, opts.getPostData());
      ret = json::parse(curlHandle.query(endpoint, opts));
      if (!binanceError(ret)) {
        return ret;
      }
      statusCode = ret["code"];
    }
    log::error("Full Binance json error: '{}'", ret.dump());
    throw exception("Error: {}, msg: {}", MonetaryAmount(statusCode), ret["msg"].get<std::string_view>());
  }
  return ret;
}

}  // namespace

BinancePrivate::BinancePrivate(const CoincenterInfo& coincenterInfo, BinancePublic& binancePublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, binancePublic, apiKey),
      _curlHandle(BinancePublic::kURLBases, coincenterInfo.metricGatewayPtr(), exchangeInfo().privateAPIRate(),
                  coincenterInfo.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _apiKey, binancePublic),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic),
      _allWithdrawFeesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic),
      _withdrawFeesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic) {}

CurrencyExchangeFlatSet BinancePrivate::TradableCurrenciesCache::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/config/getall");
  return _public.queryTradableCurrencies(result);
}

BalancePortfolio BinancePrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/account");
  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  BalancePortfolio balancePortfolio;
  for (const json& balance : result["balances"]) {
    CurrencyCode currencyCode(balance["asset"].get<std::string_view>());
    MonetaryAmount amount(balance["free"].get<std::string_view>(), currencyCode);
    MonetaryAmount usedAmount(balance["locked"].get<std::string_view>(), currencyCode);

    if (withBalanceInUse) {
      amount += usedAmount;
    }

    addBalance(balancePortfolio, amount, equiCurrency);
  }
  return balancePortfolio;
}

Wallet BinancePrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  // Limitation : we do not provide network here, we use default in accordance of getTradableCurrenciesService
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/deposit/address",
                             {{"coin", currencyCode.str()}});
  std::string_view tag(result["tag"].get<std::string_view>());
  const CoincenterInfo& coincenterInfo = _public.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(_public.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet wallet(ExchangeName(_public.name(), _apiKey.name()), currencyCode,
                std::move(result["address"].get_ref<string&>()), tag, walletCheck);
  log::info("Retrieved {} (URL: '{}')", wallet, result["url"].get<std::string_view>());
  return wallet;
}

bool BinancePrivate::checkMarketAppendSymbol(Market mk, CurlPostData& params) {
  MarketSet markets = _exchangePublic.queryTradableMarkets();
  if (!markets.contains(mk)) {
    mk = mk.reverse();
    if (!markets.contains(mk)) {
      return false;
    }
  }
  params.append("symbol", mk.assetsPairStrUpper());
  return true;
}

Orders BinancePrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  Orders openedOrders;
  CurlPostData params;
  if (openedOrdersConstraints.isMarketDefined()) {
    // Symbol (which corresponds to a market) is optional - however, it costs 40 credits if omitted and should exist
    if (!checkMarketAppendSymbol(openedOrdersConstraints.market(), params)) {
      return openedOrders;
    }
  }
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/openOrders", std::move(params));

  auto cur1Str = openedOrdersConstraints.curStr1();
  auto cur2Str = openedOrdersConstraints.curStr2();
  MarketSet markets;
  for (const json& orderDetails : result) {
    std::string_view marketStr = orderDetails["symbol"].get<std::string_view>();  // already higher case
    std::size_t cur1Pos = marketStr.find(cur1Str);
    if (openedOrdersConstraints.isCur1Defined() && cur1Pos == std::string_view::npos) {
      continue;
    }
    if (openedOrdersConstraints.isCur2Defined() && marketStr.find(cur2Str) == std::string_view::npos) {
      continue;
    }
    int64_t millisecondsSinceEpoch = orderDetails["time"].get<int64_t>();

    TimePoint placedTime{std::chrono::milliseconds(millisecondsSinceEpoch)};
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    std::optional<Market> optMarket =
        _exchangePublic.determineMarketFromMarketStr(marketStr, markets, openedOrdersConstraints.cur1());

    CurrencyCode volumeCur;
    CurrencyCode priceCur;

    if (optMarket) {
      volumeCur = optMarket->base();
      priceCur = optMarket->quote();
    } else {
      continue;
    }

    int64_t orderId = orderDetails["orderId"].get<int64_t>();
    string id = ToString(orderId);
    if (!openedOrdersConstraints.validateOrderId(id)) {
      continue;
    }

    MonetaryAmount matchedVolume(orderDetails["executedQty"].get<std::string_view>(), volumeCur);
    MonetaryAmount originalVolume(orderDetails["origQty"].get<std::string_view>(), volumeCur);
    MonetaryAmount remainingVolume = originalVolume - matchedVolume;
    MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
    TradeSide side = orderDetails["side"].get<std::string_view>() == "BUY" ? TradeSide::kBuy : TradeSide::kSell;

    openedOrders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

int BinancePrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params;
  bool isMarketDefined = openedOrdersConstraints.isMarketDefined();
  bool canUseCancelAllEndpoint = openedOrdersConstraints.isAtMostMarketDependent();
  if (isMarketDefined) {
    if (!checkMarketAppendSymbol(openedOrdersConstraints.market(), params)) {
      return 0;
    }
    if (canUseCancelAllEndpoint) {
      json cancelledOrders =
          PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v3/openOrders", std::move(params));
      return static_cast<int>(cancelledOrders.size());
    }
  }

  Orders openedOrders = queryOpenedOrders(openedOrdersConstraints);

  using OrdersByMarketMap = std::unordered_map<Market, SmallVector<Order, 3>>;
  OrdersByMarketMap ordersByMarketMap;
  std::for_each(std::make_move_iterator(openedOrders.begin()), std::make_move_iterator(openedOrders.end()),
                [&ordersByMarketMap](Order&& order) {
                  Market mk = order.market();
                  ordersByMarketMap[mk].push_back(std::move(order));
                });
  int nbOrdersCancelled = 0;
  for (const auto& [market, orders] : ordersByMarketMap) {
    if (!isMarketDefined) {
      params.set("symbol", market.assetsPairStrUpper());
    }
    if (orders.size() > 1 && canUseCancelAllEndpoint) {
      params.erase("orderId");
      json cancelledOrders = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v3/openOrders", params);
      nbOrdersCancelled += static_cast<int>(cancelledOrders.size());
    } else {
      for (const Order& order : orders) {
        params.set("orderId", order.id());
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v3/order", params);
      }
      nbOrdersCancelled += orders.size();
    }
  }
  return nbOrdersCancelled;
}

Deposits BinancePrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.append("coin", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isReceivedTimeAfterDefined()) {
    options.append("startTime", TimestampToMs(depositsConstraints.receivedAfter()));
  }
  if (depositsConstraints.isReceivedTimeBeforeDefined()) {
    options.append("endTime", TimestampToMs(depositsConstraints.receivedBefore()));
  }
  if (depositsConstraints.isDepositIdDefined()) {
    if (depositsConstraints.depositIdSet().size() == 1) {
      options.append("txId", depositsConstraints.depositIdSet().front());
    }
  }
  json depositStatus =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/deposit/hisrec", std::move(options));
  for (json& depositDetail : depositStatus) {
    int status = depositDetail["status"].get<int>();
    if (status == 1) {  // 1: success, 0: pending, 6: credited but cannot withdraw
      CurrencyCode currencyCode(depositDetail["coin"].get<std::string_view>());
      string& id = depositDetail["id"].get_ref<string&>();
      MonetaryAmount amountReceived(depositDetail["amount"].get<double>(), currencyCode);
      int64_t millisecondsSinceEpoch = depositDetail["insertTime"].get<int64_t>();
      TimePoint timestamp{TimeInMs(millisecondsSinceEpoch)};

      deposits.emplace_back(std::move(id), timestamp, amountReceived);
    }
  }
  log::info("Retrieved {} recent deposits for {}", deposits.size(), exchangeName());
  return deposits;
}

WithdrawalFeeMap BinancePrivate::AllWithdrawFeesFunc::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/asset/assetDetail");
  WithdrawalFeeMap ret;
  for (const auto& [curCodeStr, withdrawFeeDetails] : result.items()) {
    if (withdrawFeeDetails["withdrawStatus"].get<bool>()) {
      CurrencyCode cur(curCodeStr);
      ret.insert_or_assign(cur, MonetaryAmount(withdrawFeeDetails["withdrawFee"].get<std::string_view>(), cur));
    }
  }
  return ret;
}

MonetaryAmount BinancePrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/asset/assetDetail",
                             {{"asset", currencyCode.str()}});
  if (!result.contains(currencyCode.str())) {
    throw exception("Unable to find asset information in assetDetail query to Binance");
  }
  const json& withdrawFeeDetails = result[currencyCode.str()];
  if (!withdrawFeeDetails["withdrawStatus"].get<bool>()) {
    log::error("{} is currently unavailable for withdraw from {}", currencyCode, _exchangePublic.name());
  }
  return {withdrawFeeDetails["withdrawFee"].get<std::string_view>(), currencyCode};
}

namespace {
TradedAmounts ParseTrades(Market mk, CurrencyCode fromCurrencyCode, const json& fillDetail) {
  MonetaryAmount price(fillDetail["price"].get<std::string_view>(), mk.quote());
  MonetaryAmount quantity(fillDetail["qty"].get<std::string_view>(), mk.base());
  MonetaryAmount quantityTimesPrice = quantity.toNeutral() * price;
  TradedAmounts detailTradedInfo(fromCurrencyCode == mk.quote() ? quantityTimesPrice : quantity,
                                 fromCurrencyCode == mk.quote() ? quantity : quantityTimesPrice);
  MonetaryAmount fee(fillDetail["commission"].get<std::string_view>(),
                     fillDetail["commissionAsset"].get<std::string_view>());
  log::debug("Gross {} has been matched at {} price, with a fee of {}", quantity, price, fee);
  if (fee.currencyCode() == detailTradedInfo.tradedFrom.currencyCode()) {
    detailTradedInfo.tradedFrom += fee;
  } else if (fee.currencyCode() == detailTradedInfo.tradedTo.currencyCode()) {
    detailTradedInfo.tradedTo -= fee;
  } else {
    log::debug("Fee is deduced from {} which is outside {}, do not count it in this trade", fee.currencyStr(), mk);
  }
  return detailTradedInfo;
}

TradedAmounts QueryOrdersAfterPlace(Market mk, CurrencyCode fromCurrencyCode, const json& orderJson) {
  CurrencyCode toCurrencyCode(fromCurrencyCode == mk.quote() ? mk.base() : mk.quote());
  TradedAmounts ret(fromCurrencyCode, toCurrencyCode);

  if (orderJson.contains("fills")) {
    for (const json& fillDetail : orderJson["fills"]) {
      ret += ParseTrades(mk, fromCurrencyCode, fillDetail);
    }
  }

  return ret;
}
}  // namespace

PlaceOrderInfo BinancePrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  const Market mk = tradeInfo.tradeContext.mk;
  const std::string_view buyOrSell = fromCurrencyCode == mk.base() ? "SELL" : "BUY";
  const bool placeSimulatedRealOrder = binancePublic.exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const std::string_view orderType = isTakerStrategy ? "MARKET" : "LIMIT";
  const bool isSimulation = tradeInfo.options.isSimulation();

  price = binancePublic.sanitizePrice(mk, price);

  MonetaryAmount sanitizedVol = binancePublic.sanitizeVolume(mk, volume, price, isTakerStrategy);
  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));
  if (volume < sanitizedVol && !isSimulationWithRealOrder) {
    static constexpr CurrencyCode kBinanceCoinCur("BNB");
    if (!isSimulation && toCurrencyCode == kBinanceCoinCur) {
      // Use special Binance Dust transfer
      log::info("Volume too low for standard trade, but we can use Dust transfer to trade to {}", kBinanceCoinCur);
      json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/sapi/v1/asset/dust",
                                 {{"asset", from.currencyStr()}});
      if (!result.contains("transferResult") || result["transferResult"].empty()) {
        throw exception("Unexpected dust transfer result");
      }
      const json& res = result["transferResult"].front();
      SetString(placeOrderInfo.orderId, res["tranId"].get<std::size_t>());
      MonetaryAmount netTransferredAmount(res["transferedAmount"].get<std::string_view>(), kBinanceCoinCur);
      placeOrderInfo.tradedAmounts() += TradedAmounts(from, netTransferredAmount);
    } else {
      log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode,
                sanitizedVol);
    }

    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
  volume = sanitizedVol;

  CurlPostData placePostData{
      {"symbol", mk.assetsPairStrUpper()}, {"side", buyOrSell}, {"type", orderType}, {"quantity", volume.amountStr()}};

  if (!isTakerStrategy) {
    placePostData.append("timeInForce", "GTC");
    placePostData.append("price", price.amountStr());
  }

  const std::string_view methodName = isSimulation ? "/api/v3/order/test" : "/api/v3/order";

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, methodName, placePostData);
  if (isSimulation) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
  SetString(placeOrderInfo.orderId, result["orderId"].get<size_t>());
  std::string_view status = result["status"].get<std::string_view>();
  if (status == "FILLED" || status == "REJECTED" || status == "EXPIRED") {
    if (status == "FILLED") {
      placeOrderInfo.tradedAmounts() += QueryOrdersAfterPlace(mk, fromCurrencyCode, result);
    } else {
      log::error("{} rejected our place order with status {}", _exchangePublic.name(), status);
    }

    placeOrderInfo.setClosed();
  }
  return placeOrderInfo;
}

OrderInfo BinancePrivate::queryOrder(OrderIdView orderId, const TradeContext& tradeContext,
                                     HttpRequestType requestType) {
  const Market mk = tradeContext.mk;
  const CurrencyCode fromCurrencyCode = tradeContext.side == TradeSide::kSell ? mk.base() : mk.quote();
  const CurrencyCode toCurrencyCode = tradeContext.side == TradeSide::kBuy ? mk.base() : mk.quote();
  const string assetsStr = mk.assetsPairStrUpper();
  const std::string_view assets(assetsStr);
  json result =
      PrivateQuery(_curlHandle, _apiKey, requestType, "/api/v3/order", {{"symbol", assets}, {"orderId", orderId}});
  const std::string_view status = result["status"].get<std::string_view>();
  bool isClosed = false;
  bool queryClosedOrder = false;
  if (status == "FILLED" || status == "CANCELED") {
    isClosed = true;
    queryClosedOrder = true;
  } else if (status == "REJECTED" || status == "EXPIRED") {
    log::error("{} rejected our order {} with status {}", _exchangePublic.name(), orderId, status);
    isClosed = true;
  }
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  if (queryClosedOrder) {
    CurlPostData myTradesOpts{{"symbol", assets}};
    auto timeIt = result.find("time");
    if (timeIt != result.end()) {
      myTradesOpts.append("startTime", timeIt->get<int64_t>() - 100L);  // -100 just to be sure
    }
    result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/myTrades", myTradesOpts);
    int64_t integralOrderId = FromString<int64_t>(orderId);
    for (const json& tradeDetails : result) {
      if (tradeDetails["orderId"].get<int64_t>() == integralOrderId) {
        orderInfo.tradedAmounts += ParseTrades(mk, fromCurrencyCode, tradeDetails);
      }
    }
  }
  return orderInfo;
}

InitiatedWithdrawInfo BinancePrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  CurlPostData withdrawPostData{
      {"coin", currencyCode.str()}, {"address", destinationWallet.address()}, {"amount", grossAmount.amountStr()}};
  if (destinationWallet.hasTag()) {
    withdrawPostData.append("addressTag", destinationWallet.tag());
  }
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/sapi/v1/capital/withdraw/apply",
                             std::move(withdrawPostData));
  return {std::move(destinationWallet), std::move(result["id"].get_ref<string&>()), grossAmount};
}

SentWithdrawInfo BinancePrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json withdrawStatus = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/withdraw/history",
                                     {{"coin", currencyCode.str()}});
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  MonetaryAmount netEmittedAmount;
  MonetaryAmount fee;
  bool isWithdrawSent = false;
  for (const json& withdrawDetail : withdrawStatus) {
    std::string_view withdrawDetailId(withdrawDetail["id"].get<std::string_view>());
    if (withdrawDetailId == withdrawId) {
      int withdrawStatusInt = withdrawDetail["status"].get<int>();
      switch (withdrawStatusInt) {
        case 0:
          log::warn("Email was sent");
          break;
        case 1:
          log::warn("Withdraw cancelled");
          break;
        case 2:
          log::warn("Awaiting Approval");
          break;
        case 3:
          log::error("Withdraw rejected");
          break;
        case 4:
          log::info("Processing withdraw...");
          break;
        case 5:
          log::error("Withdraw failed");
          break;
        case 6:
          log::info("Withdraw completed!");
          isWithdrawSent = true;
          break;
        default:
          log::error("unknown status value {}", withdrawStatusInt);
          break;
      }
      netEmittedAmount = MonetaryAmount(withdrawDetail["amount"].get<double>(), currencyCode);
      fee = MonetaryAmount(withdrawDetail["transactionFee"].get<double>(), currencyCode);
      if (netEmittedAmount + fee != initiatedWithdrawInfo.grossEmittedAmount()) {
        log::warn("{} + {} != {}, maybe a change in API", netEmittedAmount.amountStr(), fee.amountStr(),
                  initiatedWithdrawInfo.grossEmittedAmount().amountStr());
      }
      break;
    }
  }
  return {netEmittedAmount, fee, isWithdrawSent};
}

ReceivedWithdrawInfo BinancePrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                                        const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json depositStatus = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/deposit/hisrec",
                                    {{"coin", currencyCode.str()}});
  const Wallet& wallet = initiatedWithdrawInfo.receivingWallet();
  RecentDeposit::RecentDepositVector recentDeposits;
  for (const json& depositDetail : depositStatus) {
    std::string_view depositAddress(depositDetail["address"].get<std::string_view>());
    int status = depositDetail["status"].get<int>();
    if (status == 1) {  // success
      if (depositAddress == wallet.address()) {
        MonetaryAmount amountReceived(depositDetail["amount"].get<double>(), currencyCode);
        int64_t millisecondsSinceEpoch = depositDetail["insertTime"].get<int64_t>();

        TimePoint timestamp{std::chrono::milliseconds(millisecondsSinceEpoch)};

        recentDeposits.emplace_back(amountReceived, timestamp);
      }
    }
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  const RecentDeposit* pClosestRecentDeposit = expectedDeposit.selectClosestRecentDeposit(recentDeposits);
  return {pClosestRecentDeposit == nullptr ? MonetaryAmount() : pClosestRecentDeposit->amount(),
          pClosestRecentDeposit != nullptr};
}

}  // namespace cct::api
