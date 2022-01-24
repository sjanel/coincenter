#include "kucoinprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "kucoinpublicapi.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"

namespace cct::api {

namespace {

json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view method,
                  CurlPostData&& postdata = CurlPostData()) {
  string strToSign(Nonce_TimeSinceEpochInMs());
  auto nonceSize = strToSign.size();
  strToSign.append(ToString(requestType));
  strToSign.append(method);

  CurlOptions::PostDataFormat postdataFormat = CurlOptions::PostDataFormat::kString;
  if (!postdata.empty()) {
    if (requestType == HttpRequestType::kGet || requestType == HttpRequestType::kDelete) {
      strToSign.push_back('?');
      strToSign.append(postdata.str());
    } else {
      strToSign.append(postdata.toJson().dump());
      postdataFormat = CurlOptions::PostDataFormat::kJson;
    }
  }

  string signature = B64Encode(ssl::ShaBin(ssl::ShaType::kSha256, strToSign, apiKey.privateKey()));
  string passphrase = B64Encode(ssl::ShaBin(ssl::ShaType::kSha256, apiKey.passphrase(), apiKey.privateKey()));

  CurlOptions opts(requestType, std::move(postdata), KucoinPublic::kUserAgent, postdataFormat);
  opts.appendHttpHeader("KC-API-KEY", apiKey.key());
  opts.appendHttpHeader("KC-API-SIGN", signature);
  opts.appendHttpHeader("KC-API-TIMESTAMP", std::string_view(strToSign.data(), nonceSize));
  opts.appendHttpHeader("KC-API-PASSPHRASE", passphrase);
  opts.appendHttpHeader("KC-API-KEY-VERSION", 2);

  string url(KucoinPublic::kUrlBase);
  url.append(method);

  json dataJson = json::parse(curlHandle.query(url, std::move(opts)));
  auto errCodeIt = dataJson.find("code");
  if (errCodeIt != dataJson.end() && errCodeIt->get<std::string_view>() != "200000") {
    string errStr("Kucoin error ");
    errStr.append(errCodeIt->get<std::string_view>());
    auto msgIt = dataJson.find("msg");
    if (msgIt != dataJson.end()) {
      errStr.append(" - ");
      errStr.append(msgIt->get<std::string_view>());
    }
    if (requestType == HttpRequestType::kDelete) {
      log::warn("{} bypassed, object probably disappeared correctly", errStr);
      dataJson.clear();
      return dataJson;
    }
    throw exception(std::move(errStr));
  }
  return dataJson["data"];
}

void InnerTransfer(CurlHandle& curlHandle, const APIKey& apiKey, MonetaryAmount amount, std::string_view fromStr,
                   std::string_view toStr) {
  log::info("Perform inner transfer of {} to {} account", amount.str(), toStr);
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
  json balanceCur =
      PrivateQuery(curlHandle, apiKey, HttpRequestType::kGet, "/api/v1/accounts", {{"currency", cur.str()}});
  MonetaryAmount totalAvailableAmount(0, cur);
  MonetaryAmount amountInTargetAccount = totalAvailableAmount;
  for (const json& balanceDetail : balanceCur) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    MonetaryAmount av(balanceDetail["available"].get<std::string_view>(), cur);
    totalAvailableAmount += av;
    if (typeStr == accountName) {
      amountInTargetAccount = av;
    }
  }
  if (totalAvailableAmount < expectedAmount) {
    log::error("Insufficient funds to place in '{}' ({} < {})", accountName, totalAvailableAmount.str(),
               expectedAmount.str());
    return false;
  }
  if (amountInTargetAccount < expectedAmount) {
    for (const json& balanceDetail : balanceCur) {
      std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
      MonetaryAmount av(balanceDetail["available"].get<std::string_view>(), cur);
      if (typeStr != accountName && !av.isZero()) {
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

KucoinPrivate::KucoinPrivate(const CoincenterInfo& config, KucoinPublic& kucoinPublic, const APIKey& apiKey)
    : ExchangePrivate(config, kucoinPublic, apiKey),
      _curlHandle(config.metricGatewayPtr(), config.exchangeInfo(kucoinPublic.name()).minPrivateQueryDelay(),
                  config.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, kucoinPublic) {}

BalancePortfolio KucoinPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/accounts");
  BalancePortfolio balancePortfolio;
  for (const json& balanceDetail : result) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    CurrencyCode currencyCode(
        _coincenterInfo.standardizeCurrencyCode(CurrencyCode(balanceDetail["currency"].get<std::string_view>())));
    MonetaryAmount amount(balanceDetail["available"].get<std::string_view>(), currencyCode);
    log::debug("{} in account '{}' on {}", amount.str(), typeStr, _exchangePublic.name());
    this->addBalance(balancePortfolio, amount, equiCurrency);
  }
  return balancePortfolio;
}

Wallet KucoinPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v2/deposit-addresses",
                             {{"currency", currencyCode.str()}});
  if (result.empty()) {
    log::info("No deposit address for {} in {}, creating one", currencyCode.str(), _kucoinPublic.name());
    result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/api/v1/deposit-addresses",
                          {{"currency", currencyCode.str()}});
  } else {
    result = result.front();
  }

  std::string_view address = result["address"].get<std::string_view>();
  auto memoIt = result.find("memo");
  std::string_view tag = (memoIt != result.end() && !memoIt->is_null()) ? memoIt->get<std::string_view>() : "";

  PrivateExchangeName privateExchangeName(_kucoinPublic.name(), _apiKey.name());

  const CoincenterInfo& coincenterInfo = _kucoinPublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(privateExchangeName.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  Wallet w(std::move(privateExchangeName), currencyCode, address, tag, walletCheck);
  log::info("Retrieved {}", w.str());
  return w;
}

ExchangePrivate::Orders KucoinPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params{{"status", "active"}, {"tradeType", "TRADE"}};

  if (openedOrdersConstraints.isCur1Defined()) {
    ExchangePublic::MarketSet markets;
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.append("symbol", filterMarket.assetsPairStrUpper('-'));
    }
  }
  if (openedOrdersConstraints.isPlacedTimeAfterDefined()) {
    params.append("startAt", std::chrono::duration_cast<std::chrono::milliseconds>(
                                 openedOrdersConstraints.placedAfter().time_since_epoch())
                                 .count());
  }
  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/orders", std::move(params));

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

void KucoinPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.isMarketOnlyDependent() || openedOrdersConstraints.noConstraints()) {
    CurlPostData params;
    if (openedOrdersConstraints.isMarketDefined()) {
      params.append("symbol", openedOrdersConstraints.market().assetsPairStrUpper('-'));
    }
    PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/api/v1/orders", std::move(params));
    return;
  }
  for (const Order& o : queryOpenedOrders(openedOrdersConstraints)) {
    cancelOrderProcess(o.id());
  }
}

PlaceOrderInfo KucoinPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                         const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.toCur());

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));

  if (!EnsureEnoughAmountIn(_curlHandle, _apiKey, from, "trade")) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  const Market m = tradeInfo.m;

  bool isTakerStrategy = tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeInfo().placeSimulateRealOrder());

  KucoinPublic& kucoinPublic = dynamic_cast<KucoinPublic&>(_exchangePublic);

  price = kucoinPublic.sanitizePrice(m, price);

  MonetaryAmount sanitizedVol = kucoinPublic.sanitizeVolume(m, volume);
  if (volume < sanitizedVol) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(), toCurrencyCode.str(),
              sanitizedVol.str());
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  std::string_view buyOrSell = fromCurrencyCode == m.base() ? "sell" : "buy";
  std::string_view strategyType = isTakerStrategy ? "market" : "limit";

  CurlPostData params = KucoinPublic::GetSymbolPostData(m);
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

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/api/v1/orders", std::move(params));
  placeOrderInfo.orderId = std::move(result["orderId"].get_ref<string&>());
  return placeOrderInfo;
}

OrderInfo KucoinPrivate::cancelOrder(const OrderRef& orderRef) {
  cancelOrderProcess(orderRef.id);
  return queryOrderInfo(orderRef);
}

void KucoinPrivate::cancelOrderProcess(const OrderId& id) {
  string endpoint("/api/v1/orders/");
  endpoint.append(id);
  PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, endpoint);
}

OrderInfo KucoinPrivate::queryOrderInfo(const OrderRef& orderRef) {
  const CurrencyCode fromCurrencyCode(orderRef.fromCur());
  const Market m = orderRef.m;
  string endpoint = "/api/v1/orders/";
  endpoint.append(orderRef.id);

  json data = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint);

  MonetaryAmount size(data["size"].get<std::string_view>(), m.base());
  MonetaryAmount matchedSize(data["dealSize"].get<std::string_view>(), m.base());

  // Fee is already deduced from the matched amount
  MonetaryAmount fromAmount, toAmount;
  MonetaryAmount dealFunds(data["dealFunds"].get<std::string_view>(), m.quote());
  if (fromCurrencyCode == m.base()) {
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

InitiatedWithdrawInfo KucoinPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  if (!EnsureEnoughAmountIn(_curlHandle, _apiKey, grossAmount, "main")) {
    throw exception("Insufficient funds for withdraw");
  }
  const CurrencyCode currencyCode = grossAmount.currencyCode();

  MonetaryAmount fee(_exchangePublic.queryWithdrawalFee(grossAmount.currencyCode()));
  MonetaryAmount netEmittedAmount = grossAmount - fee;

  CurlPostData opts{
      {"currency", currencyCode.str()}, {"address", wallet.address()}, {"amount", netEmittedAmount.amountStr()}};
  if (wallet.hasTag()) {
    opts.append("memo", wallet.tag());
  }

  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/api/v1/withdrawals", std::move(opts));
  return InitiatedWithdrawInfo(std::move(wallet), result["withdrawalId"].get<std::string_view>(), grossAmount);
}

SentWithdrawInfo KucoinPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  json withdrawJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/withdrawals",
                                   {{"currency", currencyCode.str()}});
  MonetaryAmount netEmittedAmount;
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
      MonetaryAmount fee(withdrawDetail["fee"].get<std::string_view>(), currencyCode);
      if (netEmittedAmount + fee != initiatedWithdrawInfo.grossEmittedAmount()) {
        log::error("{} + {} != {}, maybe a change in API", netEmittedAmount.amountStr(), fee.amountStr(),
                   initiatedWithdrawInfo.grossEmittedAmount().amountStr());
      }
      break;
    }
  }
  return SentWithdrawInfo(netEmittedAmount, isWithdrawSent);
}

bool KucoinPrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                       const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();

  json depositJson = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/deposits",
                                  {{"currency", currencyCode.str()}, {"status", "SUCCESS"}});
  auto itemsIt = depositJson.find("items");
  if (itemsIt == depositJson.end()) {
    throw exception("Unexpected result from Kucoin deposit API");
  }
  RecentDeposit::RecentDepositVector recentDeposits;
  recentDeposits.reserve(itemsIt->size());
  for (const json& depositDetail : *itemsIt) {
    MonetaryAmount amount(depositDetail["amount"].get<std::string_view>(), currencyCode);
    int64_t millisecondsSinceEpoch = depositDetail["updatedAt"].get<int64_t>();

    TimePoint timestamp{std::chrono::milliseconds(millisecondsSinceEpoch)};

    recentDeposits.emplace_back(amount, timestamp);
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  return expectedDeposit.selectClosestRecentDeposit(recentDeposits) != nullptr;
}
}  // namespace cct::api