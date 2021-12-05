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
#include "ssl_sha.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"
#include "upbitpublicapi.hpp"

namespace cct {
namespace api {

namespace {

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view method,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  string method_url(UpbitPublic::kUrlBase);
  method_url.append("/v1/");
  method_url.append(method);

  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData), UpbitPublic::kUserAgent);

  auto jsonWebToken = jwt::create()
                          .set_type("JWT")
                          .set_payload_claim("access_key", jwt::claim(std::string(apiKey.key())))
                          .set_payload_claim("nonce", jwt::claim(std::string(Nonce_TimeSinceEpochInMs())));

  if (!opts.postdata.empty()) {
    string queryHash = ssl::ShaDigest(ssl::ShaType::kSha512, opts.postdata.str());

    jsonWebToken.set_payload_claim("query_hash", jwt::claim(std::string(queryHash)))
        .set_payload_claim("query_hash_alg", jwt::claim(std::string("SHA512")));
  }

  // hs256 does not accept std::string_view, we need a copy...
  string token = jsonWebToken.sign(jwt::algorithm::hs256{std::string(apiKey.privateKey())});

  opts.httpHeaders.emplace_back("Authorization: Bearer ").append(token);

  json dataJson = json::parse(curlHandle.query(method_url, opts));
  if (dataJson.contains("error")) {
    const json& errorPart = dataJson["error"];
    if (errorPart.contains("name")) {
      throw exception(errorPart["name"].get<std::string_view>());
    }
    if (errorPart.contains("message")) {
      throw exception(errorPart["message"].get<std::string_view>());
    }
    throw exception("Unknown Upbit API error message");
  }
  return dataJson;
}
}  // namespace

UpbitPrivate::UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(upbitPublic, config, apiKey),
      _curlHandle(config.metricGatewayPtr(), config.exchangeInfo(upbitPublic.name()).minPrivateQueryDelay(),
                  config.getRunMode()),
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
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "status/wallet");
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
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "accounts");
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
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "deposits/coin_address", postdata);
  bool generateDepositAddressNeeded = false;
  if (result.contains("error")) {
    std::string_view name = result["error"]["name"].get<std::string_view>();
    std::string_view msg = result["error"]["message"].get<std::string_view>();
    if (name == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one...", currencyCode.str());
      generateDepositAddressNeeded = true;
    } else {
      throw exception("error: " + string(name) + "msg = " + string(msg));
    }
  }
  if (generateDepositAddressNeeded) {
    json genCoinAddressResult =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "deposits/generate_coin_address", postdata);
    if (genCoinAddressResult.contains("success")) {
      log::info("Successfully generated address");
    }
    result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "deposits/coin_address", postdata);
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

PlaceOrderInfo UpbitPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  const CurrencyCode toCurrencyCode(tradeInfo.toCurrencyCode);
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy();
  const bool isSimulation = tradeInfo.options.isSimulation();
  const Market m = tradeInfo.m;

  const std::string_view askOrBid = fromCurrencyCode == m.base() ? "ask" : "bid";
  const std::string_view orderType = isTakerStrategy ? (fromCurrencyCode == m.base() ? "market" : "price") : "limit";

  CurlPostData placePostData{{"market", m.reverse().assetsPairStr('-')}, {"side", askOrBid}, {"ord_type", orderType}};

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));
  if (isSimulation) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
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

  json placeOrderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "orders", placePostData);

  placeOrderInfo.orderId = placeOrderRes["uuid"];
  placeOrderInfo.orderInfo = parseOrderJson(placeOrderRes, fromCurrencyCode, m);

  // Upbit takes some time to match the market order - We should wait that it has been matched
  bool takerOrderNotClosed = isTakerStrategy && !placeOrderInfo.orderInfo.isClosed;
  while (takerOrderNotClosed) {
    json orderRes =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "order", {{"uuid", placeOrderInfo.orderId}});

    placeOrderInfo.orderInfo = parseOrderJson(orderRes, fromCurrencyCode, m);

    takerOrderNotClosed = !placeOrderInfo.orderInfo.isClosed;
  }
  return placeOrderInfo;
}

OrderInfo UpbitPrivate::cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) {
  CurlPostData postData{{"uuid", orderId}};
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "order", postData);
  bool cancelledOrderClosed = isOrderClosed(orderRes);
  while (!cancelledOrderClosed) {
    orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "order", postData);
    cancelledOrderClosed = isOrderClosed(orderRes);
  }
  return parseOrderJson(orderRes, tradeInfo.fromCurrencyCode, tradeInfo.m);
}

OrderInfo UpbitPrivate::queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) {
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "order", {{"uuid", orderId}});
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  return parseOrderJson(orderRes, fromCurrencyCode, tradeInfo.m);
}

MonetaryAmount UpbitPrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "withdraws/chance", {{"currency", currencyCode.str()}});
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

InitiatedWithdrawInfo UpbitPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{
      {"currency", currencyCode.str()}, {"amount", netEmittedAmount.amountStr()}, {"address", wallet.address()}};
  if (wallet.hasTag()) {
    withdrawPostData.append("secondary_address", wallet.tag());
  }
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "withdraws/coin", withdrawPostData);
  std::string_view withdrawId(result["uuid"].get<std::string_view>());
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo UpbitPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "withdraw",
                             {{"currency", currencyCode.str()}, {"uuid", initiatedWithdrawInfo.withdrawId()}});
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  MonetaryAmount realFee(result["fee"].get<std::string_view>(), currencyCode);
  if (realFee != withdrawFee) {
    log::error("{} withdraw fee is {} instead of {}", _exchangePublic.name(), realFee.str(), withdrawFee.str());
  }
  MonetaryAmount netEmittedAmount(result["amount"].get<std::string_view>(), currencyCode);

  std::string_view state(result["state"].get<std::string_view>());
  string stateUpperStr;
  std::transform(state.begin(), state.end(), std::back_inserter(stateUpperStr), [](char c) { return toupper(c); });
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
  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "deposits", {{"currency", currencyCode.str()}});
  for (const json& trx : result) {
    MonetaryAmount netAmountReceived(trx["amount"].get<std::string_view>(), currencyCode);
    if (netAmountReceived == sentWithdrawInfo.netEmittedAmount()) {
      std::string_view depositState(trx["state"].get<std::string_view>());
      log::debug("Deposit state {}", depositState);
      string depositStateUpperStr;
      std::transform(depositState.begin(), depositState.end(), std::back_inserter(depositStateUpperStr),
                     [](char c) { return toupper(c); });
      if (depositStateUpperStr == "ACCEPTED") {
        return true;
      }
    }
    log::debug("Deposit {} with amount {} is similar, but different amount than {}",
               trx["refid"].get<std::string_view>(), netAmountReceived.str(),
               sentWithdrawInfo.netEmittedAmount().str());
  }
  return false;
}

}  // namespace api
}  // namespace cct
