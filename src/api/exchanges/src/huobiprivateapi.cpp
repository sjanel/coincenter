#include "huobiprivateapi.hpp"

#include <iterator>
#include <thread>

#include "apikey.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "huobipublicapi.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"

namespace cct::api {

namespace {

string BuildParamStr(HttpRequestType requestType, std::string_view baseUrl, std::string_view method,
                     std::string_view postDataStr) {
  std::string_view urlBaseWithoutHttps(baseUrl.begin() + std::string_view("https://").size(), baseUrl.end());

  string paramsStr(ToString(requestType));
  paramsStr.reserve(paramsStr.size() + urlBaseWithoutHttps.size() + method.size() + postDataStr.size() + 3U);
  paramsStr.push_back('\n');
  paramsStr.append(urlBaseWithoutHttps);
  paramsStr.push_back('\n');
  paramsStr.append(method);
  paramsStr.push_back('\n');
  paramsStr.append(postDataStr);
  return paramsStr;
}

json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostData&& postData = CurlPostData(), bool throwIfError = true) {
  CurlPostData signaturePostData{
      {"AccessKeyId", apiKey.key()},
      {"SignatureMethod", "HmacSHA256"},
      {"SignatureVersion", 2},
      {"Timestamp", curlHandle.urlEncode(Nonce_LiteralDate(kTimeYearToSecondTSeparatedFormat))}};

  CurlOptions::PostDataFormat postDataFormat = CurlOptions::PostDataFormat::kString;
  if (!postData.empty()) {
    if (requestType == HttpRequestType::kGet) {
      // Warning: Huobi expects that all parameters of the query are ordered lexicographically
      // We trust the caller for this. In case the order is not respected, error 'Signature not valid' will be returned
      // from Huobi
      signaturePostData.append(postData);
      postData = CurlPostData();
    } else {
      postDataFormat = CurlOptions::PostDataFormat::kJson;
    }
  }

  string sig = curlHandle.urlEncode(B64Encode(ssl::ShaBin(
      ssl::ShaType::kSha256, BuildParamStr(requestType, curlHandle.getNextBaseUrl(), endpoint, signaturePostData.str()),
      apiKey.privateKey())));

  signaturePostData.append("Signature", sig);

  string method(endpoint);
  method.push_back('?');
  method.append(signaturePostData.str());

  json ret = json::parse(
      curlHandle.query(method, CurlOptions(requestType, std::move(postData), HuobiPublic::kUserAgent, postDataFormat)));
  if (throwIfError) {
    auto statusIt = ret.find("status");
    if (statusIt != ret.end() && statusIt->get<std::string_view>() != "ok") {
      log::error("Full Huobi json error: '{}'", ret.dump());
      auto errIt = ret.find("err-msg");
      throw exception("Huobi error: {}", errIt == ret.end() ? "unknown" : errIt->get<std::string_view>());
    }
  }

  return ret;
}

}  // namespace

HuobiPrivate::HuobiPrivate(const CoincenterInfo& coincenterInfo, HuobiPublic& huobiPublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, huobiPublic, apiKey),
      _curlHandle(HuobiPublic::kURLBases, coincenterInfo.metricGatewayPtr(),
                  coincenterInfo.exchangeInfo(huobiPublic.name()).privateAPIRate(), coincenterInfo.getRunMode()),
      _accountIdCache(CachedResultOptions(std::chrono::hours(96), _cachedResultVault), _curlHandle, apiKey),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, huobiPublic) {}

bool HuobiPrivate::validateApiKey() {
  constexpr bool throwIfError = false;
  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/account/accounts", CurlPostData(), throwIfError);
  auto statusIt = result.find("status");
  return statusIt == result.end() || statusIt->get<std::string_view>() == "ok";
}

BalancePortfolio HuobiPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  string method = "/v1/account/accounts/";
  AppendString(method, _accountIdCache.get());
  method.append("/balance");
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, method);
  BalancePortfolio balancePortfolio;

  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  for (const json& balanceDetail : result["data"]["list"]) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    CurrencyCode currencyCode(balanceDetail["currency"].get<std::string_view>());
    MonetaryAmount amount(balanceDetail["balance"].get<std::string_view>(), currencyCode);
    if (typeStr == "trade" || (withBalanceInUse && typeStr == "frozen")) {
      this->addBalance(balancePortfolio, amount, equiCurrency);
    } else {
      log::debug("Do not consider {} as it is {} on {}", amount, typeStr, _exchangePublic.name());
    }
  }
  return balancePortfolio;
}

Wallet HuobiPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  string lowerCaseCur = ToLower(currencyCode.str());
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v2/account/deposit/address",
                           {{"currency", lowerCaseCur}})["data"];
  string address;
  std::string_view tag;
  ExchangeName exchangeName(_huobiPublic.name(), _apiKey.name());
  const CoincenterInfo& coincenterInfo = _huobiPublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(_huobiPublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  for (json& depositDetail : data) {
    tag = depositDetail["addressTag"].get<std::string_view>();

    std::string_view addressView = depositDetail["address"].get<std::string_view>();

    if (Wallet::ValidateWallet(walletCheck, exchangeName, currencyCode, addressView, tag)) {
      address = std::move(depositDetail["address"].get_ref<string&>());
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", addressView, tag);
    tag = std::string_view();
  }

  Wallet wallet(std::move(exchangeName), currencyCode, std::move(address), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

Orders HuobiPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params;

  MarketSet markets;

  if (openedOrdersConstraints.isCur1Defined()) {
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.append("symbol", filterMarket.assetsPairStrLower());
    }
  }

  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order/openOrders", std::move(params));
  Orders openedOrders;
  for (const json& orderDetails : data["data"]) {
    string marketStr = ToUpper(orderDetails["symbol"].get<std::string_view>());

    std::optional<Market> optMarket =
        _exchangePublic.determineMarketFromMarketStr(marketStr, markets, openedOrdersConstraints.cur1());

    CurrencyCode volumeCur;
    CurrencyCode priceCur;

    if (optMarket) {
      volumeCur = optMarket->base();
      priceCur = optMarket->quote();
      if (!openedOrdersConstraints.validateCur(volumeCur, priceCur)) {
        continue;
      }
    } else {
      continue;
    }

    int64_t millisecondsSinceEpoch = orderDetails["created-at"].get<int64_t>();

    TimePoint placedTime{std::chrono::milliseconds(millisecondsSinceEpoch)};
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    int64_t idInt = orderDetails["id"].get<int64_t>();
    string id = ToString(idInt);
    if (!openedOrdersConstraints.validateOrderId(id)) {
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
  Orders openedOrders = queryOpenedOrders(openedOrdersConstraints);

  vector<OrderId> orderIds;
  orderIds.reserve(openedOrders.size());
  std::transform(std::make_move_iterator(openedOrders.begin()), std::make_move_iterator(openedOrders.end()),
                 std::back_inserter(orderIds), [](Order&& order) -> OrderId&& { return std::move(order.id()); });
  return batchCancel(OrdersConstraints::OrderIdSet(std::move(orderIds)));
}

namespace {
Deposit::Status DepositStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "unknown" || statusStr == "confirming") {
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
    options.append("currency", ToLower(depositsConstraints.currencyCode().str()));
  }
  options.append("size", 500);
  options.append("type", "deposit");
  json depositJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw",
                                  std::move(options))["data"];
  for (const json& depositDetail : depositJson) {
    std::string_view statusStr = depositDetail["state"].get<std::string_view>();
    int64_t id = depositDetail["id"].get<int64_t>();
    Deposit::Status status = DepositStatusFromStatusStr(statusStr);

    CurrencyCode currencyCode(depositDetail["currency"].get<std::string_view>());
    MonetaryAmount amount(depositDetail["amount"].get<double>(), currencyCode);
    int64_t millisecondsSinceEpoch = depositDetail["updated-at"].get<int64_t>();
    TimePoint timestamp{std::chrono::milliseconds(millisecondsSinceEpoch)};
    if (!depositsConstraints.validateTime(timestamp)) {
      continue;
    }
    string idStr = ToString(id);
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
  if (statusStr == "canceled") {
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
    options.append("currency", ToLower(withdrawsConstraints.currencyCode().str()));
  }
  options.append("size", 500);
  options.append("type", "withdraw");
  return options;
}
}  // namespace

WithdrawsSet HuobiPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;
  json withdrawJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw",
                                   CreateOptionsFromWithdrawConstraints(withdrawsConstraints))["data"];
  for (const json& withdrawDetail : withdrawJson) {
    std::string_view statusStr = withdrawDetail["state"].get<std::string_view>();
    int64_t id = withdrawDetail["id"].get<int64_t>();
    Withdraw::Status status = WithdrawStatusFromStatusStr(statusStr, withdrawsConstraints.isCurDefined());

    CurrencyCode currencyCode(withdrawDetail["currency"].get<std::string_view>());
    MonetaryAmount netEmittedAmount(withdrawDetail["amount"].get<double>(), currencyCode);
    MonetaryAmount fee(withdrawDetail["fee"].get<double>(), currencyCode);
    int64_t millisecondsSinceEpoch = withdrawDetail["updated-at"].get<int64_t>();
    TimePoint timestamp{std::chrono::milliseconds(millisecondsSinceEpoch)};
    if (!withdrawsConstraints.validateTime(timestamp)) {
      continue;
    }
    string idStr = ToString(id);
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

  const bool placeSimulatedRealOrder = _exchangePublic.exchangeInfo().placeSimulateRealOrder();
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
    placePostData.append("price", price.amountStr());
  }
  placePostData.append("symbol", lowerCaseMarket);
  placePostData.append("type", type);

  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/order/orders/place", std::move(placePostData));
  placeOrderInfo.orderId = std::move(result["data"].get_ref<string&>());
  return placeOrderInfo;
}

void HuobiPrivate::cancelOrderProcess(OrderIdView id) {
  string endpoint = "/v1/order/orders/";
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
  string endpoint = "/v1/order/orders/";
  endpoint.append(orderId);

  json res = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint);
  const json& data = res["data"];
  // Warning: I think Huobi's API has a typo with the 'filled' transformed into 'field' (even documentation is
  // ambiguous on this point). Let's handle both just to be sure.
  std::string_view filledAmount;
  std::string_view filledCashAmount;
  std::string_view filledFees;
  if (data.contains("field-amount")) {
    filledAmount = data["field-amount"].get<std::string_view>();
    filledCashAmount = data["field-cash-amount"].get<std::string_view>();
    filledFees = data["field-fees"].get<std::string_view>();
  } else {
    filledAmount = data["filled-amount"].get<std::string_view>();
    filledCashAmount = data["filled-cash-amount"].get<std::string_view>();
    filledFees = data["filled-fees"].get<std::string_view>();
  }

  MonetaryAmount baseMatchedAmount(filledAmount, mk.base());
  MonetaryAmount quoteMatchedAmount(filledCashAmount, mk.quote());
  MonetaryAmount fromAmount = fromCurrencyCode == mk.base() ? baseMatchedAmount : quoteMatchedAmount;
  MonetaryAmount toAmount = fromCurrencyCode == mk.base() ? quoteMatchedAmount : baseMatchedAmount;
  // Fee is always in destination currency (according to Huobi documentation)
  MonetaryAmount fee(filledFees, toCurrencyCode);
  toAmount -= fee;
  std::string_view state = data["state"].get<std::string_view>();
  bool isClosed = state == "filled" || state == "partial-canceled" || state == "canceled";
  return OrderInfo(TradedAmounts(fromAmount, toAmount), isClosed);
}

InitiatedWithdrawInfo HuobiPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  string lowerCaseCur = ToLower(currencyCode.str());
  HuobiPublic& huobiPublic = dynamic_cast<HuobiPublic&>(_exchangePublic);

  json queryWithdrawAddressJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet,
                                               "/v2/account/withdraw/address", {{"currency", lowerCaseCur}});
  std::string_view huobiWithdrawAddressName;
  for (const json& withdrawAddress : queryWithdrawAddressJson["data"]) {
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
    withdrawPostData.append("addr-tag", destinationWallet.tag());
  }
  withdrawPostData.append("address", destinationWallet.address());

  MonetaryAmount fee(_exchangePublic.queryWithdrawalFee(currencyCode));
  HuobiPublic::WithdrawParams withdrawParams = huobiPublic.getWithdrawParams(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - fee;
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

  withdrawPostData.append("amount", netEmittedAmount.amountStr());
  withdrawPostData.append("currency", lowerCaseCur);
  // Strange to have the fee as input parameter of a withdraw...
  withdrawPostData.append("fee", fee.amountStr());

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/dw/withdraw/api/create",
                             std::move(withdrawPostData));
  string withdrawIdStr = ToString(result["data"].get<int64_t>());
  return {std::move(destinationWallet), std::move(withdrawIdStr), grossAmount};
}

int HuobiPrivate::AccountIdFunc::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/account/accounts");
  for (const json& accDetails : result["data"]) {
    std::string_view state = accDetails["state"].get<std::string_view>();
    if (state == "working") {
      return accDetails["id"].get<int>();
    }
  }
  throw exception("Unable to find a working Huobi account");
}
}  // namespace cct::api
