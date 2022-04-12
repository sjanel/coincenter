#include "huobiprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "huobipublicapi.hpp"
#include "recentdeposit.hpp"
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
                  CurlPostData&& postdata = CurlPostData()) {
  Nonce nonce = Nonce_LiteralDate("%Y-%m-%dT%H:%M:%S");
  string encodedNonce = curlHandle.urlEncode(nonce);

  CurlPostData signaturePostdata;

  signaturePostdata.append("AccessKeyId", apiKey.key());
  signaturePostdata.append("SignatureMethod", "HmacSHA256");
  signaturePostdata.append("SignatureVersion", 2);
  signaturePostdata.append("Timestamp", std::move(encodedNonce));

  CurlOptions::PostDataFormat postDataFormat = CurlOptions::PostDataFormat::kString;
  if (!postdata.empty()) {
    if (requestType == HttpRequestType::kGet) {
      signaturePostdata.append(std::move(postdata));
      // After a move, an object has unspecified but valid state. Let's call clear() to force it to be empty.
      postdata.clear();
    } else {
      postDataFormat = CurlOptions::PostDataFormat::kJson;
    }
  }

  string sig = curlHandle.urlEncode(B64Encode(ssl::ShaBin(
      ssl::ShaType::kSha256, BuildParamStr(requestType, curlHandle.getNextBaseUrl(), endpoint, signaturePostdata.str()),
      apiKey.privateKey())));

  signaturePostdata.append("Signature", sig);

  string method(endpoint);
  method.push_back('?');
  method.append(signaturePostdata.str());

  json ret = json::parse(
      curlHandle.query(method, CurlOptions(requestType, std::move(postdata), HuobiPublic::kUserAgent, postDataFormat)));
  auto statusIt = ret.find("status");
  if (statusIt != ret.end() && statusIt->get<std::string_view>() != "ok") {
    log::error("Full Huobi json error: '{}'", ret.dump());
    string errMsg("Huobi error: ");
    auto errIt = ret.find("err-msg");
    if (errIt == ret.end()) {
      errMsg.append("unknown");
    } else {
      errMsg.append(errIt->get<std::string_view>());
    }
    throw exception(std::move(errMsg));
  }

  return ret;
}

}  // namespace

HuobiPrivate::HuobiPrivate(const CoincenterInfo& config, HuobiPublic& huobiPublic, const APIKey& apiKey)
    : ExchangePrivate(config, huobiPublic, apiKey),
      _curlHandle(HuobiPublic::kURLBases, config.metricGatewayPtr(),
                  config.exchangeInfo(huobiPublic.name()).privateAPIRate(), config.getRunMode()),
      _accountIdCache(CachedResultOptions(std::chrono::hours(96), _cachedResultVault), _curlHandle, apiKey),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, huobiPublic) {}

BalancePortfolio HuobiPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  string method = "/v1/account/accounts/";
  AppendString(method, _accountIdCache.get());
  method.append("/balance");
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, method);
  BalancePortfolio balancePortfolio;
  for (const json& balanceDetail : result["data"]["list"]) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    CurrencyCode currencyCode(balanceDetail["currency"].get<std::string_view>());
    MonetaryAmount amount(balanceDetail["balance"].get<std::string_view>(), currencyCode);
    if (typeStr == "trade") {
      this->addBalance(balancePortfolio, amount, equiCurrency);
    } else {
      log::debug("Do not consider {} as it is {} on {}", amount.str(), typeStr, _exchangePublic.name());
    }
  }
  return balancePortfolio;
}

Wallet HuobiPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  string lowerCaseCur = ToLower(currencyCode.str());
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v2/account/deposit/address",
                             {{"currency", lowerCaseCur}});
  std::string_view address, tag;
  ExchangeName exchangeName(_huobiPublic.name(), _apiKey.name());
  const CoincenterInfo& coincenterInfo = _huobiPublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(_huobiPublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  for (const json& depositDetail : result["data"]) {
    address = depositDetail["address"].get<std::string_view>();
    tag = depositDetail["addressTag"].get<std::string_view>();

    if (Wallet::ValidateWallet(walletCheck, exchangeName, currencyCode, address, tag)) {
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
    address = std::string_view();
    tag = std::string_view();
  }

  Wallet w(std::move(exchangeName), currencyCode, address, tag, walletCheck);
  log::info("Retrieved {}", w.str());
  return w;
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

void HuobiPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.isOrderIdOnlyDependent()) {
    batchCancel(openedOrdersConstraints.orderIdSet());
    return;
  }
  Orders openedOrders = queryOpenedOrders(openedOrdersConstraints);

  vector<OrderId> orderIds;
  orderIds.reserve(openedOrders.size());
  std::transform(std::make_move_iterator(openedOrders.begin()), std::make_move_iterator(openedOrders.end()),
                 std::back_inserter(orderIds), [](Order&& order) -> OrderId&& { return std::move(order.id()); });
  batchCancel(OrdersConstraints::OrderIdSet(std::move(orderIds)));
}

void HuobiPrivate::batchCancel(const OrdersConstraints::OrderIdSet& orderIdSet) {
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
}

PlaceOrderInfo HuobiPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.toCur());

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market m = tradeInfo.m;
  string lowerCaseMarket = m.assetsPairStrLower();

  const bool placeSimulatedRealOrder = _exchangePublic.exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  std::string_view type;
  if (isTakerStrategy) {
    type = fromCurrencyCode == m.base() ? "sell-market" : "buy-market";
  } else {
    type = fromCurrencyCode == m.base() ? "sell-limit" : "buy-limit";
  }

  HuobiPublic& huobiPublic = dynamic_cast<HuobiPublic&>(_exchangePublic);

  price = huobiPublic.sanitizePrice(m, price);

  MonetaryAmount sanitizedVol = huobiPublic.sanitizeVolume(m, fromCurrencyCode, volume, price, isTakerStrategy);
  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;
  if (volume < sanitizedVol && !isSimulationWithRealOrder) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(), toCurrencyCode.str(),
              sanitizedVol.str());
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  CurlPostData placePostData{{"account-id", _accountIdCache.get()}, {"amount", volume.amountStr()}};
  if (isTakerStrategy) {
    if (fromCurrencyCode == m.quote()) {
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

void HuobiPrivate::cancelOrderProcess(const OrderId& id) {
  string endpoint = "/v1/order/orders/";
  endpoint.append(id);
  endpoint.append("/submitcancel");
  PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, endpoint);
}

OrderInfo HuobiPrivate::cancelOrder(const OrderRef& orderRef) {
  cancelOrderProcess(orderRef.id);
  return queryOrderInfo(orderRef);
}

OrderInfo HuobiPrivate::queryOrderInfo(const OrderRef& orderRef) {
  const CurrencyCode fromCurrencyCode = orderRef.fromCur();
  const CurrencyCode toCurrencyCode = orderRef.toCur();
  const Market m = orderRef.m;
  string endpoint = "/v1/order/orders/";
  endpoint.append(orderRef.id);

  json res = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint);
  const json& data = res["data"];
  // Warning: I think Huobi's API has a typo with the 'filled' transformed into 'field' (even documentation is
  // ambiguous on this point). Let's handle both just to be sure.
  std::string_view filledAmount, filledCashAmount, filledFees;
  if (data.contains("field-amount")) {
    filledAmount = data["field-amount"].get<std::string_view>();
    filledCashAmount = data["field-cash-amount"].get<std::string_view>();
    filledFees = data["field-fees"].get<std::string_view>();
  } else {
    filledAmount = data["filled-amount"].get<std::string_view>();
    filledCashAmount = data["filled-cash-amount"].get<std::string_view>();
    filledFees = data["filled-fees"].get<std::string_view>();
  }

  MonetaryAmount baseMatchedAmount(filledAmount, m.base());
  MonetaryAmount quoteMatchedAmount(filledCashAmount, m.quote());
  MonetaryAmount fromAmount = fromCurrencyCode == m.base() ? baseMatchedAmount : quoteMatchedAmount;
  MonetaryAmount toAmount = fromCurrencyCode == m.base() ? quoteMatchedAmount : baseMatchedAmount;
  // Fee is always in destination currency (according to Huobi documentation)
  MonetaryAmount fee(filledFees, toCurrencyCode);
  toAmount -= fee;
  std::string_view state = data["state"].get<std::string_view>();
  bool isClosed = state == "filled" || state == "partial-canceled" || state == "canceled";
  return OrderInfo(TradedAmounts(fromAmount, toAmount), isClosed);
}

InitiatedWithdrawInfo HuobiPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  string lowerCaseCur = ToLower(currencyCode.str());

  json queryWithdrawAddressJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet,
                                               "/v2/account/withdraw/address", {{"currency", lowerCaseCur}});
  std::string_view huobiWithdrawAddressName;
  for (const json& withdrawAddress : queryWithdrawAddressJson["data"]) {
    std::string_view address(withdrawAddress["address"].get<std::string_view>());
    std::string_view addressTag(withdrawAddress["addressTag"].get<std::string_view>());
    if (address == wallet.address() && addressTag == wallet.tag()) {
      huobiWithdrawAddressName = withdrawAddress["note"].get<std::string_view>();
      break;
    }
  }
  if (huobiWithdrawAddressName.empty()) {
    throw exception("Address should be stored in your Huobi account manually in order to withdraw from API");
  }
  log::info("Found stored {} withdraw address '{}'", _exchangePublic.name(), huobiWithdrawAddressName);

  CurlPostData withdrawPostData{{"address", wallet.address()}};
  if (wallet.hasTag()) {
    withdrawPostData.append("addr-tag", wallet.tag());
  }

  MonetaryAmount fee(_exchangePublic.queryWithdrawalFee(grossAmount.currencyCode()));
  MonetaryAmount netEmittedAmount = grossAmount - fee;

  withdrawPostData.append("amount", netEmittedAmount.amountStr());
  withdrawPostData.append("currency", lowerCaseCur);
  // Strange to have the fee as input parameter of a withdraw...
  withdrawPostData.append("fee", fee.amountStr());

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/dw/withdraw/api/create",
                             std::move(withdrawPostData));
  string withdrawIdStr = ToString(result["data"].get<int64_t>());
  return InitiatedWithdrawInfo(std::move(wallet), std::move(withdrawIdStr), grossAmount);
}

SentWithdrawInfo HuobiPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  string lowerCaseCur = ToLower(currencyCode.str());
  std::string_view withdrawIdStr = initiatedWithdrawInfo.withdrawId();
  int64_t withdrawId = FromString<int64_t>(withdrawIdStr);
  json withdrawJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw",
                                   {{"currency", lowerCaseCur}, {"from", withdrawIdStr}, {"type", "withdraw"}});
  MonetaryAmount netEmittedAmount;
  bool isWithdrawSent = false;
  for (const json& withdrawDetail : withdrawJson["data"]) {
    if (withdrawDetail["id"].get<int64_t>() == withdrawId) {
      std::string_view withdrawStatus = withdrawDetail["state"].get<std::string_view>();
      if (withdrawStatus == "verifying") {
        log::debug("Awaiting verification");
      } else if (withdrawStatus == "failed") {
        log::error("Verification failed");
      } else if (withdrawStatus == "submitted") {
        log::debug("Withdraw request submitted successfully");
      } else if (withdrawStatus == "reexamine") {
        log::warn("Under examination for withdraw validation");
      } else if (withdrawStatus == "canceled") {
        log::error("Withdraw canceled");
      } else if (withdrawStatus == "pass") {
        log::debug("Withdraw validation passed");
      } else if (withdrawStatus == "reject") {
        log::error("Withdraw validation rejected");
      } else if (withdrawStatus == "pre-transfer") {
        log::debug("Withdraw is about to be released");
      } else if (withdrawStatus == "wallet-transfer") {
        log::debug("On-chain transfer initiated");
      } else if (withdrawStatus == "wallet-reject") {
        log::error("Transfer rejected by chain");
      } else if (withdrawStatus == "confirmed") {
        isWithdrawSent = true;
        log::debug("On-chain transfer completed with one confirmation");
      } else if (withdrawStatus == "confirm-error") {
        log::error("On-chain transfer failed to get confirmation");
      } else if (withdrawStatus == "repealed") {
        log::error("Withdraw terminated by system");
      } else {
        log::error("unknown status value '{}'", withdrawStatus);
      }
      netEmittedAmount = MonetaryAmount(withdrawDetail["amount"].get<double>(), currencyCode);
      MonetaryAmount fee(withdrawDetail["fee"].get<double>(), currencyCode);
      double diffExpected = (netEmittedAmount + fee - initiatedWithdrawInfo.grossEmittedAmount()).toDouble();
      if (diffExpected > 0.001 || diffExpected < -0.001) {
        log::error("Unexpected fee - {} + {} != {}, maybe a change in API", netEmittedAmount.amountStr(),
                   fee.amountStr(), initiatedWithdrawInfo.grossEmittedAmount().amountStr());
      }
      break;
    }
  }
  return SentWithdrawInfo(netEmittedAmount, isWithdrawSent);
}

bool HuobiPrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                      const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  string lowerCaseCur = ToLower(currencyCode.str());

  json depositJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/query/deposit-withdraw",
                                  {{"currency", lowerCaseCur}, {"type", "deposit"}});
  MonetaryAmount netEmittedAmount = sentWithdrawInfo.netEmittedAmount();
  RecentDeposit::RecentDepositVector recentDeposits;
  for (const json& depositDetail : depositJson["data"]) {
    std::string_view depositStatus = depositDetail["state"].get<std::string_view>();
    log::debug("Exploring Huobi deposit with status {}", depositStatus);
    if (depositStatus == "confirmed") {
      MonetaryAmount amount(depositDetail["amount"].get<double>(), currencyCode);
      int64_t millisecondsSinceEpoch = depositDetail["updated-at"].get<int64_t>();
      TimePoint timestamp{std::chrono::milliseconds(millisecondsSinceEpoch)};

      recentDeposits.emplace_back(amount, timestamp);
    }
  }
  RecentDeposit expectedDeposit(netEmittedAmount, Clock::now());
  return expectedDeposit.selectClosestRecentDeposit(recentDeposits) != nullptr;
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