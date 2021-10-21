#include "krakenprivateapi.hpp"

#include <cassert>
#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_nonce.hpp"
#include "coincenterinfo.hpp"
#include "krakenpublicapi.hpp"
#include "ssl_sha.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
namespace {

string PrivateSignature(const APIKey& apiKey, string data, const Nonce& nonce, std::string_view postdata) {
  // concatenate nonce and postdata and compute SHA256
  string noncePostData(nonce.begin(), nonce.end());
  noncePostData.append(postdata);
  ssl::Sha256 nonce_postdata = ssl::ComputeSha256(noncePostData);

  // concatenate path and nonce_postdata (path + ComputeSha256(nonce + postdata))
  data.append(std::begin(nonce_postdata), std::end(nonce_postdata));

  // and compute HMAC
  return B64Encode(ssl::ShaBin(ssl::ShaType::kSha512, data, B64Decode(apiKey.privateKey()).c_str()));
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view method,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  string path;
  path.reserve(method.size() + 11);
  path.push_back('/');
  path.push_back(KrakenPublic::kVersion);
  path.append("/private/");
  path.append(method);

  string method_url = KrakenPublic::kUrlBase + path;

  CurlOptions opts(CurlOptions::RequestType::kPost, std::forward<CurlPostDataT>(curlPostData));
  opts.userAgent = KrakenPublic::kUserAgent;

  Nonce nonce = Nonce_TimeSinceEpoch();
  opts.postdata.append("nonce", std::string_view(nonce.begin(), nonce.end()));
  opts.httpHeaders.reserve(2);
  opts.httpHeaders.emplace_back("API-Key: ").append(apiKey.key());
  opts.httpHeaders.emplace_back("API-Sign: " + PrivateSignature(apiKey, path, nonce, opts.postdata.str()));

  string ret = curlHandle.query(method_url, opts);
  json jsonData = json::parse(std::move(ret));
  CurlHandle::Clock::duration sleepingTime = curlHandle.minDurationBetweenQueries();
  while (jsonData.contains("error") && !jsonData["error"].empty() &&
         jsonData["error"].front() == "EAPI:Rate limit exceeded") {
    log::error("Kraken private API rate limit exceeded");
    sleepingTime *= 2;
    log::debug("Wait {} ms...", std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
    std::this_thread::sleep_for(sleepingTime);

    // We need to update the nonce
    nonce = Nonce_TimeSinceEpoch();
    opts.postdata.set("nonce", std::string_view(nonce.begin(), nonce.end()));
    opts.httpHeaders.back() = "API-Sign: " + PrivateSignature(apiKey, path, nonce, opts.postdata.str());
    ret = curlHandle.query(method_url, opts);
    jsonData = json::parse(std::move(ret));
  }
  if (jsonData.contains("error") && !jsonData["error"].empty()) {
    if (method == "CancelOrder" && jsonData["error"].front() == "EOrder:Unknown order") {
      log::warn("Unknown order from Kraken CancelOrder. Assuming closed order");
      jsonData = "{\" error \":[],\" result \":{\" count \":1}}"_json;
    } else {
      throw exception("Kraken private query error: " + string(jsonData["error"].front()));
    }
  }
  return jsonData["result"];
}
}  // namespace

KrakenPrivate::KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey)
    : ExchangePrivate(krakenPublic, config, apiKey),
      _curlHandle(config.exchangeInfo("kraken").minPrivateQueryDelay(), config.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, krakenPublic) {}

CurrencyExchangeFlatSet KrakenPrivate::queryTradableCurrencies() { return _exchangePublic.queryTradableCurrencies(); }

BalancePortfolio KrakenPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  BalancePortfolio balancePortfolio;
  json res = PrivateQuery(_curlHandle, _apiKey, "Balance");
  // Kraken returns an empty array in case of account with no balance at all
  for (const auto& [curCode, amountStr] : res.items()) {
    string amount = amountStr;
    CurrencyCode currencyCode(_config.standardizeCurrencyCode(curCode));

    addBalance(balancePortfolio, MonetaryAmount(std::move(amount), currencyCode), equiCurrency);
  }
  log::info("Retrieved {} balance for {} assets", _exchangePublic.name(), balancePortfolio.size());
  return balancePortfolio;
}

Wallet KrakenPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  json res = PrivateQuery(_curlHandle, _apiKey, "DepositMethods", {{"asset", krakenCurrency.altStr()}});
  // [ { "fee": "0.0000000000", "gen-address": true, "limit": false, "method": "Bitcoin"}]
  if (res.empty()) {
    throw exception("No deposit method found on Kraken for " + string(currencyCode.str()));
  }
  const string method = res.front()["method"];
  res =
      PrivateQuery(_curlHandle, _apiKey, "DepositAddresses", {{"asset", krakenCurrency.altStr()}, {"method", method}});
  if (res.empty()) {
    // This means user has not created a wallet yet, but it's possible to do it via DepositMethods query above.
    log::warn("No deposit address found on {} for {}, creating a new one...", _exchangePublic.name(),
              currencyCode.str());
    res = PrivateQuery(_curlHandle, _apiKey, "DepositAddresses",
                       {{"asset", krakenCurrency.altStr()}, {"method", method}, {"new", "true"}});
    if (res.empty()) {
      throw exception("Cannot create a new deposit address on Kraken for " + string(currencyCode.str()));
    }
  }
  PrivateExchangeName privateExchangeName(_exchangePublic.name(), _apiKey.name());

  string address, tag;
  for (const json& depositDetail : res) {
    for (const auto& [keyStr, valueStr] : depositDetail.items()) {
      if (keyStr == "address") {
        address = valueStr;
      } else if (keyStr == "expiretm") {
        if (valueStr != "0") {
          log::error("{} wallet has an expire time of {}", _exchangePublic.name(), valueStr);
        }
      } else if (keyStr == "new") {
        // Never used, it's ok, safely pass this
      } else {
        // Heuristic: this last field may change key name and is optional (tag for XRP, memo for EOS for instance)
        if (!tag.empty()) {
          throw exception("Tag already set / unknown key information for " + string(currencyCode.str()));
        }
        if (valueStr.is_number_integer()) {
          tag = std::to_string(static_cast<long>(valueStr));
        } else {
          tag = valueStr.get<string>();
        }
      }
    }
    if (Wallet::IsAddressPresentInDepositFile(privateExchangeName, currencyCode, address, tag)) {
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
    tag.clear();
  }

  Wallet w(privateExchangeName, currencyCode, address, tag);
  log::info("Retrieved {}", w.str());
  return w;
}

PlaceOrderInfo KrakenPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                         const TradeInfo& tradeInfo) {
  using Clock = TradeOptions::Clock;
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  const CurrencyCode toCurrencyCode(tradeInfo.toCurrencyCode);
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy();
  const bool isSimulation = tradeInfo.options.isSimulation();
  const Market m = tradeInfo.m;
  KrakenPublic& krakenPublic = dynamic_cast<KrakenPublic&>(_exchangePublic);
  const MonetaryAmount orderMin = krakenPublic.queryVolumeOrderMin(m);
  CurrencyExchange krakenCurrencyBase = _exchangePublic.convertStdCurrencyToCurrencyExchange(m.base());
  CurrencyExchange krakenCurrencyQuote = _exchangePublic.convertStdCurrencyToCurrencyExchange(m.quote());
  Market krakenMarket(krakenCurrencyBase.altStr(), krakenCurrencyQuote.altStr());
  const std::string_view orderType = fromCurrencyCode == m.base() ? "sell" : "buy";

  auto volAndPriNbDecimals = krakenPublic._marketsCache.get().second.find(m)->second.volAndPriNbDecimals;

  price.truncate(volAndPriNbDecimals.priNbDecimals);

  // volume in quote currency (viqc) is not available (as of March 2021), receiving error 'EAPI:Feature disabled:viqc'
  // We have to compute the amount manually (always in base currency)
  volume.truncate(volAndPriNbDecimals.volNbDecimals);

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));
  if (volume < orderMin) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(), toCurrencyCode.str(),
              orderMin.str());
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  // minimum expire time tested on my side was 5 seconds. I chose 10 seconds just to be sure that we will not have any
  // problem.
  const int maxTradeTimeInSeconds =
      static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(tradeInfo.options.maxTradeTime()).count());
  const int expireTimeInSeconds = std::max(10, maxTradeTimeInSeconds);

  const auto nbSecondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();

  // oflags: Ask fee in destination currency.
  // This will not work if user has enough Kraken Fee Credits (in this case, they will be used instead).
  // Warning: this does not change the currency of the returned fee from Kraken in the get Closed / Opened orders,
  // which will be always in quote currency (as per the documentation)
  CurlPostData placePostData{{"pair", krakenMarket.assetsPairStr()},
                             {"type", orderType},
                             {"ordertype", isTakerStrategy ? "market" : "limit"},
                             {"price", price.amountStr()},
                             {"volume", volume.amountStr()},
                             {"oflags", fromCurrencyCode == m.quote() ? "fcib" : "fciq"},
                             {"expiretm", nbSecondsSinceEpoch + expireTimeInSeconds},
                             {"userref", tradeInfo.userRef}};
  if (isSimulation) {
    placePostData.append("validate", "true");  // validate inputs only. do not submit order (optional)
  }

  json placeOrderRes = PrivateQuery(_placeCancelOrder, _apiKey, "AddOrder", placePostData);
  // {"error":[],"result":{"descr":{"order":"buy 24.69898116 XRPETH @ limit 0.0003239"},"txid":["OWBA44-TQZQ7-EEYSXA"]}}
  if (isSimulation) {
    // In simulation mode, there is no txid returned. If we arrived here (after CollectResults) we assume that the call
    // to api was a success.
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  placeOrderInfo.orderId = placeOrderRes["txid"].front();

  // Kraken will automatically truncate the decimals to the maximum allowed for the trade assets. Get this information
  // and adjust our amount.
  std::string_view orderDescriptionStr = placeOrderRes["descr"]["order"].get<std::string_view>();
  std::string_view krakenTruncatedAmount(
      orderDescriptionStr.begin() + orderType.size() + 1,
      orderDescriptionStr.begin() + orderDescriptionStr.find_first_of(' ', orderType.size() + 1));
  MonetaryAmount krakenVolume(krakenTruncatedAmount, m.base());
  log::debug("Kraken adjusted volume: {}", krakenVolume.str());

  placeOrderInfo.orderInfo =
      queryOrderInfo(placeOrderInfo.orderId, tradeInfo,
                     isTakerStrategy ? QueryOrder::kClosedThenOpened : QueryOrder::kOpenedThenClosed);

  return placeOrderInfo;
}

OrderInfo KrakenPrivate::cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) {
  PrivateQuery(_placeCancelOrder, _apiKey, "CancelOrder", {{"txid", orderId}});
  // {"error":[],"result":{"count":1}}
  return queryOrderInfo(orderId, tradeInfo, QueryOrder::kClosedThenOpened);
}

OrderInfo KrakenPrivate::queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo, QueryOrder queryOrder) {
  const CurrencyCode fromCurrencyCode = tradeInfo.fromCurrencyCode;
  const CurrencyCode toCurrencyCode = tradeInfo.toCurrencyCode;
  const Market m = tradeInfo.m;

  json ordersRes = queryOrdersData(m, fromCurrencyCode, tradeInfo.userRef, orderId, queryOrder);
  const bool orderInOpenedPart = ordersRes.contains("open") && ordersRes["open"].contains(orderId);
  const json& orderJson = orderInOpenedPart ? ordersRes["open"][orderId] : ordersRes["closed"][orderId];
  MonetaryAmount vol(orderJson["vol"].get<std::string_view>(), m.base());             // always in base currency
  MonetaryAmount tradedVol(orderJson["vol_exec"].get<std::string_view>(), m.base());  // always in base currency
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode), !orderInOpenedPart);
  // Avoid division by 0 as the price is returned as 0.
  if (!tradedVol.isZero()) {
    MonetaryAmount tradedCost(orderJson["cost"].get<std::string_view>(), m.quote());  // always in quote currency
    MonetaryAmount fee(orderJson["fee"].get<std::string_view>(), m.quote());          // always in quote currency

    if (fromCurrencyCode == m.quote()) {
      MonetaryAmount price(orderJson["price"].get<std::string_view>(), m.base());
      orderInfo.tradedAmounts.tradedFrom += tradedCost;
      orderInfo.tradedAmounts.tradedTo += (tradedCost - fee).toNeutral() / price;
    } else {
      orderInfo.tradedAmounts.tradedFrom += tradedVol;
      orderInfo.tradedAmounts.tradedTo += tradedCost - fee;
    }
  }

  return orderInfo;
}

json KrakenPrivate::queryOrdersData(Market, CurrencyCode, std::string_view userRef, const OrderId& orderId,
                                    QueryOrder queryOrder) {
  constexpr int kNbMaxRetriesQueryOrders = 10;
  int nbRetries = 0;
  CurlPostData ordersPostData{{"trades", "true"}, {"userref", userRef}};
  const bool kOpenedFirst = queryOrder == QueryOrder::kOpenedThenClosed;
  const std::string_view kFirstQueryFullName = kOpenedFirst ? "OpenOrders" : "ClosedOrders";
  do {
    json data = PrivateQuery(_curlHandle, _apiKey, kFirstQueryFullName, ordersPostData);
    /*
     {"error":[],"result":{"closed":{"OFA3RW-ZJ5OF-RLZ2N5":{"refid":null,"userref":1616887973,"status":"closed","reason":null,"opentm":1616887987.0551,"closetm":1616887987.0562,"starttm":0,"expiretm":1616888001,"descr":{"pair":"XRPETH","type":"buy","ordertype":"market","price":"0","price2":"0","leverage":"none","order":"buy
     24.96099843 XRPETH @
     market","close":""},"vol":"24.96099843","vol_exec":"24.96099843","cost":"0.0080124","fee":"0.0000208","price":"0.0003210","stopprice":"0.0000000000","limitprice":"0.0000000000","misc":"","oflags":"fciq","trades":["TLEW2Y-T6E5D-FFS5L6"]},"OWPCKX-UKXP4-7RSVOE":{"refid":null,"userref":1616887973,"status":"canceled","reason":"User
     requested","opentm":1616887973.6032,"closetm":1616887986.248,"starttm":0,"expiretm":1616887988,"descr":{"pair":"XRPETH","type":"buy","ordertype":"limit","price":"0.0003205","price2":"0","leverage":"none","order":"buy
     24.96099843 XRPETH @ limit
     0.0003205","close":""},"vol":"24.96099843","vol_exec":"0.00000000","cost":"0.0000000000","fee":"0.0000000000","price":"0.0000000000","stopprice":"0.0000000000","limitprice":"0.0000000000","misc":"","oflags":"fciq"}},"count":2}}
    */
    const json& firstOrders = data[kOpenedFirst ? "open" : "closed"];
    bool foundOrder = firstOrders.contains(orderId);
    if (!foundOrder) {
      const std::string_view kSecondQueryFullName = kOpenedFirst ? "ClosedOrders" : "OpenOrders";
      data.update(PrivateQuery(_curlHandle, _apiKey, kSecondQueryFullName, ordersPostData));
      const json& secondOrders = data[kOpenedFirst ? "closed" : "open"];
      foundOrder = secondOrders.contains(orderId);
    }

    if (!foundOrder) {
      if (++nbRetries < kNbMaxRetriesQueryOrders) {
        log::warn("{} is not present in opened nor closed orders, retry {}", orderId, nbRetries);
        continue;
      }
      throw exception("I lost contact with Kraken order " + orderId);
    }
    return data;

  } while (true);
}

InitiatedWithdrawInfo KrakenPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);

  string krakenWalletName(wallet.exchangeName());
  krakenWalletName.push_back('_');
  krakenWalletName.append(currencyCode.str());
  std::transform(std::begin(krakenWalletName), std::end(krakenWalletName), krakenWalletName.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  json withdrawData = PrivateQuery(
      _curlHandle, _apiKey, "Withdraw",
      {{"amount", grossAmount.amountStr()}, {"asset", krakenCurrency.altStr()}, {"key", krakenWalletName}});

  // {"refid":"BSH3QF5-TDIYVJ-X6U74X"}
  std::string_view withdrawId = withdrawData["refid"].get<std::string_view>();
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo KrakenPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  CurlPostData checkWithdrawPostData{{"asset", krakenCurrency.altStr()}};
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  json trxList = PrivateQuery(_curlHandle, _apiKey, "WithdrawStatus", checkWithdrawPostData);
  for (const json& trx : trxList) {
    std::string_view withdrawId = trx["refid"].get<std::string_view>();
    if (withdrawId == initiatedWithdrawInfo.withdrawId()) {
      MonetaryAmount realFee(trx["fee"].get<std::string_view>(), currencyCode);
      if (realFee != withdrawFee) {
        log::warn("Kraken withdraw fee is {} instead of parsed {}", realFee.str(), withdrawFee.str());
      }
      std::string_view status = trx["status"].get<std::string_view>();
      MonetaryAmount netWithdrawAmount(trx["amount"].get<std::string_view>(), currencyCode);
      return SentWithdrawInfo(netWithdrawAmount, status == "Success");
    }
  }
  throw exception("Kraken: unable to find withdrawal confirmation of " +
                  initiatedWithdrawInfo.grossEmittedAmount().str());
}

bool KrakenPrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                       const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  CurlPostData checkDepositPostData{{"asset", krakenCurrency.altStr()}};
  json trxList = PrivateQuery(_curlHandle, _apiKey, "DepositStatus", checkDepositPostData);
  for (const json& trx : trxList) {
    std::string_view status(trx["status"].get<std::string_view>());
    if (status != "Success") {
      log::debug("Deposit {} status {}", trx["refid"].get<std::string_view>(), status);
      continue;
    }
    MonetaryAmount netAmountReceived(trx["amount"].get<std::string_view>(), currencyCode);
    if (netAmountReceived == sentWithdrawInfo.netEmittedAmount()) {
      return true;
    }
    log::debug("Deposit {} with amount {} is similar, but different amount than {}",
               trx["refid"].get<std::string_view>(), netAmountReceived.str(),
               sentWithdrawInfo.netEmittedAmount().str());
  }
  return false;
}

}  // namespace api
}  // namespace cct
