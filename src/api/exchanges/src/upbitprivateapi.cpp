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
#include "cct_nonce.hpp"
#include "cct_toupper.hpp"
#include "coincenterinfo.hpp"
#include "jsonhelpers.hpp"
#include "monetaryamount.hpp"
#include "ssl_sha.hpp"
#include "tradeoptions.hpp"
#include "upbitpublicapi.hpp"

namespace cct {
namespace api {

namespace {

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, CurlOptions::RequestType requestType,
                  std::string_view method, CurlPostDataT&& curlPostData = CurlPostData()) {
  std::string method_url = UpbitPublic::kUrlBase;
  method_url.append("/v1/");
  method_url.append(method);

  Nonce nonce = Nonce_TimeSinceEpoch();
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData), UpbitPublic::kUserAgent);

  auto jsonWebToken = jwt::create()
                          .set_type("JWT")
                          .set_payload_claim("access_key", jwt::claim(apiKey.key()))
                          .set_payload_claim("nonce", jwt::claim(nonce));

  if (!opts.postdata.empty()) {
    std::string queryHash = ssl::ShaDigest(ssl::ShaType::kSha512, opts.postdata.toString());

    jsonWebToken.set_payload_claim("query_hash", jwt::claim(queryHash))
        .set_payload_claim("query_hash_alg", jwt::claim(std::string("SHA512")));
  }

  std::string token = jsonWebToken.sign(jwt::algorithm::hs256{apiKey.privateKey()});

  opts.httpHeaders.emplace_back("Authorization: Bearer ").append(token);

  json dataJson = json::parse(curlHandle.query(method_url, opts));
  return dataJson;
}
}  // namespace

UpbitPrivate::UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(upbitPublic, config, apiKey),
      _curlHandle(config.exchangeInfo(upbitPublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic) {}

CurrencyExchangeFlatSet UpbitPrivate::TradableCurrenciesFunc::operator()() {
  const CurrencyExchangeFlatSet& partialInfoCurrencies = _exchangePublic._tradableCurrenciesCache.get();
  CurrencyExchangeFlatSet currencies;
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "status/wallet");
  currencies.reserve(partialInfoCurrencies.size());
  for (const json& curDetails : result) {
    CurrencyCode cur(curDetails["currency"].get<std::string_view>());
    if (partialInfoCurrencies.contains(cur)) {
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
        log::info("{} cannot be withdrawn from Upbit", cur.str());
      }
      if (depositStatus == CurrencyExchange::Deposit::kUnavailable) {
        log::info("{} cannot be deposited to Upbit", cur.str());
      }
      currencies.insert(CurrencyExchange(cur, cur, cur, depositStatus, withdrawStatus));
    }
  }
  log::info("Retrieved {} Upbit currencies", currencies.size());
  return currencies;
}

BalancePortfolio UpbitPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "accounts");
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
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "deposits/coin_address", postdata);
  bool generateDepositAddressNeeded = false;
  if (result.contains("error")) {
    std::string_view name = result["error"]["name"].get<std::string_view>();
    std::string_view msg = result["error"]["message"].get<std::string_view>();
    if (name == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one...", currencyCode.str());
      generateDepositAddressNeeded = true;
    } else {
      throw exception("error: " + std::string(name) + "msg = " + std::string(msg));
    }
  }
  if (generateDepositAddressNeeded) {
    json genCoinAddressResult =
        PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "deposits/generate_coin_address", postdata);
    if (genCoinAddressResult.contains("success")) {
      log::info("Successfully generated address");
    }
    result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "deposits/coin_address", postdata);
  }
  if (result["deposit_address"].is_null()) {
    throw exception("Deposit address for " + currencyCode.toString() + " is undefined");
  }
  std::string_view address = result["deposit_address"].get<std::string_view>();
  std::string_view tag;
  if (result.contains("secondary_address") && !result["secondary_address"].is_null()) {
    tag = result["secondary_address"].get<std::string_view>();
  }

  Wallet w(PrivateExchangeName(_exchangePublic.name(), _apiKey.name()), currencyCode, address, tag);
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
    const ExchangeInfo& exchangeInfo = _config.exchangeInfo(_exchangePublic.name());
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

  json placeOrderRes = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "orders", placePostData);

  placeOrderInfo.orderId = placeOrderRes["uuid"];
  placeOrderInfo.orderInfo = parseOrderJson(placeOrderRes, fromCurrencyCode, m);

  // Upbit takes some time to match the market order - We should wait that it has been matched
  bool takerOrderNotClosed = isTakerStrategy && !placeOrderInfo.orderInfo.isClosed;
  while (takerOrderNotClosed) {
    json orderRes =
        PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "order", {{"uuid", placeOrderInfo.orderId}});

    placeOrderInfo.orderInfo = parseOrderJson(orderRes, fromCurrencyCode, m);

    takerOrderNotClosed = !placeOrderInfo.orderInfo.isClosed;
  }
  return placeOrderInfo;
}

OrderInfo UpbitPrivate::cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) {
  CurlPostData postData{{"uuid", orderId}};
  json orderRes = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kDelete, "order", postData);
  bool cancelledOrderClosed = isOrderClosed(orderRes);
  while (!cancelledOrderClosed) {
    orderRes = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "order", postData);
    cancelledOrderClosed = isOrderClosed(orderRes);
  }
  return parseOrderJson(orderRes, tradeInfo.fromCurrencyCode, tradeInfo.m);
}

OrderInfo UpbitPrivate::queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) {
  json orderRes = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "order", {{"uuid", orderId}});
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  return parseOrderJson(orderRes, fromCurrencyCode, tradeInfo.m);
}

json UpbitPrivate::withdrawalInformation(CurrencyCode currencyCode) {
  return PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "withdraws/chance",
                      {{"currency", currencyCode.str()}});
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
      {MonetaryAmount(5000, "KRW", 0), MonetaryAmount(5, "BTC", 4)}};  // 5000 KRW or 0.0005 BTC is min
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
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFees(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{
      {"currency", currencyCode.str()}, {"amount", netEmittedAmount.amountStr()}, {"address", wallet.address()}};
  if (wallet.hasDestinationTag()) {
    withdrawPostData.append("secondary_address", wallet.destinationTag());
  }
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "withdraws/coin", withdrawPostData);
  std::string_view withdrawId(result["uuid"].get<std::string_view>());
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo UpbitPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "withdraw",
                             {{"currency", currencyCode.str()}, {"uuid", initiatedWithdrawInfo.withdrawId()}});
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFees(currencyCode);
  MonetaryAmount realFee(result["fee"].get<std::string_view>(), currencyCode);
  if (realFee != withdrawFee) {
    log::error("{} withdraw fee is {} instead of {}", _exchangePublic.name(), realFee.str(), withdrawFee.str());
  }
  MonetaryAmount netEmittedAmount(result["amount"].get<std::string_view>(), currencyCode);

  std::string_view state(result["state"].get<std::string_view>());
  std::string stateUpperStr;
  std::transform(state.begin(), state.end(), std::back_inserter(stateUpperStr), [](char c) { return cct::toupper(c); });
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
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "deposits",
                             {{"currency", currencyCode.str()}});
  for (const json& trx : result) {
    MonetaryAmount netAmountReceived(trx["amount"].get<std::string_view>(), currencyCode);
    if (netAmountReceived == sentWithdrawInfo.netEmittedAmount()) {
      std::string_view depositState(trx["state"].get<std::string_view>());
      log::debug("Deposit state {}", depositState);
      std::string depositStateUpperStr;
      std::transform(depositState.begin(), depositState.end(), std::back_inserter(depositStateUpperStr),
                     [](char c) { return cct::toupper(c); });
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
