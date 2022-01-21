#include "upbitprivateapi.hpp"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <cassert>
#include <execution>
#include <string_view>
#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "monetaryamount.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"
#include "upbitpublicapi.hpp"

namespace cct::api {

namespace {

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostDataT&& curlPostData = CurlPostData(), bool throwIfError = true) {
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData), UpbitPublic::kUserAgent);

  auto jsonWebToken = jwt::create()
                          .set_type("JWT")
                          .set_payload_claim("access_key", jwt::claim(std::string(apiKey.key())))
                          .set_payload_claim("nonce", jwt::claim(std::string(Nonce_TimeSinceEpochInMs())));

  if (!opts.getPostData().empty()) {
    string queryHash = ssl::ShaDigest(ssl::ShaType::kSha512, opts.getPostData().str());

    jsonWebToken.set_payload_claim("query_hash", jwt::claim(std::string(queryHash)))
        .set_payload_claim("query_hash_alg", jwt::claim(std::string("SHA512")));
  }

  // hs256 does not accept std::string_view, we need a copy...
  auto token = jsonWebToken.sign(jwt::algorithm::hs256{std::string(apiKey.privateKey())});
  string authStr("Bearer ");
  authStr.append(token.begin(), token.end());

  opts.appendHttpHeader("Authorization", authStr);

  json dataJson = json::parse(curlHandle.query(endpoint, std::move(opts)));
  if (throwIfError) {
    auto errorIt = dataJson.find("error");
    if (errorIt != dataJson.end()) {
      if (errorIt->contains("name")) {
        throw exception(std::move((*errorIt)["name"].get_ref<string&>()));
      }
      if (errorIt->contains("message")) {
        throw exception(std::move((*errorIt)["message"].get_ref<string&>()));
      }
      throw exception("Unknown Upbit API error message");
    }
  }
  return dataJson;
}
}  // namespace

UpbitPrivate::UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(config, upbitPublic, apiKey),
      _curlHandle(UpbitPublic::kUrlBase, config.metricGatewayPtr(),
                  config.exchangeInfo(upbitPublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle, _apiKey, config.exchangeInfo(_exchangePublic.name()), upbitPublic._cryptowatchApi),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic),
      _withdrawalFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic) {}

CurrencyExchangeFlatSet UpbitPrivate::TradableCurrenciesFunc::operator()() {
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  CurrencyExchangeVector currencies;
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/status/wallet");
  currencies.reserve(static_cast<CurrencyExchangeVector::size_type>(result.size() - excludedCurrencies.size()));
  for (const json& curDetails : result) {
    CurrencyCode cur(curDetails["currency"].get<std::string_view>());
    if (UpbitPublic::CheckCurrencyCode(cur, excludedCurrencies)) {
      std::string_view walletState = curDetails["wallet_state"].get<std::string_view>();
      CurrencyExchange::Withdraw withdrawStatus = CurrencyExchange::Withdraw::kUnavailable;
      CurrencyExchange::Deposit depositStatus = CurrencyExchange::Deposit::kUnavailable;
      if (walletState == "working") {
        withdrawStatus = CurrencyExchange::Withdraw::kAvailable;
        depositStatus = CurrencyExchange::Deposit::kAvailable;
      } else if (walletState == "withdraw_only") {
        withdrawStatus = CurrencyExchange::Withdraw::kAvailable;
      } else if (walletState == "deposit_only") {
        depositStatus = CurrencyExchange::Deposit::kAvailable;
      }
      if (withdrawStatus == CurrencyExchange::Withdraw::kUnavailable) {
        log::debug("{} cannot be withdrawn from Upbit", cur.str());
      }
      if (depositStatus == CurrencyExchange::Deposit::kUnavailable) {
        log::debug("{} cannot be deposited to Upbit", cur.str());
      }
      currencies.emplace_back(cur, cur, cur, depositStatus, withdrawStatus,
                              _cryptowatchApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat
                                                                           : CurrencyExchange::Type::kCrypto);
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Upbit currencies", ret.size());
  return ret;
}

BalancePortfolio UpbitPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/accounts");

  BalancePortfolio ret;
  for (const json& accountDetail : result) {
    MonetaryAmount a(accountDetail["balance"].get<std::string_view>(),
                     accountDetail["currency"].get<std::string_view>());
    this->addBalance(ret, a, equiCurrency);
  }
  return ret;
}

Wallet UpbitPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurlPostData postdata{{"currency", currencyCode.str()}};
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits/coin_address", postdata, false);
  bool generateDepositAddressNeeded = false;
  if (result.contains("error")) {
    std::string_view name = result["error"]["name"].get<std::string_view>();
    std::string_view msg = result["error"]["message"].get<std::string_view>();
    if (name == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one", currencyCode.str());
      generateDepositAddressNeeded = true;
    } else {
      string err("error: ");
      err.append(name).append(", msg: ").append(msg);
      throw exception(std::move(err));
    }
  }
  if (generateDepositAddressNeeded) {
    json genCoinAddressResult =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/deposits/generate_coin_address", postdata);
    if (genCoinAddressResult.contains("success")) {
      log::info("Successfully generated address");
    }
    std::chrono::seconds sleepingTime(1);
    static constexpr int kNbMaxRetries = 8;
    int nbRetries = 0;
    do {
      if (nbRetries > 0) {
        log::info("Waiting {} s for address to be generated...",
                  std::chrono::duration_cast<std::chrono::seconds>(sleepingTime).count());
      }
      std::this_thread::sleep_for(sleepingTime);
      result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits/coin_address", postdata);
      sleepingTime *= 2;
      ++nbRetries;
    } while (nbRetries < kNbMaxRetries && result["deposit_address"].is_null());
  }
  if (result["deposit_address"].is_null()) {
    throw exception("Deposit address for " + string(currencyCode.str()) + " is undefined");
  }
  std::string_view address = result["deposit_address"].get<std::string_view>();
  std::string_view tag;
  if (result.contains("secondary_address") && !result["secondary_address"].is_null()) {
    tag = result["secondary_address"].get<std::string_view>();
  }

  PrivateExchangeName privateExchangeName(_exchangePublic.name(), _apiKey.name());

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(privateExchangeName.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet w(std::move(privateExchangeName), currencyCode, address, tag, walletCheck);
  log::info("Retrieved {}", w.str());
  return w;
}

ExchangePrivate::Orders UpbitPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params{{"state", "wait"}};

  if (openedOrdersConstraints.isCur1Defined()) {
    ExchangePublic::MarketSet markets;
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.append("market", UpbitPublic::ReverseMarketStr(filterMarket));
    }
  }
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/orders", std::move(params));

  Orders openedOrders;
  for (json& orderDetails : data) {
    std::string_view marketStr = orderDetails["market"].get<std::string_view>();
    std::size_t dashPos = marketStr.find('-');
    assert(dashPos != std::string_view::npos);
    CurrencyCode priceCur(std::string_view(marketStr.data(), dashPos));
    CurrencyCode volumeCur(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()));

    if (!openedOrdersConstraints.validateCur(volumeCur, priceCur)) {
      continue;
    }

    // 'created_at' string is in this format: "2019-01-04T13:48:09+09:00"
    TimePoint placedTime = FromString(orderDetails["created_at"].get_ref<const string&>().c_str(), "%Y-%m-%dT%H:%M:%S");
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    string id = std::move(orderDetails["uuid"].get_ref<string&>());
    if (!openedOrdersConstraints.validateOrderId(id)) {
      continue;
    }

    MonetaryAmount originalVolume(orderDetails["volume"].get<std::string_view>(), volumeCur);
    MonetaryAmount remainingVolume(orderDetails["remaining_volume"].get<std::string_view>(), volumeCur);
    MonetaryAmount matchedVolume = originalVolume - remainingVolume;
    MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
    TradeSide side = orderDetails["side"].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

    openedOrders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
  }
  std::ranges::sort(openedOrders);
  openedOrders.shrink_to_fit();
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

void UpbitPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  // No faster way to cancel several orders at once, doing a simple for loop
  for (const Order& o : queryOpenedOrders(openedOrdersConstraints)) {
    cancelOrder(OrderRef(o.id(), 0 /*userRef, unused*/, o.market(), o.side()));
  }
}

PlaceOrderInfo UpbitPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.toCur());
  const bool isTakerStrategy =
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeInfo().placeSimulateRealOrder());
  const Market m = tradeInfo.m;

  const std::string_view askOrBid = fromCurrencyCode == m.base() ? "ask" : "bid";
  const std::string_view orderType = isTakerStrategy ? (fromCurrencyCode == m.base() ? "market" : "price") : "limit";

  CurlPostData placePostData{{"market", UpbitPublic::ReverseMarketStr(m)}, {"side", askOrBid}, {"ord_type", orderType}};

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));

  volume = sanitizeVolume(volume, price);

  if (fromCurrencyCode == m.quote()) {
    // For 'buy', from amount is fee excluded
    ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
    const ExchangeInfo& exchangeInfo = _coincenterInfo.exchangeInfo(_exchangePublic.name());
    if (isTakerStrategy) {
      from = exchangeInfo.applyFee(from, feeType);
    } else {
      volume = exchangeInfo.applyFee(volume, feeType);
    }
  }
  if (isTakerStrategy) {
    // Upbit has an exotic way to distinguish buy and sell on the same market
    if (fromCurrencyCode == m.base()) {
      placePostData.append("volume", volume.amountStr());
    } else {
      placePostData.append("price", from.amountStr());
    }
  } else {
    placePostData.append("volume", volume.amountStr());
    placePostData.append("price", price.amountStr());
  }

  if (isOrderTooSmall(volume, price)) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  json placeOrderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/orders", placePostData);

  placeOrderInfo.orderId = placeOrderRes["uuid"];
  placeOrderInfo.orderInfo = parseOrderJson(placeOrderRes, fromCurrencyCode, m);

  // Upbit takes some time to match the market order - We should wait that it has been matched
  bool takerOrderNotClosed = isTakerStrategy && !placeOrderInfo.orderInfo.isClosed;
  while (takerOrderNotClosed) {
    json orderRes =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", {{"uuid", placeOrderInfo.orderId}});

    placeOrderInfo.orderInfo = parseOrderJson(orderRes, fromCurrencyCode, m);

    takerOrderNotClosed = !placeOrderInfo.orderInfo.isClosed;
  }
  return placeOrderInfo;
}

OrderInfo UpbitPrivate::cancelOrder(const OrderRef& orderRef) {
  CurlPostData postData{{"uuid", orderRef.id}};
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/v1/order", postData);
  bool cancelledOrderClosed = isOrderClosed(orderRes);
  while (!cancelledOrderClosed) {
    orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", postData);
    cancelledOrderClosed = isOrderClosed(orderRes);
  }
  return parseOrderJson(orderRes, orderRef.fromCur(), orderRef.m);
}

OrderInfo UpbitPrivate::queryOrderInfo(const OrderRef& orderRef) {
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", {{"uuid", orderRef.id}});
  const CurrencyCode fromCurrencyCode(orderRef.fromCur());
  return parseOrderJson(orderRes, fromCurrencyCode, orderRef.m);
}

MonetaryAmount UpbitPrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws/chance",
                             {{"currency", currencyCode.str()}});
  std::string_view amountStr = result["currency"]["withdraw_fee"].get<std::string_view>();
  return MonetaryAmount(amountStr, currencyCode);
}

OrderInfo UpbitPrivate::parseOrderJson(const json& orderJson, CurrencyCode fromCurrencyCode, Market m) const {
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, fromCurrencyCode == m.base() ? m.quote() : m.base()),
                      isOrderClosed(orderJson));

  if (orderJson.contains("trades")) {
    CurrencyCode feeCurrencyCode(m.quote());  // TODO: to be confirmed (this is true at least for markets involving KRW)
    MonetaryAmount fee(orderJson["paid_fee"].get<std::string_view>(), feeCurrencyCode);

    for (const json& orderDetails : orderJson["trades"]) {
      MonetaryAmount tradedVol(orderDetails["volume"].get<std::string_view>(), m.base());  // always in base currency
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), m.quote());      // always in quote currency
      MonetaryAmount tradedCost(orderDetails["funds"].get<std::string_view>(), m.quote());

      if (fromCurrencyCode == m.quote()) {
        orderInfo.tradedAmounts.tradedFrom += tradedCost;
        orderInfo.tradedAmounts.tradedTo += tradedVol;
      } else {
        orderInfo.tradedAmounts.tradedFrom += tradedVol;
        orderInfo.tradedAmounts.tradedTo += tradedCost;
      }
    }
    if (fromCurrencyCode == m.quote()) {
      orderInfo.tradedAmounts.tradedFrom += fee;
    } else {
      orderInfo.tradedAmounts.tradedTo -= fee;
    }
  }

  return orderInfo;
}

bool UpbitPrivate::isOrderClosed(const json& orderJson) const {
  std::string_view state = orderJson["state"].get<std::string_view>();
  if (state == "done" || state == "cancel") {
    return true;
  } else if (state == "wait" || state == "watch") {
    return false;
  } else {
    log::error("Unknown state {} to be handled for Upbit", state);
    return true;
  }
}

bool UpbitPrivate::isOrderTooSmall(MonetaryAmount volume, MonetaryAmount price) const {
  /// Value found in this page:
  /// https://cryptoexchangenews.net/2021/02/upbit-notes-information-on-changing-the-minimum-order-amount-at-krw-market-to-stabilize-the/
  /// confirmed with some tests. However, could change in the future.
  constexpr std::array<MonetaryAmount, 2> minOrderAmounts{
      {MonetaryAmount(5000, "KRW"), MonetaryAmount(5, "BTC", 4)}};  // 5000 KRW or 0.0005 BTC is min
  bool orderIsTooSmall = false;
  for (MonetaryAmount minOrderAmount : minOrderAmounts) {
    if (volume.currencyCode() == minOrderAmount.currencyCode()) {
      orderIsTooSmall = volume < minOrderAmount;
      if (orderIsTooSmall) {
        log::warn("No trade of {} because min vol order is {} for this market", volume.str(), minOrderAmount.str());
      }
    } else if (price.currencyCode() == minOrderAmount.currencyCode()) {
      MonetaryAmount orderAmount(volume.toNeutral() * price);
      orderIsTooSmall = orderAmount < minOrderAmount;
      if (orderIsTooSmall) {
        log::warn("No trade of {} because min vol order is {} for this market", orderAmount.str(),
                  minOrderAmount.str());
      }
    }
    if (orderIsTooSmall) {
      break;
    }
  }
  return orderIsTooSmall;
}

MonetaryAmount UpbitPrivate::sanitizeVolume(MonetaryAmount volume, MonetaryAmount price) const {
  // Upbit can return this error for big trades:
  // "최대매수금액 1000000000.0 KRW 보다 작은 주문을 입력해 주세요."
  // It means that total value of the order should not exceed 1000000000 KRW.
  // Let's adjust volume to avoid this issue.
  static constexpr MonetaryAmount kMaximumOrderValue = MonetaryAmount(1000000000, CurrencyCode("KRW"));
  MonetaryAmount ret = volume;
  if (price.currencyCode() == kMaximumOrderValue.currencyCode() && volume.toNeutral() * price > kMaximumOrderValue) {
    log::warn("{} / {} = {}", kMaximumOrderValue.str(), price.str(), (kMaximumOrderValue / price).str());
    ret = MonetaryAmount(kMaximumOrderValue / price, volume.currencyCode());
    log::warn("Order too big, decrease volume {} to {}", volume.str(), ret.str());
  }
  return ret;
}

InitiatedWithdrawInfo UpbitPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{
      {"currency", currencyCode.str()}, {"amount", netEmittedAmount.amountStr()}, {"address", wallet.address()}};
  if (wallet.hasTag()) {
    withdrawPostData.append("secondary_address", wallet.tag());
  }
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/withdraws/coin", withdrawPostData);
  std::string_view withdrawId(result["uuid"].get<std::string_view>());
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo UpbitPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraw",
                             {{"currency", currencyCode.str()}, {"uuid", initiatedWithdrawInfo.withdrawId()}});
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  MonetaryAmount realFee(result["fee"].get<std::string_view>(), currencyCode);
  if (realFee != withdrawFee) {
    log::error("{} withdraw fee is {} instead of {}", _exchangePublic.name(), realFee.str(), withdrawFee.str());
  }
  MonetaryAmount netEmittedAmount(result["amount"].get<std::string_view>(), currencyCode);

  std::string_view state(result["state"].get<std::string_view>());
  string stateUpperStr = ToUpper(state);
  log::debug("{} withdrawal status {}", _exchangePublic.name(), state);
  // state values: {'submitting', 'submitted', 'almost_accepted', 'rejected', 'accepted', 'processing', 'done',
  // 'canceled'}
  const bool isCanceled = stateUpperStr == "CANCELED";
  if (isCanceled) {
    log::error("{} withdraw of {} has been cancelled", _exchangePublic.name(), currencyCode.str());
  }
  const bool isDone = stateUpperStr == "DONE";
  return SentWithdrawInfo(netEmittedAmount, isDone || isCanceled);
}

bool UpbitPrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                      const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits",
                             {{"currency", currencyCode.str()}, {"state", "accepted"}});
  RecentDeposit::RecentDepositVector recentDeposits;
  recentDeposits.reserve(recentDeposits.size());
  for (const json& trx : result) {
    MonetaryAmount amount(trx["amount"].get<std::string_view>(), currencyCode);
    // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
    TimePoint timestamp = FromString(trx["done_at"].get_ref<const string&>().c_str(), "%Y-%m-%dT%H:%M:%S");

    recentDeposits.emplace_back(amount, timestamp);
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  return expectedDeposit.selectClosestRecentDeposit(recentDeposits) != nullptr;
}

}  // namespace cct::api
