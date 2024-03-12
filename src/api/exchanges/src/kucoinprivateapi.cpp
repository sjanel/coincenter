#include "kucoinprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
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
#include "closed-order.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangeconfig.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "kucoinpublicapi.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
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
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

constexpr std::string_view kStatusCodeOK = "200000";

json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostData&& postData = CurlPostData(), bool throwIfError = true) {
  string strToSign(Nonce_TimeSinceEpochInMs());
  auto nonceSize = strToSign.size();
  strToSign.append(ToString(requestType));
  strToSign.append(endpoint);

  CurlOptions::PostDataFormat postDataFormat = CurlOptions::PostDataFormat::kString;
  if (!postData.empty()) {
    if (requestType == HttpRequestType::kGet || requestType == HttpRequestType::kDelete) {
      strToSign.push_back('?');
      strToSign.append(postData.str());
    } else {
      strToSign.append(postData.toJson().dump());
      postDataFormat = CurlOptions::PostDataFormat::kJson;
    }
  }

  string signature = B64Encode(ssl::ShaBin(ssl::ShaType::kSha256, strToSign, apiKey.privateKey()));
  string passphrase = B64Encode(ssl::ShaBin(ssl::ShaType::kSha256, apiKey.passphrase(), apiKey.privateKey()));

  CurlOptions opts(requestType, std::move(postData), postDataFormat);
  opts.appendHttpHeader("KC-API-KEY", apiKey.key());
  opts.appendHttpHeader("KC-API-SIGN", signature);
  opts.appendHttpHeader("KC-API-TIMESTAMP", std::string_view(strToSign.data(), nonceSize));
  opts.appendHttpHeader("KC-API-PASSPHRASE", passphrase);
  opts.appendHttpHeader("KC-API-KEY-VERSION", 2);

  json ret = json::parse(curlHandle.query(endpoint, opts));
  auto errCodeIt = ret.find("code");
  if (errCodeIt != ret.end() && errCodeIt->get<std::string_view>() != kStatusCodeOK) {
    auto msgIt = ret.find("msg");
    std::string_view msg = msgIt == ret.end() ? std::string_view() : msgIt->get<std::string_view>();
    if (requestType == HttpRequestType::kDelete) {
      log::warn("Kucoin error {}:'{}' bypassed, object probably disappeared correctly",
                errCodeIt->get<std::string_view>(), msg);
      return ret;
    }
    if (throwIfError) {
      log::error("Full Kucoin json error: '{}'", ret.dump());
      throw exception("Kucoin error: {}, msg: {}", errCodeIt->get<std::string_view>(), msg);
    }
  }
  return ret;
}

void InnerTransfer(CurlHandle& curlHandle, const APIKey& apiKey, MonetaryAmount amount, std::string_view fromStr,
                   std::string_view toStr) {
  log::info("Perform inner transfer of {} to {} account", amount, toStr);
  PrivateQuery(curlHandle, apiKey, HttpRequestType::kPost, "/api/v2/accounts/inner-transfer",
               {{"clientOid", Nonce_TimeSinceEpochInMs()},  // Not really needed, but it's mandatory apparently
                {"currency", amount.currencyStr()},
                {"amount", amount.amountStr()},
                {"from", fromStr},
                {"to", toStr}});
}

bool EnsureEnoughAmountIn(CurlHandle& curlHandle, const APIKey& apiKey, MonetaryAmount expectedAmount,
                          std::string_view accountName) {
  // Check if enough balance in the 'accountName' account of Kucoin
  CurrencyCode cur = expectedAmount.currencyCode();
  json res =
      PrivateQuery(curlHandle, apiKey, HttpRequestType::kGet, "/api/v1/accounts", {{"currency", cur.str()}})["data"];
  MonetaryAmount totalAvailableAmount(0, cur);
  MonetaryAmount amountInTargetAccount = totalAvailableAmount;
  for (const json& balanceDetail : res) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    MonetaryAmount av(balanceDetail["available"].get<std::string_view>(), cur);
    totalAvailableAmount += av;
    if (typeStr == accountName) {
      amountInTargetAccount = av;
    }
  }
  if (totalAvailableAmount < expectedAmount) {
    log::error("Insufficient funds to place in '{}' ({} < {})", accountName, totalAvailableAmount, expectedAmount);
    return false;
  }
  if (amountInTargetAccount < expectedAmount) {
    for (const json& balanceDetail : res) {
      std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
      MonetaryAmount av(balanceDetail["available"].get<std::string_view>(), cur);
      if (typeStr != accountName && av != 0) {
        MonetaryAmount remainingAmountToInnerTransfer = expectedAmount - amountInTargetAccount;
        if (av < remainingAmountToInnerTransfer) {
          InnerTransfer(curlHandle, apiKey, av, typeStr, accountName);
          amountInTargetAccount += av;
        } else {
          InnerTransfer(curlHandle, apiKey, remainingAmountToInnerTransfer, typeStr, accountName);
          break;
        }
      }
    }
  }
  return true;
}

}  // namespace

KucoinPrivate::KucoinPrivate(const CoincenterInfo& coincenterInfo, KucoinPublic& kucoinPublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, kucoinPublic, apiKey),
      _curlHandle(KucoinPublic::kUrlBase, coincenterInfo.metricGatewayPtr(),
                  PermanentCurlOptions::Builder()
                      .setMinDurationBetweenQueries(exchangeConfig().privateAPIRate())
                      .setAcceptedEncoding(exchangeConfig().acceptEncoding())
                      .setRequestCallLogLevel(exchangeConfig().requestsCallLogLevel())
                      .setRequestAnswerLogLevel(exchangeConfig().requestsAnswerLogLevel())
                      .build(),
                  coincenterInfo.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, kucoinPublic) {}

bool KucoinPrivate::validateApiKey() {
  constexpr bool throwIfError = false;
  json ret =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/accounts", CurlPostData(), throwIfError);
  auto errCodeIt = ret.find("code");
  return errCodeIt == ret.end() || errCodeIt->get<std::string_view>() == kStatusCodeOK;
}

BalancePortfolio KucoinPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/accounts")["data"];
  BalancePortfolio balancePortfolio;
  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  const std::string_view amountKey = withBalanceInUse ? "balance" : "available";
  for (const json& balanceDetail : result) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    CurrencyCode currencyCode(
        _coincenterInfo.standardizeCurrencyCode(balanceDetail["currency"].get<std::string_view>()));
    MonetaryAmount amount(balanceDetail[amountKey].get<std::string_view>(), currencyCode);
    log::debug("{} in account '{}' on {}", amount, typeStr, exchangeName());
    this->addBalance(balancePortfolio, amount, equiCurrency);
  }
  return balancePortfolio;
}

Wallet KucoinPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v2/deposit-addresses",
                             {{"currency", currencyCode.str()}})["data"];
  ExchangeName exchangeName(_kucoinPublic.name(), _apiKey.name());
  if (result.empty()) {
    log::info("No deposit address for {} in {}, creating one", currencyCode, exchangeName);
    result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/api/v1/deposit-addresses",
                          {{"currency", currencyCode.str()}})["data"];
  } else {
    result = result.front();
  }

  auto memoIt = result.find("memo");
  std::string_view tag = (memoIt != result.end() && !memoIt->is_null()) ? memoIt->get<std::string_view>() : "";

  const CoincenterInfo& coincenterInfo = _kucoinPublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeConfig(_kucoinPublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  Wallet wallet(std::move(exchangeName), currencyCode, std::move(result["address"].get_ref<string&>()), tag,
                walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
template <class OrderVectorType>
void FillOrders(const OrdersConstraints& ordersConstraints, CurlHandle& curlHandle, const APIKey& apiKey,
                ExchangePublic& exchangePublic, OrderVectorType& orderVector) {
  using OrderType = std::remove_cvref_t<decltype(*std::declval<OrderVectorType>().begin())>;

  CurlPostData params{{"status", std::is_same_v<OrderType, OpenedOrder> ? "active" : "done"}, {"tradeType", "TRADE"}};

  if (ordersConstraints.isCurDefined()) {
    MarketSet markets;
    Market filterMarket =
        exchangePublic.determineMarketFromFilterCurrencies(markets, ordersConstraints.cur1(), ordersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.append("symbol", filterMarket.assetsPairStrUpper('-'));
    }
  }
  if (ordersConstraints.isPlacedTimeAfterDefined()) {
    params.append("startAt", TimestampToMillisecondsSinceEpoch(ordersConstraints.placedAfter()));
  }
  if (ordersConstraints.isPlacedTimeBeforeDefined()) {
    params.append("endAt", TimestampToMillisecondsSinceEpoch(ordersConstraints.placedBefore()));
  }
  json data = PrivateQuery(curlHandle, apiKey, HttpRequestType::kGet, "/api/v1/orders", std::move(params))["data"];

  for (json& orderDetails : data["items"]) {
    std::string_view marketStr = orderDetails["symbol"].get<std::string_view>();
    std::size_t dashPos = marketStr.find('-');
    if (dashPos == std::string_view::npos) {
      throw exception("Expected a dash in {} for {} orders query", marketStr, exchangePublic.name());
    }
    CurrencyCode volumeCur(std::string_view(marketStr.data(), dashPos));
    CurrencyCode priceCur(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()));

    if (!ordersConstraints.validateCur(volumeCur, priceCur)) {
      continue;
    }

    TimePoint placedTime{milliseconds(orderDetails["createdAt"].get<int64_t>())};

    string id = std::move(orderDetails["id"].get_ref<string&>());
    if (!ordersConstraints.validateId(id)) {
      continue;
    }

    MonetaryAmount matchedVolume(orderDetails["dealSize"].get<std::string_view>(), volumeCur);
    MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
    TradeSide side = orderDetails["side"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

    if constexpr (std::is_same_v<OrderType, OpenedOrder>) {
      MonetaryAmount originalVolume(orderDetails["size"].get<std::string_view>(), volumeCur);
      MonetaryAmount remainingVolume = originalVolume - matchedVolume;

      orderVector.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
    } else if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
      const TimePoint matchedTime = placedTime;

      orderVector.emplace_back(std::move(id), matchedVolume, price, placedTime, matchedTime, side);
    } else {
      // Note: below ugly template lambda can be replaced with 'static_assert(false);' in C++23
      []<bool flag = false>() { static_assert(flag, "no match"); }
      ();
    }
  }
  std::ranges::sort(orderVector);
  orderVector.shrink_to_fit();
}
}  // namespace

ClosedOrderVector KucoinPrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  ClosedOrderVector closedOrders;
  FillOrders(closedOrdersConstraints, _curlHandle, _apiKey, _exchangePublic, closedOrders);
  log::info("Retrieved {} closed orders from {}", closedOrders.size(), _exchangePublic.name());
  return closedOrders;
}

OpenedOrderVector KucoinPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  OpenedOrderVector openedOrders;
  FillOrders(openedOrdersConstraints, _curlHandle, _apiKey, _exchangePublic, openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

int KucoinPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.isMarketOnlyDependent() || openedOrdersConstraints.noConstraints()) {
    CurlPostData params;
    if (openedOrdersConstraints.isMarketDefined()) {
      params.append("symbol", openedOrdersConstraints.market().assetsPairStrUpper('-'));
    }
    json res = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v1/orders", std::move(params));
    int nbCancelledOrders = 0;
    auto dataIt = res.find("data");
    if (dataIt != res.end()) {
      auto cancelledOrderIdsIt = dataIt->find("cancelledOrderIds");
      if (cancelledOrderIdsIt != dataIt->end()) {
        nbCancelledOrders = cancelledOrderIdsIt->size();
      }
    }
    return nbCancelledOrders;
  }
  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const OpenedOrder& order : openedOrders) {
    cancelOrderProcess(order.id());
  }
  return openedOrders.size();
}

namespace {
Deposit::Status DepositStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "SUCCESS") {
    return Deposit::Status::kSuccess;
  }
  if (statusStr == "PROCESSING") {
    return Deposit::Status::kProcessing;
  }
  if (statusStr == "FAILURE") {
    return Deposit::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized deposit status '{}' from Kucoin", statusStr);
}
}  // namespace

DepositsSet KucoinPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.append("currency", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isTimeAfterDefined()) {
    options.append("startAt", TimestampToMillisecondsSinceEpoch(depositsConstraints.timeAfter()));
  }
  if (depositsConstraints.isTimeBeforeDefined()) {
    options.append("endAt", TimestampToMillisecondsSinceEpoch(depositsConstraints.timeBefore()));
  }
  if (depositsConstraints.isIdDefined()) {
    if (depositsConstraints.idSet().size() == 1) {
      options.append("txId", depositsConstraints.idSet().front());
    }
  }
  json depositJson =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/deposits", std::move(options))["data"];
  auto itemsIt = depositJson.find("items");
  if (itemsIt == depositJson.end()) {
    throw exception("Unexpected result from Kucoin deposit API");
  }
  Deposits deposits;
  deposits.reserve(itemsIt->size());
  for (const json& depositDetail : *itemsIt) {
    CurrencyCode currencyCode(depositDetail["currency"].get<std::string_view>());
    MonetaryAmount amount(depositDetail["amount"].get<std::string_view>(), currencyCode);
    int64_t millisecondsSinceEpoch = depositDetail["updatedAt"].get<int64_t>();

    std::string_view statusStr = depositDetail["status"].get<std::string_view>();
    Deposit::Status status = DepositStatusFromStatusStr(statusStr);

    TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};

    // Kucoin does not provide any transaction id, let's generate it from currency and timestamp...
    string id = currencyCode.str();
    id.push_back('-');
    id.append(ToString(millisecondsSinceEpoch));

    deposits.emplace_back(std::move(id), timestamp, amount, status);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr, bool logStatus) {
  if (statusStr == "PROCESSING") {
    if (logStatus) {
      log::debug("Processing");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "WALLET_PROCESSING") {
    if (logStatus) {
      log::debug("Wallet processing");
    }
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "SUCCESS") {
    if (logStatus) {
      log::debug("Success");
    }
    return Withdraw::Status::kSuccess;
  }
  if (statusStr == "FAILURE") {
    if (logStatus) {
      log::warn("Failure");
    }
    return Withdraw::Status::kFailureOrRejected;
  }
  throw exception("unknown status value '{}' returned by Kucoin", statusStr);
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options;
  if (withdrawsConstraints.isCurDefined()) {
    options.append("currency", withdrawsConstraints.currencyCode().str());
  }
  if (withdrawsConstraints.isTimeAfterDefined()) {
    options.append("startAt", TimestampToMillisecondsSinceEpoch(withdrawsConstraints.timeAfter()));
  }
  if (withdrawsConstraints.isTimeBeforeDefined()) {
    options.append("endAt", TimestampToMillisecondsSinceEpoch(withdrawsConstraints.timeBefore()));
  }
  return options;
}

}  // namespace

WithdrawsSet KucoinPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  json withdrawJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/withdrawals",
                                   CreateOptionsFromWithdrawConstraints(withdrawsConstraints))["data"];
  auto itemsIt = withdrawJson.find("items");
  if (itemsIt == withdrawJson.end()) {
    throw exception("Unexpected result from Kucoin withdraw API");
  }
  Withdraws withdraws;
  withdraws.reserve(itemsIt->size());
  for (const json& withdrawDetail : *itemsIt) {
    CurrencyCode currencyCode(withdrawDetail["currency"].get<std::string_view>());
    MonetaryAmount netEmittedAmount(withdrawDetail["amount"].get<std::string_view>(), currencyCode);
    MonetaryAmount fee(withdrawDetail["fee"].get<std::string_view>(), currencyCode);
    int64_t millisecondsSinceEpoch = withdrawDetail["updatedAt"].get<int64_t>();

    std::string_view statusStr = withdrawDetail["status"].get<std::string_view>();
    Withdraw::Status status = WithdrawStatusFromStatusStr(statusStr, withdrawsConstraints.isIdDependent());

    TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};

    std::string_view id = withdrawDetail["id"].get<std::string_view>();
    if (!withdrawsConstraints.validateId(id)) {
      continue;
    }

    withdraws.emplace_back(id, timestamp, netEmittedAmount, status, fee);
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdrawals for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

PlaceOrderInfo KucoinPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                         const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  if (!EnsureEnoughAmountIn(_curlHandle, _apiKey, from, "trade")) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  const Market mk = tradeInfo.tradeContext.mk;

  bool isTakerStrategy = tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeConfig().placeSimulateRealOrder());

  KucoinPublic& kucoinPublic = dynamic_cast<KucoinPublic&>(_exchangePublic);

  price = kucoinPublic.sanitizePrice(mk, price);

  MonetaryAmount sanitizedVol = kucoinPublic.sanitizeVolume(mk, volume);
  if (volume < sanitizedVol) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode,
              sanitizedVol);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  std::string_view buyOrSell = fromCurrencyCode == mk.base() ? "sell" : "buy";
  std::string_view strategyType = isTakerStrategy ? "market" : "limit";

  CurlPostData params = KucoinPublic::GetSymbolPostData(mk);
  params.append("clientOid", Nonce_TimeSinceEpochInMs());
  params.append("side", buyOrSell);
  params.append("type", strategyType);
  params.append("remark", "Placed by coincenter client");
  params.append("tradeType", "TRADE");
  params.append("size", volume.amountStr());
  if (!isTakerStrategy) {
    params.append("price", price.amountStr());
  }

  // Add automatic cancelling just in case program unexpectedly stops
  params.append("timeInForce", "GTT");  // Good until cancelled or time expires
  params.append("cancelAfter", std::chrono::duration_cast<seconds>(tradeInfo.options.maxTradeTime()).count() + 1);

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/api/v1/orders", std::move(params))["data"];
  placeOrderInfo.orderId = std::move(result["orderId"].get_ref<string&>());
  return placeOrderInfo;
}

OrderInfo KucoinPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId);
  return queryOrderInfo(orderId, tradeContext);
}

void KucoinPrivate::cancelOrderProcess(OrderIdView orderId) {
  string endpoint("/api/v1/orders/");
  endpoint.append(orderId);
  PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, endpoint);
}

OrderInfo KucoinPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  const CurrencyCode fromCurrencyCode(tradeContext.fromCur());
  const Market mk = tradeContext.mk;
  string endpoint("/api/v1/orders/");
  endpoint.append(orderId);

  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint)["data"];

  MonetaryAmount size(data["size"].get<std::string_view>(), mk.base());
  MonetaryAmount matchedSize(data["dealSize"].get<std::string_view>(), mk.base());

  // Fee is already deduced from the matched amount
  MonetaryAmount fromAmount;
  MonetaryAmount toAmount;
  MonetaryAmount dealFunds(data["dealFunds"].get<std::string_view>(), mk.quote());
  if (fromCurrencyCode == mk.base()) {
    // sell
    fromAmount = matchedSize;
    toAmount = dealFunds;
  } else {
    // buy
    fromAmount = dealFunds;
    toAmount = matchedSize;
  }
  return OrderInfo(TradedAmounts(fromAmount, toAmount), !data["isActive"].get<bool>());
}

InitiatedWithdrawInfo KucoinPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  if (!EnsureEnoughAmountIn(_curlHandle, _apiKey, grossAmount, "main")) {
    throw exception("Insufficient funds for withdraw");
  }
  const CurrencyCode currencyCode = grossAmount.currencyCode();

  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFeeOrZero(currencyCode);

  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;

  CurlPostData opts{{"currency", currencyCode.str()},
                    {"address", destinationWallet.address()},
                    {"amount", netEmittedAmount.amountStr()}};
  if (destinationWallet.hasTag()) {
    opts.append("memo", destinationWallet.tag());
  }

  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/api/v1/withdrawals", std::move(opts))["data"];
  return {std::move(destinationWallet), std::move(result["withdrawalId"].get_ref<string&>()), grossAmount};
}

}  // namespace cct::api
