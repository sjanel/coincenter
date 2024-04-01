#include "huobiprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "base64.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "huobipublicapi.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "request-retry.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "toupperlower-string.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "url-encode.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

string BuildParamStr(HttpRequestType requestType, std::string_view baseUrl, std::string_view method,
                     std::string_view postDataStr) {
  std::string_view urlBaseWithoutHttps(baseUrl.begin() + std::string_view("https://").size(), baseUrl.end());
  std::string_view requestTypeStr = ToString(requestType);
  string paramsStr(requestTypeStr.size() + urlBaseWithoutHttps.size() + method.size() + postDataStr.size() + 3U, '\n');

  auto it = paramsStr.begin();
  it = std::ranges::copy(requestTypeStr, it).out;
  it = std::ranges::copy(urlBaseWithoutHttps, it + 1).out;
  it = std::ranges::copy(method, it + 1).out;
  std::ranges::copy(postDataStr, it + 1);

  return paramsStr;
}

CurlOptions::PostDataFormat ComputePostDataFormat(HttpRequestType requestType, const CurlPostData& postData) {
  CurlOptions::PostDataFormat postDataFormat = CurlOptions::PostDataFormat::kString;
  if (!postData.empty() && requestType != HttpRequestType::kGet) {
    postDataFormat = CurlOptions::PostDataFormat::kJson;
  }
  return postDataFormat;
}

void SetNonceAndSignature(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType,
                          std::string_view endpoint, CurlPostData& postData, CurlPostData& signaturePostData) {
  auto isNotEncoded = [](char ch) { return isalnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~'; };

  signaturePostData.set("Timestamp", URLEncode(Nonce_LiteralDate(kTimeYearToSecondTSeparatedFormat), isNotEncoded));

  if (!postData.empty() && requestType == HttpRequestType::kGet) {
    // Warning: Huobi expects that all parameters of the query are ordered lexicographically
    // We trust the caller for this. In case the order is not respected, error 'Signature not valid' will be
    // returned from Huobi
    signaturePostData.append(postData);
    postData = CurlPostData();
  }

  static constexpr std::string_view kSignatureKey = "Signature";

  signaturePostData.set_back(kSignatureKey,
                             URLEncode(B64Encode(ssl::Sha256Bin(BuildParamStr(requestType, curlHandle.getNextBaseUrl(),
                                                                              endpoint, signaturePostData.str()),
                                                                apiKey.privateKey())),
                                       isNotEncoded));
}

json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostData&& postData = CurlPostData()) {
  CurlPostData signaturePostData{
      {"AccessKeyId", apiKey.key()}, {"SignatureMethod", "HmacSHA256"}, {"SignatureVersion", 2}};

  string method(endpoint);
  method.push_back('?');

  CurlOptions::PostDataFormat postDataFormat = ComputePostDataFormat(requestType, postData);

  RequestRetry requestRetry(curlHandle, CurlOptions(requestType, std::move(postData), postDataFormat),
                            QueryRetryPolicy{.initialRetryDelay = seconds{1}, .nbMaxRetries = 3});

  json ret = requestRetry.queryJson(
      method,
      [](const json& jsonResponse) {
        const auto statusIt = jsonResponse.find("status");
        if (statusIt != jsonResponse.end() && statusIt->get<std::string_view>() != "ok") {
          log::warn("Full Huobi json error: '{}'", jsonResponse.dump());
          return RequestRetry::Status::kResponseError;
        }
        return RequestRetry::Status::kResponseOK;
      },
      [&signaturePostData, &curlHandle, &apiKey, requestType, endpoint, &method](CurlOptions& opts) {
        SetNonceAndSignature(curlHandle, apiKey, requestType, endpoint, opts.mutablePostData(), signaturePostData);

        method.replace(method.begin() + endpoint.size() + 1U, method.end(), signaturePostData.str());
      });

  return ret;
}

constexpr std::string_view kBaseUrlOrders = "/v1/order/orders/";

}  // namespace

HuobiPrivate::HuobiPrivate(const CoincenterInfo& coincenterInfo, HuobiPublic& huobiPublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, huobiPublic, apiKey),
      _curlHandle(HuobiPublic::kURLBases, coincenterInfo.metricGatewayPtr(),
                  exchangeConfig().curlOptionsBuilderBase(ExchangeConfig::Api::kPrivate).build(),
                  coincenterInfo.getRunMode()),
      _accountIdCache(CachedResultOptions(std::chrono::hours(48), _cachedResultVault), _curlHandle, apiKey),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, huobiPublic) {}

bool HuobiPrivate::validateApiKey() {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/account/accounts", CurlPostData());
  if (result.empty()) {
    return false;
  }
  auto statusIt = result.find("status");
  return statusIt == result.end() || statusIt->get<std::string_view>() == "ok";
}

BalancePortfolio HuobiPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  string method = "/v1/account/accounts/";
  AppendString(method, _accountIdCache.get());
  method.append("/balance");
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, method);
  BalancePortfolio balancePortfolio;
  if (result.empty()) {
    return balancePortfolio;
  }

  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  for (const json& balanceDetail : result["data"]["list"]) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    CurrencyCode currencyCode(balanceDetail["currency"].get<std::string_view>());
    MonetaryAmount amount(balanceDetail["balance"].get<std::string_view>(), currencyCode);
    if (typeStr == "trade" || (withBalanceInUse && typeStr == "frozen")) {
      balancePortfolio += amount;
    } else {
      log::trace("Do not consider {} as it is {} on {}", amount, typeStr, _exchangePublic.name());
    }
  }
  return balancePortfolio;
}

Wallet HuobiPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  string lowerCaseCur = ToLower(currencyCode.str());
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v2/account/deposit/address",
                           {{"currency", lowerCaseCur}});

  string address;
  std::string_view tag;
  ExchangeName exchangeName(_huobiPublic.name(), _apiKey.name());
  const CoincenterInfo& coincenterInfo = _huobiPublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeConfig(_huobiPublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  auto dataIt = data.find("data");
  if (dataIt != data.end()) {
    for (json& depositDetail : *dataIt) {
      tag = depositDetail["addressTag"].get<std::string_view>();

      std::string_view addressView = depositDetail["address"].get<std::string_view>();

      if (Wallet::ValidateWallet(walletCheck, exchangeName, currencyCode, addressView, tag)) {
        address = std::move(depositDetail["address"].get_ref<string&>());
        break;
      }
      log::warn("{} & tag {} are not validated in the deposit addresses file", addressView, tag);
      tag = std::string_view();
    }
  }

  Wallet wallet(std::move(exchangeName), currencyCode, std::move(address), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
TradeSide TradeSideFromTypeStr(std::string_view typeSide) {
  if (typeSide.starts_with("buy")) {
    return TradeSide::kBuy;
  }
  if (typeSide.starts_with("sell")) {
    return TradeSide::kSell;
  }
  throw exception("Unable to detect order side for type '{}'", typeSide);
}
}  // namespace

ClosedOrderVector HuobiPrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  ClosedOrderVector closedOrders;

  CurlPostData params;

  if (closedOrdersConstraints.isPlacedTimeBeforeDefined()) {
    params.emplace_back("end-time", TimestampToMillisecondsSinceEpoch(closedOrdersConstraints.placedBefore()));
  }
  if (closedOrdersConstraints.isPlacedTimeAfterDefined()) {
    params.emplace_back("start-time", TimestampToMillisecondsSinceEpoch(closedOrdersConstraints.placedAfter()));
  }

  if (closedOrdersConstraints.isMarketDefined()) {
    // we can use the more detailed endpoint that requires the market

    // Do not ask for cancelled orders without any matched part
    params.emplace_back("states", "filled");

    params.emplace_back("symbol", closedOrdersConstraints.market().assetsPairStrLower());
  } else {
    // Only past 48h orders may be retrieved without market
  }

  const std::string_view closedOrdersEndpoint =
      closedOrdersConstraints.isMarketDefined() ? "/v1/order/orders" : "/v1/order/history";

  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, closedOrdersEndpoint, std::move(params));

  const auto dataIt = data.find("data");
  if (dataIt == data.end()) {
    log::error("Unexpected closed orders query reply for {}", _exchangePublic.name());
    return closedOrders;
  }

  MarketSet markets;

  for (json& orderDetails : *dataIt) {
    string marketStr = ToUpper(orderDetails["symbol"].get<std::string_view>());

    std::optional<Market> optMarket =
        _exchangePublic.determineMarketFromMarketStr(marketStr, markets, closedOrdersConstraints.cur1());

    if (!optMarket) {
      continue;
    }

    if (!closedOrdersConstraints.validateCur(optMarket->base(), optMarket->quote())) {
      continue;
    }

    TimePoint placedTime{milliseconds(orderDetails["created-at"].get<int64_t>())};

    string idStr = ToString(orderDetails["id"].get<int64_t>());

    if (!closedOrdersConstraints.validateId(idStr)) {
      continue;
    }

    TimePoint matchedTime{milliseconds(orderDetails["finished-at"].get<int64_t>())};

    // 'field' seems to be a typo here (instead of 'filled), but it's really sent by Huobi like that.
    MonetaryAmount matchedVolume(orderDetails["field-amount"].get<std::string_view>(), optMarket->base());
    if (matchedVolume == 0) {
      continue;
    }
    MonetaryAmount price(orderDetails["price"].get<std::string_view>(), optMarket->quote());

    std::string_view typeSide = orderDetails["type"].get<std::string_view>();
    TradeSide tradeSide = TradeSideFromTypeStr(typeSide);

    closedOrders.emplace_back(std::move(idStr), matchedVolume, price, placedTime, matchedTime, tradeSide);
  }

  std::ranges::sort(closedOrders);
  log::info("Retrieved {} closed orders from {}", closedOrders.size(), _exchangePublic.name());
  return closedOrders;
}

OpenedOrderVector HuobiPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params;

  MarketSet markets;

  if (openedOrdersConstraints.isCurDefined()) {
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.emplace_back("symbol", filterMarket.assetsPairStrLower());
    }
  }

  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order/openOrders", std::move(params));
  OpenedOrderVector openedOrders;
  const auto dataIt = data.find("data");
  if (dataIt == data.end()) {
    log::error("Unexpected opened orders query reply for {}", _exchangePublic.name());
    return openedOrders;
  }

  for (const json& orderDetails : *dataIt) {
    string marketStr = ToUpper(orderDetails["symbol"].get<std::string_view>());

    std::optional<Market> optMarket =
        _exchangePublic.determineMarketFromMarketStr(marketStr, markets, openedOrdersConstraints.cur1());

    if (!optMarket) {
      continue;
    }

    CurrencyCode volumeCur = optMarket->base();
    CurrencyCode priceCur = optMarket->quote();

    if (!openedOrdersConstraints.validateCur(volumeCur, priceCur)) {
      continue;
    }

    int64_t millisecondsSinceEpoch = orderDetails["created-at"].get<int64_t>();

    TimePoint placedTime{milliseconds(millisecondsSinceEpoch)};
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    int64_t idInt = orderDetails["id"].get<int64_t>();
    string id = ToString(idInt);
    if (!openedOrdersConstraints.validateId(id)) {
      continue;
    }

    MonetaryAmount originalVolume(orderDetails["amount"].get<std::string_view>(), volumeCur);
    MonetaryAmount matchedVolume(orderDetails["filled-amount"].get<std::string_view>(), volumeCur);
    MonetaryAmount remainingVolume = originalVolume - matchedVolume;
    MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
    TradeSide side =
        orderDetails["type"].get<std::string_view>().starts_with("buy") ? TradeSide::kBuy : TradeSide::kSell;

    openedOrders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

int HuobiPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.isOrderIdOnlyDependent()) {
    return batchCancel(openedOrdersConstraints.orderIdSet());
  }
  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);

  vector<OrderId> orderIds;
  orderIds.reserve(openedOrders.size());
  std::transform(std::make_move_iterator(openedOrders.begin()), std::make_move_iterator(openedOrders.end()),
                 std::back_inserter(orderIds), [](OpenedOrder&& order) -> OrderId&& { return std::move(order.id()); });
  return batchCancel(OrdersConstraints::OrderIdSet(std::move(orderIds)));
}

namespace {
Deposit::Status DepositStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "unknown") {
    return Deposit::Status::kInitial;
  }
  if (statusStr == "confirming") {
    return Deposit::Status::kProcessing;
  }
  if (statusStr == "confirmed" || statusStr == "safe" || statusStr == "orphan") {
    return Deposit::Status::kSuccess;
  }
  throw exception("Unexpected deposit status '{}' from Huobi", statusStr);
}
}  // namespace

DepositsSet HuobiPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.emplace_back("currency", ToLower(depositsConstraints.currencyCode().str()));
  }
  options.emplace_back("size", 500);
  options.emplace_back("type", "deposit");
  json data =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw", std::move(options));
  const auto dataIt = data.find("data");
  if (dataIt != data.end()) {
    for (const json& depositDetail : *dataIt) {
      std::string_view statusStr = depositDetail["state"].get<std::string_view>();
      int64_t id = depositDetail["id"].get<int64_t>();
      Deposit::Status status = DepositStatusFromStatusStr(statusStr);

      CurrencyCode currencyCode(depositDetail["currency"].get<std::string_view>());
      MonetaryAmount amount(depositDetail["amount"].get<double>(), currencyCode);
      int64_t millisecondsSinceEpoch = depositDetail["updated-at"].get<int64_t>();
      TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};
      if (!depositsConstraints.validateTime(timestamp)) {
        continue;
      }
      string idStr = ToString(id);
      if (!depositsConstraints.validateId(idStr)) {
        continue;
      }

      deposits.emplace_back(std::move(idStr), timestamp, amount, status);
    }
  }
  DepositsSet depositsSet(std::move(deposits));
  if (dataIt != data.end()) {
    log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  }
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr, bool logStatus) {
  if (statusStr == "verifying") {
    if (logStatus) {
      log::debug("Awaiting verification");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "failed") {
    if (logStatus) {
      log::error("Verification failed");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  if (statusStr == "submitted") {
    if (logStatus) {
      log::debug("Withdraw request submitted successfully");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "reexamine") {
    if (logStatus) {
      log::warn("Under examination for withdraw validation");
    }
    return Withdraw::Status::kProcessing;
  }
  // Let's also check without typo error ('canceled' with the typo is from the official documentation)
  if (statusStr == "canceled" || statusStr == "cancelled") {
    if (logStatus) {
      log::error("Withdraw canceled");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  if (statusStr == "pass") {
    if (logStatus) {
      log::debug("Withdraw validation passed");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "reject") {
    if (logStatus) {
      log::error("Withdraw validation rejected");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  if (statusStr == "pre-transfer") {
    if (logStatus) {
      log::debug("Withdraw is about to be released");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "wallet-transfer") {
    if (logStatus) {
      log::debug("On-chain transfer initiated");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "wallet-reject") {
    if (logStatus) {
      log::error("Transfer rejected by chain");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  if (statusStr == "confirmed") {
    if (logStatus) {
      log::debug("On-chain transfer completed with one confirmation");
    }
    return Withdraw::Status::kSuccess;
  }
  if (statusStr == "confirm-error") {
    if (logStatus) {
      log::error("On-chain transfer failed to get confirmation");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  if (statusStr == "repealed") {
    if (logStatus) {
      log::error("Withdraw terminated by system");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  throw exception("unknown status value '{}'", statusStr);
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options;
  if (withdrawsConstraints.isCurDefined()) {
    options.emplace_back("currency", ToLower(withdrawsConstraints.currencyCode().str()));
  }
  options.emplace_back("size", 500);
  options.emplace_back("type", "withdraw");
  return options;
}
}  // namespace

WithdrawsSet HuobiPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw",
                           CreateOptionsFromWithdrawConstraints(withdrawsConstraints));
  const auto dataIt = data.find("data");
  if (dataIt != data.end()) {
    for (const json& withdrawDetail : *dataIt) {
      std::string_view statusStr = withdrawDetail["state"].get<std::string_view>();
      int64_t id = withdrawDetail["id"].get<int64_t>();
      Withdraw::Status status = WithdrawStatusFromStatusStr(statusStr, withdrawsConstraints.isCurDefined());

      CurrencyCode currencyCode(withdrawDetail["currency"].get<std::string_view>());
      MonetaryAmount netEmittedAmount(withdrawDetail["amount"].get<double>(), currencyCode);
      MonetaryAmount fee(withdrawDetail["fee"].get<double>(), currencyCode);
      int64_t millisecondsSinceEpoch = withdrawDetail["updated-at"].get<int64_t>();
      TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};
      if (!withdrawsConstraints.validateTime(timestamp)) {
        continue;
      }
      string idStr = ToString(id);
      if (!withdrawsConstraints.validateId(idStr)) {
        continue;
      }

      withdraws.emplace_back(std::move(idStr), timestamp, netEmittedAmount, status, fee);
    }
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  if (dataIt != data.end()) {
    log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  }
  return withdrawsSet;
}

int HuobiPrivate::batchCancel(const OrdersConstraints::OrderIdSet& orderIdSet) {
  string csvOrderIdValues;

  int nbOrderIdPerRequest = 0;
  static constexpr std::string_view kBatchCancelEndpoint = "/v1/order/orders/batchcancel";
  for (const OrderId& orderId : orderIdSet) {
    csvOrderIdValues.append(orderId);
    csvOrderIdValues.push_back(CurlPostData::kArrayElemSepChar);
    static constexpr int kMaxNbOrdersPerRequest = 50;
    if (++nbOrderIdPerRequest == kMaxNbOrdersPerRequest) {
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, kBatchCancelEndpoint,
                   {{"order-ids", csvOrderIdValues}});
      csvOrderIdValues.clear();
      nbOrderIdPerRequest = 0;
    }
  }

  if (nbOrderIdPerRequest > 0) {
    PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, kBatchCancelEndpoint, {{"order-ids", csvOrderIdValues}});
  }
  return orderIdSet.size();
}

PlaceOrderInfo HuobiPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market mk = tradeInfo.tradeContext.mk;
  string lowerCaseMarket = mk.assetsPairStrLower();

  const bool placeSimulatedRealOrder = _exchangePublic.exchangeConfig().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  std::string_view type;
  if (isTakerStrategy) {
    type = fromCurrencyCode == mk.base() ? "sell-market" : "buy-market";
  } else {
    type = fromCurrencyCode == mk.base() ? "sell-limit" : "buy-limit";
  }

  HuobiPublic& huobiPublic = dynamic_cast<HuobiPublic&>(_exchangePublic);

  price = huobiPublic.sanitizePrice(mk, price);

  MonetaryAmount sanitizedVol = huobiPublic.sanitizeVolume(mk, fromCurrencyCode, volume, price, isTakerStrategy);
  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;
  if (volume < sanitizedVol && !isSimulationWithRealOrder) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode,
              sanitizedVol);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  CurlPostData placePostData{{"account-id", _accountIdCache.get()}, {"amount", volume.amountStr()}};
  if (isTakerStrategy) {
    if (fromCurrencyCode == mk.quote()) {
      // For buy-market, Huobi asks for the buy value, not the volume. Extract from documentation:
      // 'order size (for buy market order, it's order value)'
      placePostData.set("amount", from.amountStr());
    }
  } else {
    placePostData.emplace_back("price", price.amountStr());
  }
  placePostData.emplace_back("symbol", lowerCaseMarket);
  placePostData.emplace_back("type", type);

  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/order/orders/place", std::move(placePostData));

  auto dataIt = result.find("data");
  if (dataIt == result.end()) {
    log::error("Unable to retrieve order id");
  } else {
    placeOrderInfo.orderId = std::move(dataIt->get_ref<string&>());
  }

  return placeOrderInfo;
}

void HuobiPrivate::cancelOrderProcess(OrderIdView id) {
  string endpoint(kBaseUrlOrders);
  endpoint.append(id);
  endpoint.append("/submitcancel");
  PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, endpoint);
}

OrderInfo HuobiPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId);
  return queryOrderInfo(orderId, tradeContext);
}

OrderInfo HuobiPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();
  const Market mk = tradeContext.mk;

  string endpoint(kBaseUrlOrders);
  endpoint.append(orderId);

  const json res = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint);
  const auto dataIt = res.find("data");

  // Warning: I think Huobi's API has a typo with the 'filled' transformed into 'field' (even documentation is
  // ambiguous on this point). Let's handle both just to be sure.
  std::string_view filledAmount;
  std::string_view filledCashAmount;
  std::string_view filledFees;

  if (dataIt != res.end()) {
    if (dataIt->contains("field-amount")) {
      filledAmount = (*dataIt)["field-amount"].get<std::string_view>();
      filledCashAmount = (*dataIt)["field-cash-amount"].get<std::string_view>();
      filledFees = (*dataIt)["field-fees"].get<std::string_view>();
    } else {
      filledAmount = (*dataIt)["filled-amount"].get<std::string_view>();
      filledCashAmount = (*dataIt)["filled-cash-amount"].get<std::string_view>();
      filledFees = (*dataIt)["filled-fees"].get<std::string_view>();
    }
  }

  MonetaryAmount baseMatchedAmount(filledAmount, mk.base());
  MonetaryAmount quoteMatchedAmount(filledCashAmount, mk.quote());
  MonetaryAmount fromAmount = fromCurrencyCode == mk.base() ? baseMatchedAmount : quoteMatchedAmount;
  MonetaryAmount toAmount = fromCurrencyCode == mk.base() ? quoteMatchedAmount : baseMatchedAmount;

  // Fee is always in destination currency (according to Huobi documentation)
  MonetaryAmount fee(filledFees, toCurrencyCode);

  toAmount -= fee;

  std::string_view state = dataIt != res.end() ? (*dataIt)["state"].get<std::string_view>() : "";
  bool isClosed = state == "filled" || state == "partial-canceled" || state == "canceled";
  return OrderInfo(TradedAmounts(fromAmount, toAmount), isClosed);
}

InitiatedWithdrawInfo HuobiPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  string lowerCaseCur = ToLower(currencyCode.str());
  HuobiPublic& huobiPublic = dynamic_cast<HuobiPublic&>(_exchangePublic);

  json queryWithdrawAddressJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet,
                                               "/v2/account/withdraw/address", {{"currency", lowerCaseCur}});
  const auto addressDataIt = queryWithdrawAddressJson.find("data");
  if (addressDataIt == queryWithdrawAddressJson.end()) {
    throw exception("Unexpected reply from Huobi withdraw address");
  }
  std::string_view huobiWithdrawAddressName;
  for (const json& withdrawAddress : *addressDataIt) {
    std::string_view address(withdrawAddress["address"].get<std::string_view>());
    std::string_view addressTag(withdrawAddress["addressTag"].get<std::string_view>());
    if (address == destinationWallet.address() && addressTag == destinationWallet.tag()) {
      huobiWithdrawAddressName = withdrawAddress["note"].get<std::string_view>();
      break;
    }
  }
  if (huobiWithdrawAddressName.empty()) {
    throw exception("Address should be stored in your Huobi account manually in order to withdraw from API");
  }

  log::info("Found stored {} withdraw address '{}'", _exchangePublic.name(), huobiWithdrawAddressName);

  CurlPostData withdrawPostData;
  if (destinationWallet.hasTag()) {
    withdrawPostData.emplace_back("addr-tag", destinationWallet.tag());
  }
  withdrawPostData.emplace_back("address", destinationWallet.address());

  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFeeOrZero(currencyCode);
  HuobiPublic::WithdrawParams withdrawParams = huobiPublic.getWithdrawParams(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;
  if (!withdrawParams.minWithdrawAmt.isDefault() && netEmittedAmount < withdrawParams.minWithdrawAmt) {
    throw exception("Minimum withdraw amount for {} on Huobi is {}, cannot withdraw {}", currencyCode,
                    withdrawParams.minWithdrawAmt, netEmittedAmount);
  }
  if (!withdrawParams.maxWithdrawAmt.isDefault() && netEmittedAmount > withdrawParams.maxWithdrawAmt) {
    throw exception("Maximum withdraw amount for {} on Huobi is {}, cannot withdraw {}", currencyCode,
                    withdrawParams.maxWithdrawAmt, netEmittedAmount);
  }
  if (netEmittedAmount.nbDecimals() > withdrawParams.withdrawPrecision) {
    log::warn("Withdraw amount precision for Huobi is {} - truncating {}", withdrawParams.withdrawPrecision,
              netEmittedAmount);
    netEmittedAmount.truncate(withdrawParams.withdrawPrecision);
    grossAmount.truncate(withdrawParams.withdrawPrecision);
  }

  withdrawPostData.emplace_back("amount", netEmittedAmount.amountStr());
  withdrawPostData.emplace_back("currency", lowerCaseCur);
  // Strange to have the fee as input parameter of a withdraw...
  withdrawPostData.emplace_back("fee", withdrawFee.amountStr());

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/dw/withdraw/api/create",
                             std::move(withdrawPostData));
  const auto createDataIt = result.find("data");
  if (createDataIt == result.end()) {
    throw exception("Unexpected response from withdraw create for {}", huobiPublic.name());
  }
  string withdrawIdStr = ToString(createDataIt->get<int64_t>());
  return {std::move(destinationWallet), std::move(withdrawIdStr), grossAmount};
}

int HuobiPrivate::AccountIdFunc::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/account/accounts");
  const auto dataIt = result.find("data");
  if (dataIt == result.end()) {
    throw exception("Unexpected reply from account id query for Huobi");
  }
  for (const json& accDetails : *dataIt) {
    std::string_view state = accDetails["state"].get<std::string_view>();
    if (state == "working") {
      return accDetails["id"].get<int>();
    }
  }
  throw exception("Unable to find a working Huobi account");
}
}  // namespace cct::api
