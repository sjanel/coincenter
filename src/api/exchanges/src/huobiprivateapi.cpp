#include "huobiprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
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
#include "cct_cctype.hpp"
#include "cct_exception.hpp"
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
#include "huobi-schema.hpp"
#include "huobipublicapi.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "query-retry-policy.hpp"
#include "request-retry.hpp"
#include "ssl_sha.hpp"
#include "stringconv.hpp"
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
  const std::string_view urlBaseWithoutHttps(baseUrl.begin() + std::string_view("https://").size(), baseUrl.end());
  const auto requestTypeStr = HttpRequestTypeToString(requestType);

  string paramsStr(requestTypeStr.size() + urlBaseWithoutHttps.size() + method.size() + postDataStr.size() + 3U, '\n');

  auto it = std::ranges::copy(requestTypeStr, paramsStr.data()).out;
  it = std::ranges::copy(urlBaseWithoutHttps, it + 1).out;
  it = std::ranges::copy(method, it + 1).out;

  std::ranges::copy(postDataStr, it + 1);

  return paramsStr;
}

auto ComputePostDataFormat(HttpRequestType requestType, const CurlPostData& postData) {
  CurlOptions::PostDataFormat postDataFormat = CurlOptions::PostDataFormat::kString;
  if (!postData.empty() && requestType != HttpRequestType::kGet) {
    postDataFormat = CurlOptions::PostDataFormat::json;
  }
  return postDataFormat;
}

void SetNonceAndSignature(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType,
                          std::string_view endpoint, CurlPostData& postData, CurlPostData& signaturePostData) {
  auto isNotEncoded = [](char ch) { return isalnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~'; };

  static constexpr std::string_view kSignatureKey = "Signature";

  signaturePostData.set("Timestamp", URLEncode(Nonce_LiteralDate(kTimeYearToSecondTSeparatedFormat), isNotEncoded));

  if (!postData.empty() && requestType == HttpRequestType::kGet) {
    // Warning: Huobi expects that all parameters of the query are ordered lexicographically
    // We trust the caller for this. In case the order is not respected, error 'Signature not valid' will be
    // returned from Huobi
    signaturePostData.append(postData);
    postData.clear();
  } else if (signaturePostData.back().key() == kSignatureKey) {
    // signature needs to be erased (if we had an error) before computing the sha256
    signaturePostData.pop_back();
  }

  signaturePostData.emplace_back(
      kSignatureKey, URLEncode(B64Encode(ssl::Sha256Bin(BuildParamStr(requestType, curlHandle.getNextBaseUrl(),
                                                                      endpoint, signaturePostData.str()),
                                                        apiKey.privateKey())),
                               isNotEncoded));
}

template <class T>
T PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
               CurlPostData&& postData = CurlPostData()) {
  CurlPostData signaturePostData{
      {"AccessKeyId", apiKey.key()}, {"SignatureMethod", "HmacSHA256"}, {"SignatureVersion", 2}};

  string method(endpoint.size() + 1U, '?');

  std::memcpy(method.data(), endpoint.data(), endpoint.size());

  CurlOptions::PostDataFormat postDataFormat = ComputePostDataFormat(requestType, postData);

  RequestRetry requestRetry(curlHandle, CurlOptions(requestType, std::move(postData), postDataFormat),
                            QueryRetryPolicy{.initialRetryDelay = seconds{1}, .nbMaxRetries = 3});
  return requestRetry.query<T>(
      method,
      [](const T& response) {
        if constexpr (amc::is_detected<schema::huobi::has_code_t, T>::value) {
          if (response.code != 200) {
            log::warn("Huobi error code: {}", response.code);
            return RequestRetry::Status::kResponseError;
          }
        } else if constexpr (amc::is_detected<schema::huobi::has_status_t, T>::value) {
          if (response.status != "ok") {
            if (response.status.empty()) {
              log::warn("Huobi status is empty - is it supposed to be returned by this endpoint?");
            } else {
              log::warn("Huobi status error: {}", response.status);
              return RequestRetry::Status::kResponseError;
            }
          }
        } else {
          // TODO: can be replaced by static_assert(false) in C++23
          static_assert(amc::is_detected<schema::huobi::has_code_t, T>::value ||
                            amc::is_detected<schema::huobi::has_status_t, T>::value,
                        "T should have a code or status member");
        }
        return RequestRetry::Status::kResponseOK;
      },
      [&signaturePostData, &curlHandle, &apiKey, requestType, endpoint, &method](CurlOptions& opts) {
        SetNonceAndSignature(curlHandle, apiKey, requestType, endpoint, opts.mutablePostData(), signaturePostData);

        method.replace(method.begin() + endpoint.size() + 1U, method.end(), signaturePostData.str());
      });
}

constexpr std::string_view kBaseUrlOrders = "/v1/order/orders/";

}  // namespace

HuobiPrivate::HuobiPrivate(const CoincenterInfo& coincenterInfo, HuobiPublic& huobiPublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, huobiPublic, apiKey),
      _curlHandle(HuobiPublic::kURLBases, coincenterInfo.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  coincenterInfo.getRunMode()),
      _accountIdCache(CachedResultOptions(std::chrono::hours(48), _cachedResultVault), _curlHandle, apiKey),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::depositWallet).duration,
                              _cachedResultVault),
          _curlHandle, _apiKey, huobiPublic) {}

bool HuobiPrivate::validateApiKey() {
  const auto result = PrivateQuery<schema::huobi::V1AccountAccounts>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                                     "/v1/account/accounts", CurlPostData());

  return result.status == "ok" && !result.data.empty();
}

BalancePortfolio HuobiPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  const auto method = fmt::format("/v1/account/accounts/{}/balance", _accountIdCache.get());
  const auto result =
      PrivateQuery<schema::huobi::V1AccountAccountsBalance>(_curlHandle, _apiKey, HttpRequestType::kGet, method);
  const bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;

  BalancePortfolio balancePortfolio;
  for (const auto& balanceDetail : result.data.list) {
    if (balanceDetail.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the balance", balanceDetail.currency,
                _exchangePublic.name());
      continue;
    }
    MonetaryAmount amount(balanceDetail.balance, CurrencyCode(balanceDetail.currency));
    if (balanceDetail.type == "trade" || (withBalanceInUse && balanceDetail.type == "frozen")) {
      balancePortfolio += amount;
    } else {
      log::trace("Do not consider {} as it is {} on {}", amount, balanceDetail.type, _exchangePublic.name());
    }
  }

  return balancePortfolio;
}

Wallet HuobiPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  string lowerCaseCur = ToLower(currencyCode.str());
  auto result = PrivateQuery<schema::huobi::V2AccountDepositAddress>(
      _curlHandle, _apiKey, HttpRequestType::kGet, "/v2/account/deposit/address", {{"currency", lowerCaseCur}});

  string address;
  std::string_view tag;
  ExchangeName exchangeName(_huobiPublic.exchangeNameEnum(), _apiKey.name());
  const CoincenterInfo& coincenterInfo = _huobiPublic.coincenterInfo();
  bool doCheckWallet =
      coincenterInfo.exchangeConfig(_huobiPublic.exchangeNameEnum()).withdraw.validateDepositAddressesInFile;
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  for (auto& depositDetail : result.data) {
    tag = depositDetail.addressTag;

    if (Wallet::ValidateWallet(walletCheck, exchangeName, currencyCode, depositDetail.address, tag)) {
      address = std::move(depositDetail.address);
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", depositDetail.address, tag);
    tag = std::string_view();
  }

  Wallet wallet(std::move(exchangeName), currencyCode, std::move(address), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
TradeSide TradeSideFromTypeStr(std::string_view typeSide) {
  if (typeSide.starts_with("buy")) {
    return TradeSide::buy;
  }
  if (typeSide.starts_with("sell")) {
    return TradeSide::sell;
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

  const auto result = PrivateQuery<schema::huobi::V1Orders>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                            closedOrdersEndpoint, std::move(params));

  MarketSet markets;

  for (const auto& orderDetails : result.data) {
    string marketStr = ToUpper(orderDetails.symbol);

    std::optional<Market> optMarket =
        _exchangePublic.determineMarketFromMarketStr(marketStr, markets, closedOrdersConstraints.cur1());

    if (!optMarket) {
      continue;
    }

    if (!closedOrdersConstraints.validateCur(optMarket->base(), optMarket->quote())) {
      continue;
    }

    TimePoint placedTime{milliseconds(orderDetails.createdAt)};

    string idStr = IntegralToString(orderDetails.id);

    if (!closedOrdersConstraints.validateId(idStr)) {
      continue;
    }

    TimePoint matchedTime{milliseconds(orderDetails.finishedAt)};

    // 'field' seems to be a typo here (instead of 'filled), but it's really sent by Huobi like that.
    MonetaryAmount matchedVolume(orderDetails.fieldAmount, optMarket->base());
    if (matchedVolume == 0) {
      continue;
    }
    MonetaryAmount price(orderDetails.price, optMarket->quote());

    TradeSide tradeSide = TradeSideFromTypeStr(orderDetails.type);

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

  auto result = PrivateQuery<schema::huobi::V1OrderOpenOrders>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                               "/v1/order/openOrders", std::move(params));
  OpenedOrderVector openedOrders;

  for (const auto& orderDetails : result.data) {
    string marketStr = ToUpper(orderDetails.symbol);

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

    int64_t millisecondsSinceEpoch = orderDetails.createdAt;

    TimePoint placedTime{milliseconds(millisecondsSinceEpoch)};
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    int64_t idInt = orderDetails.id;
    string id = IntegralToString(idInt);
    if (!openedOrdersConstraints.validateId(id)) {
      continue;
    }

    MonetaryAmount originalVolume(orderDetails.amount, volumeCur);
    MonetaryAmount matchedVolume(orderDetails.filledAmount, volumeCur);
    MonetaryAmount remainingVolume = originalVolume - matchedVolume;
    MonetaryAmount price(orderDetails.price, priceCur);
    TradeSide side = orderDetails.type.starts_with("buy") ? TradeSide::buy : TradeSide::sell;

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
    return Deposit::Status::initial;
  }
  if (statusStr == "confirming") {
    return Deposit::Status::processing;
  }
  if (statusStr == "confirmed" || statusStr == "safe" || statusStr == "orphan") {
    return Deposit::Status::success;
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

  const auto result = PrivateQuery<schema::huobi::V1QueryDepositWithdraw>(
      _curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw", std::move(options));
  for (const auto& depositDetail : result.data) {
    Deposit::Status status = DepositStatusFromStatusStr(depositDetail.state);

    if (depositDetail.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the deposits", depositDetail.currency,
                exchangeName());
      continue;
    }

    CurrencyCode currencyCode(depositDetail.currency);
    MonetaryAmount amount(depositDetail.amount, currencyCode);
    TimePoint timestamp{milliseconds(depositDetail.updatedAt)};
    if (!depositsConstraints.validateTime(timestamp)) {
      continue;
    }
    string idStr = IntegralToString(depositDetail.id);
    if (!depositsConstraints.validateId(idStr)) {
      continue;
    }

    deposits.emplace_back(std::move(idStr), timestamp, amount, status);
  }

  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr, bool logStatus) {
  if (statusStr == "verifying") {
    if (logStatus) {
      log::debug("Awaiting verification");
    }
    return Withdraw::Status::processing;
  }
  if (statusStr == "failed") {
    if (logStatus) {
      log::error("Verification failed");
    }
    return Withdraw::Status::failed;
  }
  if (statusStr == "submitted") {
    if (logStatus) {
      log::debug("Withdraw request submitted successfully");
    }
    return Withdraw::Status::processing;
  }
  if (statusStr == "reexamine") {
    if (logStatus) {
      log::warn("Under examination for withdraw validation");
    }
    return Withdraw::Status::processing;
  }
  // Let's also check without typo error ('canceled' with the typo is from the official documentation)
  if (statusStr == "canceled" || statusStr == "cancelled") {
    if (logStatus) {
      log::error("Withdraw canceled");
    }
    return Withdraw::Status::failed;
  }
  if (statusStr == "pass") {
    if (logStatus) {
      log::debug("Withdraw validation passed");
    }
    return Withdraw::Status::processing;
  }
  if (statusStr == "reject") {
    if (logStatus) {
      log::error("Withdraw validation rejected");
    }
    return Withdraw::Status::failed;
  }
  if (statusStr == "pre-transfer") {
    if (logStatus) {
      log::debug("Withdraw is about to be released");
    }
    return Withdraw::Status::processing;
  }
  if (statusStr == "wallet-transfer") {
    if (logStatus) {
      log::debug("On-chain transfer initiated");
    }
    return Withdraw::Status::processing;
  }
  if (statusStr == "wallet-reject") {
    if (logStatus) {
      log::error("Transfer rejected by chain");
    }
    return Withdraw::Status::failed;
  }
  if (statusStr == "confirmed") {
    if (logStatus) {
      log::debug("On-chain transfer completed with one confirmation");
    }
    return Withdraw::Status::success;
  }
  if (statusStr == "confirm-error") {
    if (logStatus) {
      log::error("On-chain transfer failed to get confirmation");
    }
    return Withdraw::Status::failed;
  }
  if (statusStr == "repealed") {
    if (logStatus) {
      log::error("Withdraw terminated by system");
    }
    return Withdraw::Status::failed;
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
  const auto result = PrivateQuery<schema::huobi::V1QueryDepositWithdraw>(
      _curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw",
      CreateOptionsFromWithdrawConstraints(withdrawsConstraints));
  for (const auto& withdrawDetail : result.data) {
    if (withdrawDetail.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the withdraws", withdrawDetail.currency,
                exchangeName());
      continue;
    }
    Withdraw::Status status = WithdrawStatusFromStatusStr(withdrawDetail.state, withdrawsConstraints.isCurDefined());
    CurrencyCode currencyCode(withdrawDetail.currency);
    MonetaryAmount netEmittedAmount(withdrawDetail.amount, currencyCode);
    MonetaryAmount fee(withdrawDetail.fee, currencyCode);
    TimePoint timestamp{milliseconds(withdrawDetail.updatedAt)};
    if (!withdrawsConstraints.validateTime(timestamp)) {
      continue;
    }
    string idStr = IntegralToString(withdrawDetail.id);
    if (!withdrawsConstraints.validateId(idStr)) {
      continue;
    }

    withdraws.emplace_back(std::move(idStr), timestamp, netEmittedAmount, status, fee);
  }

  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
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
      PrivateQuery<schema::huobi::V1OrderOrdersBatchCancel>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                            kBatchCancelEndpoint, {{"order-ids", csvOrderIdValues}});
      csvOrderIdValues.clear();
      nbOrderIdPerRequest = 0;
    }
  }

  if (nbOrderIdPerRequest > 0) {
    PrivateQuery<schema::huobi::V1OrderOrdersBatchCancel>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                          kBatchCancelEndpoint, {{"order-ids", csvOrderIdValues}});
  }
  return orderIdSet.size();
}

PlaceOrderInfo HuobiPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market mk = tradeInfo.tradeContext.market;
  string lowerCaseMarket = mk.assetsPairStrLower();

  const bool placeSimulatedRealOrder = _exchangePublic.exchangeConfig().query.placeSimulateRealOrder;
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

  auto result = PrivateQuery<schema::huobi::V1OrderOrdersPlace>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                                "/v1/order/orders/place", std::move(placePostData));

  if (result.data.empty()) {
    log::error("Unable to retrieve order id");
  } else {
    placeOrderInfo.orderId = std::move(result.data);
  }

  return placeOrderInfo;
}

void HuobiPrivate::cancelOrderProcess(OrderIdView id) {
  static constexpr std::string_view kSubmitCancelSuffix = "/submitcancel";

  string endpoint(kBaseUrlOrders.size() + id.size() + kSubmitCancelSuffix.size(), '\0');

  auto it = std::ranges::copy(kBaseUrlOrders, endpoint.begin()).out;
  it = std::ranges::copy(id, it).out;
  it = std::ranges::copy(kSubmitCancelSuffix, it).out;

  PrivateQuery<schema::huobi::V1OrderOrdersSubmitCancel>(_curlHandle, _apiKey, HttpRequestType::kPost, endpoint);
}

OrderInfo HuobiPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId);
  return queryOrderInfo(orderId, tradeContext);
}

OrderInfo HuobiPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();
  const Market mk = tradeContext.market;

  string endpoint(kBaseUrlOrders);
  endpoint.append(orderId);

  const auto result =
      PrivateQuery<schema::huobi::V1OrderOrdersDetail>(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint);

  // Warning: I think Huobi's API has a typo with the 'filled' transformed into 'field' (even documentation is
  // ambiguous on this point). Let's handle both just to be sure.
  const MonetaryAmount* pFilledAmount;
  const MonetaryAmount* pFilledCashAmount;
  const MonetaryAmount* pFilledFees;

  if (!result.data.fieldAmount.isDefault()) {
    pFilledAmount = &result.data.fieldAmount;
    pFilledCashAmount = &result.data.fieldCashAmount;
    pFilledFees = &result.data.fieldFees;
  } else {
    pFilledAmount = &result.data.filledAmount;
    pFilledCashAmount = &result.data.filledCashAmount;
    pFilledFees = &result.data.filledFees;
  }

  MonetaryAmount baseMatchedAmount(*pFilledAmount, mk.base());
  MonetaryAmount quoteMatchedAmount(*pFilledCashAmount, mk.quote());
  MonetaryAmount fromAmount = fromCurrencyCode == mk.base() ? baseMatchedAmount : quoteMatchedAmount;
  MonetaryAmount toAmount = fromCurrencyCode == mk.base() ? quoteMatchedAmount : baseMatchedAmount;

  // Fee is always in destination currency (according to Huobi documentation)
  MonetaryAmount fee(*pFilledFees, toCurrencyCode);

  toAmount -= fee;

  std::string_view state = result.data.state;
  bool isClosed = state == "filled" || state == "partial-canceled" || state == "canceled";
  return OrderInfo(TradedAmounts(fromAmount, toAmount), isClosed);
}

InitiatedWithdrawInfo HuobiPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  string lowerCaseCur = ToLower(currencyCode.str());
  HuobiPublic& huobiPublic = dynamic_cast<HuobiPublic&>(_exchangePublic);

  const auto resultWithdrawAddress = PrivateQuery<schema::huobi::V1QueryWithdrawAddress>(
      _curlHandle, _apiKey, HttpRequestType::kGet, "/v2/account/withdraw/address", {{"currency", lowerCaseCur}});
  std::string_view huobiWithdrawAddressName;
  for (const auto& withdrawAddress : resultWithdrawAddress.data) {
    if (withdrawAddress.address == destinationWallet.address() &&
        withdrawAddress.addressTag == destinationWallet.tag()) {
      huobiWithdrawAddressName = withdrawAddress.note;
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

  const auto result = PrivateQuery<schema::huobi::V1DwWithdrawApiCreate>(
      _curlHandle, _apiKey, HttpRequestType::kPost, "/v1/dw/withdraw/api/create", std::move(withdrawPostData));
  if (result.data == 0) {
    throw exception("Unexpected response from withdraw create for {}", huobiPublic.name());
  }
  return {std::move(destinationWallet), IntegralToString(result.data), grossAmount};
}

int64_t HuobiPrivate::AccountIdFunc::operator()() {
  const auto result = PrivateQuery<schema::huobi::V1AccountAccounts>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                                     "/v1/account/accounts");
  const auto it =
      std::ranges::find_if(result.data, [](const auto& accDetails) { return accDetails.state == "working"; });
  if (it != result.data.end()) {
    return it->id;
  }
  throw exception("Unable to find a working Huobi account");
}
}  // namespace cct::api
