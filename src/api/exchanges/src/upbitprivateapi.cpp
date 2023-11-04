#include "upbitprivateapi.hpp"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangeinfo.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "ssl_sha.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "upbitpublicapi.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

enum class IfError : int8_t { kThrow, kNoThrow };

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostDataT&& curlPostData = CurlPostData(), IfError ifError = IfError::kThrow) {
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData));

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

  json ret = json::parse(curlHandle.query(endpoint, opts));
  if (ifError == IfError::kThrow) {
    auto errorIt = ret.find("error");
    if (errorIt != ret.end()) {
      log::error("Full Upbit json error: '{}'", ret.dump());
      if (errorIt->contains("name")) {
        throw exception(std::move((*errorIt)["name"].get_ref<string&>()));
      }
      if (errorIt->contains("message")) {
        throw exception(std::move((*errorIt)["message"].get_ref<string&>()));
      }
      throw exception("Unknown Upbit API error message");
    }
  }
  return ret;
}
}  // namespace

UpbitPrivate::UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(config, upbitPublic, apiKey),
      _curlHandle(UpbitPublic::kUrlBase, config.metricGatewayPtr(),
                  PermanentCurlOptions::Builder()
                      .setMinDurationBetweenQueries(exchangeInfo().privateAPIRate())
                      .setAcceptedEncoding(exchangeInfo().acceptEncoding())
                      .setRequestCallLogLevel(exchangeInfo().requestsCallLogLevel())
                      .setRequestAnswerLogLevel(exchangeInfo().requestsAnswerLogLevel())
                      .build(),
                  config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _apiKey, exchangeInfo(), upbitPublic._commonApi),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic) {}

bool UpbitPrivate::validateApiKey() {
  json ret =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/api_keys", CurlPostData(), IfError::kNoThrow);
  auto errorIt = ret.find("error");
  return errorIt == ret.end() && !ret.empty();
}

CurrencyExchangeFlatSet UpbitPrivate::TradableCurrenciesFunc::operator()() {
  const CurrencyCodeSet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  CurrencyExchangeVector currencies;
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/status/wallet");
  for (const json& curDetails : result) {
    CurrencyCode cur(curDetails["currency"].get<std::string_view>());
    CurrencyCode networkName(curDetails["net_type"].get<std::string_view>());
    if (cur != networkName) {
      log::debug("Forgive about {}-{} as net type is not the main one", cur, networkName);
      continue;
    }
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
        log::debug("{} cannot be withdrawn from Upbit", cur);
      }
      if (depositStatus == CurrencyExchange::Deposit::kUnavailable) {
        log::debug("{} cannot be deposited to Upbit", cur);
      }
      currencies.emplace_back(
          cur, cur, cur, depositStatus, withdrawStatus,
          _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Upbit currencies", ret.size());
  return ret;
}

BalancePortfolio UpbitPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  BalancePortfolio ret;
  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  for (const json& accountDetail : PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/accounts")) {
    CurrencyCode currencyCode(accountDetail["currency"].get<std::string_view>());
    MonetaryAmount availableAmount(accountDetail["balance"].get<std::string_view>(), currencyCode);
    this->addBalance(ret, availableAmount, equiCurrency);

    if (withBalanceInUse) {
      MonetaryAmount amountInUse(accountDetail["locked"].get<std::string_view>(), currencyCode);
      this->addBalance(ret, amountInUse, equiCurrency);
    }
  }
  return ret;
}

Wallet UpbitPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurlPostData postData{{"currency", currencyCode.str()}, {"net_type", currencyCode.str()}};
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits/coin_address", postData,
                             IfError::kNoThrow);
  bool generateDepositAddressNeeded = false;
  auto errorIt = result.find("error");
  if (errorIt != result.end()) {
    std::string_view name = (*errorIt)["name"].get<std::string_view>();
    std::string_view msg = (*errorIt)["message"].get<std::string_view>();
    if (name == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one", currencyCode);
      generateDepositAddressNeeded = true;
    } else {
      throw exception("Upbit error: {}, msg: {}", name, msg);
    }
  }
  if (generateDepositAddressNeeded) {
    json genCoinAddressResult =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/deposits/generate_coin_address", postData);
    if (genCoinAddressResult.contains("success")) {
      log::info("Successfully generated address");
    }
    TimeInS sleepingTime(1);
    static constexpr int kNbMaxRetries = 15;
    int nbRetries = 0;
    do {
      if (nbRetries > 0) {
        log::info("Waiting {} s for address to be generated...",
                  std::chrono::duration_cast<TimeInS>(sleepingTime).count());
      }
      std::this_thread::sleep_for(sleepingTime);
      result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits/coin_address", postData);
      sleepingTime += TimeInS(1);
      ++nbRetries;
    } while (nbRetries < kNbMaxRetries && result["deposit_address"].is_null());
  }
  auto addressIt = result.find("deposit_address");
  if (addressIt == result.end() || addressIt->is_null()) {
    throw exception("Deposit address for {} is undefined", currencyCode);
  }
  std::string_view tag;
  auto secondaryAddressIt = result.find("secondary_address");
  if (secondaryAddressIt != result.end() && !secondaryAddressIt->is_null()) {
    tag = secondaryAddressIt->get<std::string_view>();
  }

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(_exchangePublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet wallet(ExchangeName(_exchangePublic.name(), _apiKey.name()), currencyCode,
                std::move(addressIt->get_ref<string&>()), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

Orders UpbitPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params{{"state", "wait"}};

  if (openedOrdersConstraints.isCur1Defined()) {
    MarketSet markets;
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
    TimePoint placedTime =
        FromString(orderDetails["created_at"].get_ref<const string&>().c_str(), kTimeYearToSecondTSeparatedFormat);
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

int UpbitPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  // No faster way to cancel several orders at once, doing a simple for loop
  Orders openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const Order& order : openedOrders) {
    TradeContext tradeContext(order.market(), order.side());
    cancelOrder(order.id(), tradeContext);
  }
  return openedOrders.size();
}

namespace {
Deposit::Status DepositStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "PROCESSING" || statusStr == "REFUNDING") {
    return Deposit::Status::kProcessing;
  }
  if (statusStr == "ACCEPTED") {
    return Deposit::Status::kSuccess;
  }
  if (statusStr == "CANCELLED" || statusStr == "REJECTED" || statusStr == "TRAVEL_RULE_SUSPECTED" ||
      statusStr == "REFUNDED") {
    return Deposit::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized deposit status '{}' from Upbit", statusStr);
}

constexpr int kNbResultsPerPage = 100;

}  // namespace

DepositsSet UpbitPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options{{"limit", kNbResultsPerPage}};
  if (depositsConstraints.isCurDefined()) {
    options.append("currency", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isIdDefined()) {
    for (std::string_view depositId : depositsConstraints.idSet()) {
      // Use the "PHP" method of arrays in query string parameter
      options.append("txids[]", depositId);
    }
  }

  // To make sure we retrieve all results, ask for next page when maximum results per page is returned
  for (int nbResults = kNbResultsPerPage, page = 1; nbResults == kNbResultsPerPage; ++page) {
    options.set("page", page);
    json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits", options);
    if (deposits.empty()) {
      deposits.reserve(result.size());
    }
    nbResults = static_cast<int>(result.size());
    for (json& trx : result) {
      CurrencyCode currencyCode(trx["currency"].get<std::string_view>());
      MonetaryAmount amount(trx["amount"].get<std::string_view>(), currencyCode);
      // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
      auto timeIt = trx.find("done_at");
      if (timeIt == trx.end() || timeIt->is_null()) {
        // It can be the case for deposits that failed - take the start time instead of the end time in this case
        timeIt = trx.find("created_at");
      }

      TimePoint timestamp = FromString(timeIt->get<std::string_view>(), kTimeYearToSecondTSeparatedFormat);
      if (!depositsConstraints.validateTime(timestamp)) {
        continue;
      }
      string& id = trx["txid"].get_ref<string&>();

      std::string_view statusStr = trx["state"].get<std::string_view>();
      Deposit::Status status = DepositStatusFromStatusStr(statusStr);

      deposits.emplace_back(std::move(id), timestamp, amount, status);
    }
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "WAITING") {
    return Withdraw::Status::kInitial;
  }
  if (statusStr == "PROCESSING") {
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "DONE") {
    return Withdraw::Status::kSuccess;
  }
  // In earlier versions of Upbit API, 'CANCELED' was written with this typo.
  // Let's support both spellings to avoid issues.
  if (statusStr == "FAILED" || statusStr == "CANCELLED" || statusStr == "CANCELED" || statusStr == "REJECTED") {
    return Withdraw::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized withdraw status '{}' from Upbit", statusStr);
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options{{"limit", kNbResultsPerPage}};
  if (withdrawsConstraints.isCurDefined()) {
    options.append("currency", withdrawsConstraints.currencyCode().str());
  }
  if (withdrawsConstraints.isIdDefined()) {
    for (std::string_view depositId : withdrawsConstraints.idSet()) {
      // Use the "PHP" method of arrays in query string parameter
      options.append("txids[]", depositId);
    }
  }
  return options;
}

}  // namespace

WithdrawsSet UpbitPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;
  CurlPostData options = CreateOptionsFromWithdrawConstraints(withdrawsConstraints);
  // To make sure we retrieve all results, ask for next page when maximum results per page is returned
  for (int nbResults = kNbResultsPerPage, page = 1; nbResults == kNbResultsPerPage; ++page) {
    options.set("page", page);
    json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws", options);
    if (withdraws.empty()) {
      withdraws.reserve(result.size());
    }
    nbResults = static_cast<int>(result.size());
    for (json& trx : result) {
      CurrencyCode currencyCode(trx["currency"].get<std::string_view>());
      MonetaryAmount netEmittedAmount(trx["amount"].get<std::string_view>(), currencyCode);
      MonetaryAmount withdrawFee(trx["fee"].get<std::string_view>(), currencyCode);
      // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
      auto timeIt = trx.find("done_at");
      if (timeIt == trx.end() || timeIt->is_null()) {
        // It can be the case for withdraws that failed - take the start time instead of the end time in this case
        timeIt = trx.find("created_at");
      }
      TimePoint timestamp = FromString(timeIt->get<std::string_view>(), kTimeYearToSecondTSeparatedFormat);
      if (!withdrawsConstraints.validateTime(timestamp)) {
        continue;
      }
      string& id = trx["txid"].get_ref<string&>();

      std::string_view statusStr = trx["state"].get<std::string_view>();
      Withdraw::Status status = WithdrawStatusFromStatusStr(statusStr);

      withdraws.emplace_back(std::move(id), timestamp, netEmittedAmount, status, withdrawFee);
    }
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

namespace {
bool IsOrderClosed(const json& orderJson) {
  std::string_view state = orderJson["state"].get<std::string_view>();
  if (state == "done" || state == "cancel") {
    return true;
  }
  if (state == "wait" || state == "watch") {
    return false;
  }
  log::error("Unknown state {} to be handled for Upbit", state);
  return true;
}

OrderInfo ParseOrderJson(const json& orderJson, CurrencyCode fromCurrencyCode, Market mk) {
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, fromCurrencyCode == mk.base() ? mk.quote() : mk.base()),
                      IsOrderClosed(orderJson));

  if (orderJson.contains("trades")) {
    // TODO: to be confirmed (this is true at least for markets involving KRW)
    CurrencyCode feeCurrencyCode(mk.quote());
    MonetaryAmount fee(orderJson["paid_fee"].get<std::string_view>(), feeCurrencyCode);

    for (const json& orderDetails : orderJson["trades"]) {
      MonetaryAmount tradedVol(orderDetails["volume"].get<std::string_view>(), mk.base());  // always in base currency
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), mk.quote());      // always in quote currency
      MonetaryAmount tradedCost(orderDetails["funds"].get<std::string_view>(), mk.quote());

      if (fromCurrencyCode == mk.quote()) {
        orderInfo.tradedAmounts.from += tradedCost;
        orderInfo.tradedAmounts.to += tradedVol;
      } else {
        orderInfo.tradedAmounts.from += tradedVol;
        orderInfo.tradedAmounts.to += tradedCost;
      }
    }
    if (fromCurrencyCode == mk.quote()) {
      orderInfo.tradedAmounts.from += fee;
    } else {
      orderInfo.tradedAmounts.to -= fee;
    }
  }

  return orderInfo;
}

}  // namespace

void UpbitPrivate::applyFee(Market mk, CurrencyCode fromCurrencyCode, bool isTakerStrategy, MonetaryAmount& from,
                            MonetaryAmount& volume) {
  if (fromCurrencyCode == mk.quote()) {
    // For 'buy', from amount is fee excluded
    ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
    const ExchangeInfo& exchangeInfo = _coincenterInfo.exchangeInfo(_exchangePublic.name());
    if (isTakerStrategy) {
      from = exchangeInfo.applyFee(from, feeType);
    } else {
      volume = exchangeInfo.applyFee(volume, feeType);
    }
  }
}

PlaceOrderInfo UpbitPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  const bool placeSimulatedRealOrder = _exchangePublic.exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const Market mk = tradeInfo.tradeContext.mk;

  const std::string_view askOrBid = fromCurrencyCode == mk.base() ? "ask" : "bid";
  const std::string_view orderType = isTakerStrategy ? (fromCurrencyCode == mk.base() ? "market" : "price") : "limit";

  CurlPostData placePostData{
      {"market", UpbitPublic::ReverseMarketStr(mk)}, {"side", askOrBid}, {"ord_type", orderType}};

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  applyFee(mk, fromCurrencyCode, isTakerStrategy, from, volume);

  MonetaryAmount sanitizedVol = UpbitPublic::SanitizeVolume(volume, price);
  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;
  if (volume < sanitizedVol && !isSimulationWithRealOrder) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode,
              sanitizedVol);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  if (isTakerStrategy) {
    // Upbit has an exotic way to distinguish buy and sell on the same market
    if (fromCurrencyCode == mk.base()) {
      placePostData.append("volume", volume.amountStr());
    } else {
      placePostData.append("price", from.amountStr());
    }
  } else {
    placePostData.append("volume", volume.amountStr());
    placePostData.append("price", price.amountStr());
  }

  json placeOrderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/orders", placePostData);

  placeOrderInfo.orderInfo = ParseOrderJson(placeOrderRes, fromCurrencyCode, mk);
  placeOrderInfo.orderId = std::move(placeOrderRes["uuid"].get_ref<string&>());

  // Upbit takes some time to match the market order - We should wait that it has been matched
  bool takerOrderNotClosed = isTakerStrategy && !placeOrderInfo.orderInfo.isClosed;
  while (takerOrderNotClosed) {
    json orderRes =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", {{"uuid", placeOrderInfo.orderId}});

    placeOrderInfo.orderInfo = ParseOrderJson(orderRes, fromCurrencyCode, mk);

    takerOrderNotClosed = !placeOrderInfo.orderInfo.isClosed;
  }
  return placeOrderInfo;
}

OrderInfo UpbitPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  CurlPostData postData{{"uuid", orderId}};
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/v1/order", postData);
  bool cancelledOrderClosed = IsOrderClosed(orderRes);
  while (!cancelledOrderClosed) {
    orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", postData);
    cancelledOrderClosed = IsOrderClosed(orderRes);
  }
  return ParseOrderJson(orderRes, tradeContext.fromCur(), tradeContext.mk);
}

OrderInfo UpbitPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", {{"uuid", orderId}});
  const CurrencyCode fromCurrencyCode(tradeContext.fromCur());
  return ParseOrderJson(orderRes, fromCurrencyCode, tradeContext.mk);
}

MonetaryAmount UpbitPrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws/chance",
                             {{"currency", currencyCode.str()}});
  std::string_view amountStr = result["currency"]["withdraw_fee"].get<std::string_view>();
  return {amountStr, currencyCode};
}

InitiatedWithdrawInfo UpbitPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{{"currency", currencyCode.str()},
                                {"amount", netEmittedAmount.amountStr()},
                                {"address", destinationWallet.address()}};
  if (destinationWallet.hasTag()) {
    withdrawPostData.append("secondary_address", destinationWallet.tag());
  }
  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/withdraws/coin", std::move(withdrawPostData));
  return {std::move(destinationWallet), std::move(result["uuid"].get_ref<string&>()), grossAmount};
}

}  // namespace cct::api
