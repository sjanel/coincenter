#include "kucoinprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "kucoinpublicapi.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"

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

  CurlOptions opts(requestType, std::move(postData), KucoinPublic::kUserAgent, postDataFormat);
  opts.appendHttpHeader("KC-API-KEY", apiKey.key());
  opts.appendHttpHeader("KC-API-SIGN", signature);
  opts.appendHttpHeader("KC-API-TIMESTAMP", std::string_view(strToSign.data(), nonceSize));
  opts.appendHttpHeader("KC-API-PASSPHRASE", passphrase);
  opts.appendHttpHeader("KC-API-KEY-VERSION", 2);

  json ret = json::parse(curlHandle.query(endpoint, opts));
  auto errCodeIt = ret.find("code");
  if (errCodeIt != ret.end() && errCodeIt->get<std::string_view>() != kStatusCodeOK) {
    auto msgIt = ret.find("msg");
    std::string_view msg = msgIt == ret.end() ? "" : msgIt->get<std::string_view>();
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
      _curlHandle(KucoinPublic::kUrlBase, coincenterInfo.metricGatewayPtr(), exchangeInfo().privateAPIRate(),
                  coincenterInfo.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
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
  bool doCheckWallet = coincenterInfo.exchangeInfo(_kucoinPublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  Wallet wallet(std::move(exchangeName), currencyCode, std::move(result["address"].get_ref<string&>()), tag,
                walletCheck);
  log::info("Retrieved {}", wallet);
  return wallet;
}

Orders KucoinPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params{{"status", "active"}, {"tradeType", "TRADE"}};

  if (openedOrdersConstraints.isCur1Defined()) {
    MarketSet markets;
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.append("symbol", filterMarket.assetsPairStrUpper('-'));
    }
  }
  if (openedOrdersConstraints.isPlacedTimeAfterDefined()) {
    params.append("startAt", TimestampToMs(openedOrdersConstraints.placedAfter()));
  }
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/orders", std::move(params))["data"];

  Orders openedOrders;
  for (json& orderDetails : data["items"]) {
    std::string_view marketStr = orderDetails["symbol"].get<std::string_view>();
    std::size_t dashPos = marketStr.find('-');
    assert(dashPos != std::string_view::npos);
    CurrencyCode volumeCur(std::string_view(marketStr.data(), dashPos));
    CurrencyCode priceCur(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()));

    if (!openedOrdersConstraints.validateCur(volumeCur, priceCur)) {
      continue;
    }

    int64_t millisecondsSinceEpoch = orderDetails["createdAt"].get<int64_t>();

    TimePoint placedTime{std::chrono::milliseconds(millisecondsSinceEpoch)};
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    string id = std::move(orderDetails["id"].get_ref<string&>());
    if (!openedOrdersConstraints.validateOrderId(id)) {
      continue;
    }

    MonetaryAmount originalVolume(orderDetails["size"].get<std::string_view>(), volumeCur);
    MonetaryAmount matchedVolume(orderDetails["dealSize"].get<std::string_view>(), volumeCur);
    MonetaryAmount remainingVolume = originalVolume - matchedVolume;
    MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
    TradeSide side = orderDetails["side"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

    openedOrders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
  }
  std::ranges::sort(openedOrders);
  openedOrders.shrink_to_fit();
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
  Orders openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const Order& order : openedOrders) {
    cancelOrderProcess(order.id());
  }
  return openedOrders.size();
}

Deposits KucoinPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  CurlPostData options{{"status", "SUCCESS"}};
  if (depositsConstraints.isCurDefined()) {
    options.append("currency", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isReceivedTimeAfterDefined()) {
    options.append("startAt", TimestampToMs(depositsConstraints.receivedAfter()));
  }
  if (depositsConstraints.isReceivedTimeBeforeDefined()) {
    options.append("endAt", TimestampToMs(depositsConstraints.receivedBefore()));
  }
  if (depositsConstraints.isDepositIdDefined()) {
    if (depositsConstraints.depositIdSet().size() == 1) {
      options.append("txId", depositsConstraints.depositIdSet().front());
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

    TimePoint timestamp{std::chrono::milliseconds(millisecondsSinceEpoch)};

    // Kucoin does not provide any transaction id, let's generate it from currency and timestamp...
    string id = currencyCode.str();
    id.push_back('-');
    id.append(ToString(millisecondsSinceEpoch));

    deposits.emplace_back(std::move(id), timestamp, amount);
  }
  log::info("Retrieved {} recent deposits for {}", deposits.size(), exchangeName());
  return deposits;
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

  bool isTakerStrategy = tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeInfo().placeSimulateRealOrder());

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
  params.append("cancelAfter", std::chrono::duration_cast<TimeInS>(tradeInfo.options.maxTradeTime()).count() + 1);

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

  MonetaryAmount fee(_exchangePublic.queryWithdrawalFee(grossAmount.currencyCode()));
  MonetaryAmount netEmittedAmount = grossAmount - fee;

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

SentWithdrawInfo KucoinPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  json withdrawJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/withdrawals",
                                   {{"currency", currencyCode.str()}})["data"];
  MonetaryAmount netEmittedAmount;
  MonetaryAmount fee;
  bool isWithdrawSent = false;
  for (const json& withdrawDetail : withdrawJson["items"]) {
    if (withdrawDetail["id"].get<std::string_view>() == withdrawId) {
      std::string_view withdrawStatus = withdrawDetail["status"].get<std::string_view>();
      if (withdrawStatus == "PROCESSING") {
        log::debug("Processing");
      } else if (withdrawStatus == "WALLET_PROCESSING") {
        log::debug("Wallet processing");
      } else if (withdrawStatus == "SUCCESS") {
        log::debug("Success");
        isWithdrawSent = true;
      } else if (withdrawStatus == "FAILURE") {
        log::warn("Failure");
      } else {
        log::error("unknown status value '{}'", withdrawStatus);
      }
      netEmittedAmount = MonetaryAmount(withdrawDetail["amount"].get<std::string_view>(), currencyCode);
      fee = MonetaryAmount(withdrawDetail["fee"].get<std::string_view>(), currencyCode);
      if (netEmittedAmount + fee != initiatedWithdrawInfo.grossEmittedAmount()) {
        log::error("{} + {} != {}, maybe a change in API", netEmittedAmount, fee,
                   initiatedWithdrawInfo.grossEmittedAmount());
      }
      break;
    }
  }
  return {netEmittedAmount, fee, isWithdrawSent};
}

}  // namespace cct::api
