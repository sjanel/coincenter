#include "binanceprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "binancepublicapi.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "closed-order.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "durationstring.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "monetary-amount-vector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

/// Binance is often slow to update its databases of open / closed orders once it gives us a new order.
/// The number of retries should be sufficiently high to avoid program to crash because of this.
/// It can happen to retry 10 times
constexpr int kNbOrderRequestsRetries = 20;

constexpr int kInvalidTimestamp = -1021;
constexpr int kCancelRejectedStatusCode = -2011;
constexpr int kNoSuchOrderStatusCode = -2013;
constexpr int kInvalidApiKey = -2015;

// Deposit statuses:
// 0(0:pending,6: credited but cannot withdraw, 7=Wrong Deposit,8=Waiting User confirm, 1:success)
constexpr int kDepositPendingCode = 0;
constexpr int kDepositSuccessCode = 1;
constexpr int kDepositCreditedButCannotWithdrawCode = 6;
constexpr int kDepositWrongDepositCode = 7;
constexpr int kDepositWaitingUserConfirmCode = 8;

// Withdraw statuses:
// 0(0:Email Sent,1:Cancelled 2:Awaiting Approval 3:Rejected 4:Processing 5:Failure 6:Completed)
constexpr int kWithdrawEmailSentCode = 0;
constexpr int kWithdrawCancelledCode = 1;
constexpr int kWithdrawAwaitingApprovalCode = 2;
constexpr int kWithdrawRejectedCode = 3;
constexpr int kWithdrawProcessingCode = 4;
constexpr int kWithdrawFailureCode = 5;
constexpr int kWithdrawCompletedCode = 6;

enum class QueryDelayDir : int8_t {
  kNoDir,
  kAhead,
  kBehind,
};

void SetNonceAndSignature(const APIKey& apiKey, CurlPostData& postData, Duration queryDelay) {
  Nonce nonce = Nonce_TimeSinceEpochInMs(queryDelay);
  postData.set("timestamp", nonce);

  static constexpr std::string_view kSignatureKey = "signature";

  auto sha256Hex = ssl::Sha256Hex(postData.str(), apiKey.privateKey());

  postData.set_back(kSignatureKey, std::string_view(sha256Hex));
}

bool CheckErrorDoRetry(int statusCode, const json& ret, QueryDelayDir& queryDelayDir, Duration& sleepingTime,
                       Duration& queryDelay) {
  static constexpr Duration kInitialDurationQueryDelay = milliseconds(200);
  switch (statusCode) {
    case kInvalidTimestamp: {
      auto msgIt = ret.find("msg");
      if (msgIt != ret.end()) {
        std::string_view msg = msgIt->get<std::string_view>();

        // 'Timestamp for this request was 1000ms ahead of the server's time.' may be the error message.
        // I guess this could happen when client time is not synchronized with binance time.
        // Let's try to induce a delay in this case.
        auto aheadPos = msg.find("ahead of the server's time");
        if (aheadPos != std::string_view::npos) {
          if (queryDelayDir != QueryDelayDir::kAhead) {
            queryDelayDir = QueryDelayDir::kAhead;

            sleepingTime = kInitialDurationQueryDelay;
          }
          queryDelay -= sleepingTime;
          log::warn("Our local time is ahead of Binance server's time. Query delay modified to {}",
                    DurationToString(queryDelay));
          // Ensure Nonce is increasing while modifying the query delay
          std::this_thread::sleep_for(sleepingTime);
          return true;
        }

        // If we are behind Binance clock, it returns below error message.
        auto behindPos = msg.find("Timestamp for this request is outside of the recvWindow.");
        if (behindPos != std::string_view::npos) {
          if (queryDelayDir != QueryDelayDir::kBehind) {
            queryDelayDir = QueryDelayDir::kBehind;

            sleepingTime = kInitialDurationQueryDelay;
          }
          queryDelay += sleepingTime;
          log::warn("Our local time is behind of Binance server's time. Query delay modified to {}",
                    DurationToString(queryDelay));
          return true;
        }
      }
      break;
    }
    case kCancelRejectedStatusCode:
      [[fallthrough]];
    case kNoSuchOrderStatusCode:
      // Order does not exist : this may be possible when we query an order info too fast
      log::warn("Binance cannot find order");
      return true;
    case kInvalidApiKey:
      log::error("Binance reported invalid API Key error");
      return false;
    default:
      break;
  }

  // unmanaged error
  return false;
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  Duration& queryDelay, CurlPostDataT&& curlPostData = CurlPostData(), bool throwIfError = true) {
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData));
  opts.mutableHttpHeaders().emplace_back("X-MBX-APIKEY", apiKey.key());

  Duration sleepingTime = curlHandle.minDurationBetweenQueries();
  int statusCode{};
  QueryDelayDir queryDelayDir = QueryDelayDir::kNoDir;
  json ret;
  for (int retryPos = 0; retryPos < kNbOrderRequestsRetries; ++retryPos) {
    if (retryPos != 0) {
      log::trace("Wait {}...", DurationToString(sleepingTime));
      std::this_thread::sleep_for(sleepingTime);
      sleepingTime = (3 * sleepingTime) / 2;
    }

    SetNonceAndSignature(apiKey, opts.mutablePostData(), queryDelay);

    static constexpr bool kAllowExceptions = false;

    ret = json::parse(curlHandle.query(endpoint, opts), nullptr, kAllowExceptions);
    if (ret.is_discarded()) {
      log::error("Badly formatted response from Binance, retry");
      continue;
    }

    auto codeIt = ret.find("code");
    if (codeIt == ret.end() || !ret.contains("msg")) {
      return ret;
    }

    // error in query
    statusCode = *codeIt;  // "1100" for instance

    if (CheckErrorDoRetry(statusCode, ret, queryDelayDir, sleepingTime, queryDelay)) {
      continue;
    }

    break;
  }
  if (throwIfError) {
    log::error("Full Binance json error for {}: '{}'", apiKey.name(), ret.dump());
    throw exception("Error: {}, msg: {}", MonetaryAmount(statusCode), ret["msg"].get<std::string_view>());
  }
  return ret;
}

}  // namespace

BinancePrivate::BinancePrivate(const CoincenterInfo& coincenterInfo, BinancePublic& binancePublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, binancePublic, apiKey),
      _curlHandle(BinancePublic::kURLBases, coincenterInfo.metricGatewayPtr(),
                  exchangeConfig().curlOptionsBuilderBase(ExchangeConfig::Api::kPrivate).build(),
                  coincenterInfo.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _apiKey, binancePublic, _queryDelay),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic, _queryDelay),
      _allWithdrawFeesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic, _queryDelay),
      _withdrawFeesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic, _queryDelay) {}

CurrencyExchangeFlatSet BinancePrivate::TradableCurrenciesCache::operator()() {
  json allCoins =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/config/getall", _queryDelay);
  return BinanceGlobalInfos::ExtractTradableCurrencies(allCoins,
                                                       _exchangePublic.exchangeConfig().excludedCurrenciesAll());
}

bool BinancePrivate::validateApiKey() {
  static constexpr bool throwIfError = false;
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/account/status", _queryDelay,
                             CurlPostData(), throwIfError);
  return result.find("code") == result.end();
}

BalancePortfolio BinancePrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  const json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/account", _queryDelay);
  const bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;

  BalancePortfolio balancePortfolio;

  auto dataIt = result.find("balances");
  if (dataIt == result.end()) {
    log::error("Unexpected get account balance reply from {}", exchangeName());
    return balancePortfolio;
  }

  balancePortfolio.reserve(static_cast<BalancePortfolio::size_type>(dataIt->size()));
  for (const json& balance : *dataIt) {
    CurrencyCode currencyCode(balance["asset"].get<std::string_view>());
    MonetaryAmount amount(balance["free"].get<std::string_view>(), currencyCode);

    if (withBalanceInUse) {
      MonetaryAmount usedAmount(balance["locked"].get<std::string_view>(), currencyCode);
      amount += usedAmount;
    }

    balancePortfolio += amount;
  }
  return balancePortfolio;
}

Wallet BinancePrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  // Limitation : we do not provide network here, we use default in accordance of getTradableCurrenciesService
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/deposit/address",
                             _queryDelay, {{"coin", currencyCode.str()}});
  std::string_view tag(result["tag"].get<std::string_view>());
  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeConfig(_exchangePublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet wallet(ExchangeName(_exchangePublic.name(), _apiKey.name()), currencyCode,
                std::move(result["address"].get_ref<string&>()), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {} (URL: '{}')", wallet, result["url"].get<std::string_view>());
  return wallet;
}

bool BinancePrivate::checkMarketAppendSymbol(Market mk, CurlPostData& params) {
  const auto optMarket = _exchangePublic.retrieveMarket(mk.base(), mk.quote());
  if (!optMarket) {
    return false;
  }
  params.emplace_back("symbol", optMarket->assetsPairStrUpper());
  return true;
}

namespace {
template <class OrderVectorType>
void FillOrders(const OrdersConstraints& ordersConstraints, const json& ordersArray, ExchangePublic& exchangePublic,
                OrderVectorType& orderVector) {
  const auto cur1Str = ordersConstraints.curStr1();
  const auto cur2Str = ordersConstraints.curStr2();

  MarketSet markets;
  for (const json& orderDetails : ordersArray) {
    std::string_view marketStr = orderDetails["symbol"].get<std::string_view>();  // already higher case
    std::size_t cur1Pos = marketStr.find(cur1Str);
    if (ordersConstraints.isCurDefined() && cur1Pos == std::string_view::npos) {
      continue;
    }
    if (ordersConstraints.isCur2Defined() && marketStr.find(cur2Str) == std::string_view::npos) {
      continue;
    }
    const auto placedTimeMsSinceEpoch = orderDetails["time"].get<int64_t>();

    const TimePoint placedTime{milliseconds(placedTimeMsSinceEpoch)};
    if (!ordersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    const auto optMarket = exchangePublic.determineMarketFromMarketStr(marketStr, markets, ordersConstraints.cur1());

    if (!optMarket) {
      continue;
    }

    const CurrencyCode volumeCur = optMarket->base();
    const CurrencyCode priceCur = optMarket->quote();
    const int64_t orderId = orderDetails["orderId"].get<int64_t>();
    string id = ToString(orderId);
    if (!ordersConstraints.validateId(id)) {
      continue;
    }

    const MonetaryAmount matchedVolume(orderDetails["executedQty"].get<std::string_view>(), volumeCur);
    const MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
    const TradeSide side = orderDetails["side"].get<std::string_view>() == "BUY" ? TradeSide::kBuy : TradeSide::kSell;

    using OrderType = std::remove_cvref_t<decltype(*std::declval<OrderVectorType>().begin())>;

    if constexpr (std::is_same_v<OrderType, OpenedOrder>) {
      const MonetaryAmount originalVolume(orderDetails["origQty"].get<std::string_view>(), volumeCur);
      const MonetaryAmount remainingVolume = originalVolume - matchedVolume;

      orderVector.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
    } else if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
      const auto matchedTimeMsSinceEpoch = orderDetails["updateTime"].get<int64_t>();
      const TimePoint matchedTime{milliseconds(matchedTimeMsSinceEpoch)};

      orderVector.emplace_back(std::move(id), matchedVolume, price, placedTime, matchedTime, side);
    } else {
      // Note: below ugly template lambda can be replaced with 'static_assert(false);' in C++23
      []<bool flag = false>() { static_assert(flag, "no match"); }
      ();
    }
  }
  std::ranges::sort(orderVector);
}
}  // namespace

ClosedOrderVector BinancePrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  ClosedOrderVector closedOrders;
  CurlPostData params;
  if (closedOrdersConstraints.isMarketDefined()) {
    if (!checkMarketAppendSymbol(closedOrdersConstraints.market(), params)) {
      return closedOrders;
    }
    if (closedOrdersConstraints.isPlacedTimeAfterDefined()) {
      params.emplace_back("startTime", TimestampToMillisecondsSinceEpoch(closedOrdersConstraints.placedAfter()));
    }
    if (closedOrdersConstraints.isPlacedTimeBeforeDefined()) {
      params.emplace_back("endTime", TimestampToMillisecondsSinceEpoch(closedOrdersConstraints.placedBefore()));
    }
    const json result =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/allOrders", _queryDelay, std::move(params));

    FillOrders(closedOrdersConstraints, result, _exchangePublic, closedOrders);
    log::info("Retrieved {} closed orders from {}", closedOrders.size(), _exchangePublic.name());
  } else {
    // If market is not provided, it's sadly currently directly impossible to query all closed orders on Binance.
    log::error("Market should be provided to query closed orders on {}", _exchangePublic.name());
  }

  return closedOrders;
}

OpenedOrderVector BinancePrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  OpenedOrderVector openedOrders;
  CurlPostData params;
  if (openedOrdersConstraints.isMarketDefined()) {
    // Symbol (which corresponds to a market) is optional - however, it costs 40 credits if omitted and should exist
    if (!checkMarketAppendSymbol(openedOrdersConstraints.market(), params)) {
      return openedOrders;
    }
  }
  const json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/openOrders", _queryDelay, std::move(params));

  FillOrders(openedOrdersConstraints, result, _exchangePublic, openedOrders);

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
      json cancelledOrders = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v3/openOrders",
                                          _queryDelay, std::move(params));
      return static_cast<int>(cancelledOrders.size());
    }
  }

  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);

  using OrdersByMarketMap = std::unordered_map<Market, SmallVector<OpenedOrder, 3>>;
  OrdersByMarketMap ordersByMarketMap;
  std::for_each(std::make_move_iterator(openedOrders.begin()), std::make_move_iterator(openedOrders.end()),
                [&ordersByMarketMap](OpenedOrder&& order) {
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
      json cancelledOrders =
          PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v3/openOrders", _queryDelay, params);
      nbOrdersCancelled += static_cast<int>(cancelledOrders.size());
    } else {
      for (const OpenedOrder& order : orders) {
        params.set("orderId", order.id());
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v3/order", _queryDelay, params);
      }
      nbOrdersCancelled += orders.size();
    }
  }
  return nbOrdersCancelled;
}

namespace {
Deposit::Status DepositStatusFromCode(int statusInt) {
  switch (statusInt) {
    case kDepositPendingCode:
      return Deposit::Status::kProcessing;
    case kDepositSuccessCode:
      [[fallthrough]];
    case kDepositCreditedButCannotWithdrawCode:
      return Deposit::Status::kSuccess;
    case kDepositWrongDepositCode:
      return Deposit::Status::kFailureOrRejected;
    case kDepositWaitingUserConfirmCode:
      return Deposit::Status::kProcessing;
    default:
      throw exception("Unknown deposit status code {} from Binance", statusInt);
  }
}
}  // namespace

DepositsSet BinancePrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.emplace_back("coin", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isTimeAfterDefined()) {
    options.emplace_back("startTime", TimestampToMillisecondsSinceEpoch(depositsConstraints.timeAfter()));
  }
  if (depositsConstraints.isTimeBeforeDefined()) {
    options.emplace_back("endTime", TimestampToMillisecondsSinceEpoch(depositsConstraints.timeBefore()));
  }
  if (depositsConstraints.isIdDefined()) {
    if (depositsConstraints.idSet().size() == 1) {
      options.emplace_back("txId", depositsConstraints.idSet().front());
    }
  }
  json depositStatus = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/deposit/hisrec",
                                    _queryDelay, std::move(options));
  for (json& depositDetail : depositStatus) {
    int statusInt = depositDetail["status"].get<int>();
    Deposit::Status status = DepositStatusFromCode(statusInt);

    CurrencyCode currencyCode(depositDetail["coin"].get<std::string_view>());
    string& id = depositDetail["id"].get_ref<string&>();
    MonetaryAmount amountReceived(depositDetail["amount"].get<double>(), currencyCode);
    int64_t millisecondsSinceEpoch = depositDetail["insertTime"].get<int64_t>();
    TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};

    deposits.emplace_back(std::move(id), timestamp, amountReceived, status);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(int statusInt, bool logStatus) {
  switch (statusInt) {
    case kWithdrawAwaitingApprovalCode:  // NOLINT(bugprone-branch-clone)
      if (logStatus) {
        log::warn("Awaiting Approval");
      }
      return Withdraw::Status::kProcessing;
    case kWithdrawProcessingCode:
      if (logStatus) {
        log::info("Processing withdraw...");
      }
      return Withdraw::Status::kProcessing;
    case kWithdrawEmailSentCode:
      if (logStatus) {
        log::warn("Email was sent");
      }
      return Withdraw::Status::kProcessing;
    case kWithdrawCancelledCode:  // NOLINT(bugprone-branch-clone)
      if (logStatus) {
        log::warn("Withdraw cancelled");
      }
      return Withdraw::Status::kFailureOrRejected;
    case kWithdrawRejectedCode:
      if (logStatus) {
        log::error("Withdraw rejected");
      }
      return Withdraw::Status::kFailureOrRejected;
    case kWithdrawFailureCode:
      if (logStatus) {
        log::error("Withdraw failed");
      }
      return Withdraw::Status::kFailureOrRejected;
    case kWithdrawCompletedCode:
      if (logStatus) {
        log::info("Withdraw completed!");
      }
      return Withdraw::Status::kSuccess;
    default:
      throw exception("Unknown withdraw status code {}", statusInt);
  }
}

TimePoint RetrieveTimeStampFromWithdrawJson(const json& withdrawJson) {
  int64_t millisecondsSinceEpoch;
  auto completeTimeIt = withdrawJson.find("completeTime");
  if (completeTimeIt != withdrawJson.end()) {
    millisecondsSinceEpoch = completeTimeIt->get<int64_t>();
  } else {
    millisecondsSinceEpoch = withdrawJson["applyTime"].get<int64_t>();
  }
  return TimePoint{milliseconds(millisecondsSinceEpoch)};
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options;
  if (withdrawsConstraints.isCurDefined()) {
    options.emplace_back("coin", withdrawsConstraints.currencyCode().str());
  }
  if (withdrawsConstraints.isTimeAfterDefined()) {
    options.emplace_back("startTime", TimestampToMillisecondsSinceEpoch(withdrawsConstraints.timeAfter()));
  }
  if (withdrawsConstraints.isTimeBeforeDefined()) {
    options.emplace_back("endTime", TimestampToMillisecondsSinceEpoch(withdrawsConstraints.timeBefore()));
  }
  return options;
}

}  // namespace

WithdrawsSet BinancePrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;
  // Binance provides field 'withdrawOrderId' tu customize user id, but it's not well documented
  // so we use Binance generated 'id' instead.
  // What is important is that the same field is considered in both queries 'launchWithdraw' and 'queryRecentWithdraws'
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/withdraw/history",
                           _queryDelay, CreateOptionsFromWithdrawConstraints(withdrawsConstraints));
  for (json& withdrawJson : data) {
    int statusInt = withdrawJson["status"].get<int>();
    Withdraw::Status status = WithdrawStatusFromStatusStr(statusInt, withdrawsConstraints.isCurDefined());
    CurrencyCode currencyCode(withdrawJson["coin"].get<std::string_view>());
    string& id = withdrawJson["id"].get_ref<string&>();
    if (!withdrawsConstraints.validateId(id)) {
      continue;
    }
    MonetaryAmount netEmittedAmount(withdrawJson["amount"].get<double>(), currencyCode);
    MonetaryAmount withdrawFee(withdrawJson["transactionFee"].get<double>(), currencyCode);
    TimePoint timestamp = RetrieveTimeStampFromWithdrawJson(withdrawJson);
    withdraws.emplace_back(std::move(id), timestamp, netEmittedAmount, status, withdrawFee);
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

MonetaryAmountByCurrencySet BinancePrivate::AllWithdrawFeesFunc::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/asset/assetDetail", _queryDelay);
  MonetaryAmountVector fees;
  for (const auto& [curCodeStr, withdrawFeeDetails] : result.items()) {
    if (withdrawFeeDetails["withdrawStatus"].get<bool>()) {
      CurrencyCode cur(curCodeStr);
      fees.emplace_back(withdrawFeeDetails["withdrawFee"].get<std::string_view>(), cur);
    }
  }
  return MonetaryAmountByCurrencySet(std::move(fees));
}

std::optional<MonetaryAmount> BinancePrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/asset/assetDetail", _queryDelay,
                             {{"asset", currencyCode.str()}});
  if (!result.contains(currencyCode.str())) {
    return {};
  }
  const json& withdrawFeeDetails = result[currencyCode.str()];
  if (!withdrawFeeDetails["withdrawStatus"].get<bool>()) {
    log::error("{} is currently unavailable for withdraw from {}", currencyCode, _exchangePublic.name());
  }
  return MonetaryAmount(withdrawFeeDetails["withdrawFee"].get<std::string_view>(), currencyCode);
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
  if (fee.currencyCode() == detailTradedInfo.from.currencyCode()) {
    detailTradedInfo.from += fee;
  } else if (fee.currencyCode() == detailTradedInfo.to.currencyCode()) {
    detailTradedInfo.to -= fee;
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
  const bool placeSimulatedRealOrder = binancePublic.exchangeConfig().placeSimulateRealOrder();
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
      json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/sapi/v1/asset/dust", _queryDelay,
                                 {{"asset", from.currencyStr()}});
      auto transferResultIt = result.find("transferResult");
      if (transferResultIt == result.end() || transferResultIt->empty()) {
        throw exception("Unexpected dust transfer result");
      }
      const json& res = transferResultIt->front();
      SetString(placeOrderInfo.orderId, res["tranId"].get<std::size_t>());
      // 'transfered' is misspelled (against 'transferred') but the field is really named like this in Binance REST API
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
    placePostData.emplace_back("timeInForce", "GTC");
    placePostData.emplace_back("price", price.amountStr());
  }

  const std::string_view methodName = isSimulation ? "/api/v3/order/test" : "/api/v3/order";

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, methodName, _queryDelay, placePostData);
  if (isSimulation) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
  SetString(placeOrderInfo.orderId, result["orderId"].get<std::size_t>());
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
  json result = PrivateQuery(_curlHandle, _apiKey, requestType, "/api/v3/order", _queryDelay,
                             {{"symbol", assets}, {"orderId", orderId}});
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
      myTradesOpts.emplace_back("startTime", timeIt->get<int64_t>() - 100L);  // -100 just to be sure
    }
    result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v3/myTrades", _queryDelay, myTradesOpts);
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
    withdrawPostData.emplace_back("addressTag", destinationWallet.tag());
  }
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/sapi/v1/capital/withdraw/apply",
                             _queryDelay, std::move(withdrawPostData));
  return {std::move(destinationWallet), std::move(result["id"].get_ref<string&>()), grossAmount};
}

MonetaryAmount BinancePrivate::queryWithdrawDelivery(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                                     const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json depositStatus = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/sapi/v1/capital/deposit/hisrec",
                                    _queryDelay, {{"coin", currencyCode.str()}});
  const Wallet& wallet = initiatedWithdrawInfo.receivingWallet();

  ClosestRecentDepositPicker closestRecentDepositPicker;

  for (const json& depositDetail : depositStatus) {
    std::string_view depositAddress(depositDetail["address"].get<std::string_view>());
    int status = depositDetail["status"].get<int>();
    if (status == 1) {  // success
      if (depositAddress == wallet.address()) {
        MonetaryAmount amountReceived(depositDetail["amount"].get<double>(), currencyCode);
        int64_t millisecondsSinceEpoch = depositDetail["insertTime"].get<int64_t>();

        TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};

        closestRecentDepositPicker.addDeposit(RecentDeposit(amountReceived, timestamp));
      }
    }
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  return closestRecentDepositPicker.pickClosestRecentDepositOrDefault(expectedDeposit).amount();
}

}  // namespace cct::api
