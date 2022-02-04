#include "krakenprivateapi.hpp"

#include <cassert>
#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "krakenpublicapi.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"

namespace cct::api {
namespace {

string PrivateSignature(const APIKey& apiKey, string data, const Nonce& nonce, std::string_view postdata) {
  // concatenate nonce and postdata and compute SHA256
  string noncePostData(nonce.begin(), nonce.end());
  noncePostData.append(postdata);
  ssl::Sha256 nonce_postdata = ssl::ComputeSha256(noncePostData);

  // concatenate path and nonce_postdata (path + ComputeSha256(nonce + postdata))
  data.append(nonce_postdata.begin(), nonce_postdata.end());

  // and compute HMAC
  return B64Encode(ssl::ShaBin(ssl::ShaType::kSha512, data, B64Decode(apiKey.privateKey())));
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view method,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  string path(std::string_view(KrakenPublic::kUrlBase.end() - 2, KrakenPublic::kUrlBase.end()));  // Take /<Version>
  path.append(method);

  CurlOptions opts(HttpRequestType::kPost, std::forward<CurlPostDataT>(curlPostData), KrakenPublic::kUserAgent);

  Nonce nonce = Nonce_TimeSinceEpochInMs();
  opts.getPostData().append("nonce", nonce);
  opts.appendHttpHeader("API-Key", apiKey.key());
  opts.appendHttpHeader("API-Sign", PrivateSignature(apiKey, path, nonce, opts.getPostData().str()));

  string ret = curlHandle.query(method, opts);
  json jsonData = json::parse(std::move(ret));
  Duration sleepingTime = curlHandle.minDurationBetweenQueries();
  while (jsonData.contains("error") && !jsonData["error"].empty() &&
         jsonData["error"].front().get<std::string_view>() == "EAPI:Rate limit exceeded") {
    log::error("Kraken private API rate limit exceeded");
    sleepingTime *= 2;
    log::debug("Wait {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
    std::this_thread::sleep_for(sleepingTime);

    // We need to update the nonce
    nonce = Nonce_TimeSinceEpochInMs();
    opts.getPostData().set("nonce", nonce);
    opts.setHttpHeader("API-Sign", PrivateSignature(apiKey, path, nonce, opts.getPostData().str()));
    ret = curlHandle.query(method, opts);
    jsonData = json::parse(std::move(ret));
  }
  auto errorIt = jsonData.find("error");
  if (errorIt != jsonData.end() && !errorIt->empty()) {
    std::string_view msg = errorIt->front().get<std::string_view>();
    if (method.ends_with("CancelOrder") && msg == "EOrder:Unknown order") {
      log::warn("Unknown order from Kraken CancelOrder. Assuming closed order");
      jsonData = "{\" error \":[],\" result \":{\" count \":1}}"_json;
    } else {
      string ex("Kraken private query error: ");
      ex.append(msg);
      throw exception(std::move(ex));
    }
  }
  return jsonData["result"];
}
}  // namespace

KrakenPrivate::KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey)
    : ExchangePrivate(config, krakenPublic, apiKey),
      _curlHandle(KrakenPublic::kUrlBase, config.metricGatewayPtr(), exchangeInfo().privateAPIRate(),
                  config.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, krakenPublic) {}

CurrencyExchangeFlatSet KrakenPrivate::queryTradableCurrencies() { return _exchangePublic.queryTradableCurrencies(); }

BalancePortfolio KrakenPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  BalancePortfolio balancePortfolio;
  json res = PrivateQuery(_curlHandle, _apiKey, "/private/Balance");
  // Kraken returns an empty array in case of account with no balance at all
  for (const auto& [curCode, amountStr] : res.items()) {
    string amount = amountStr;
    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(curCode));

    addBalance(balancePortfolio, MonetaryAmount(std::move(amount), currencyCode), equiCurrency);
  }
  return balancePortfolio;
}

Wallet KrakenPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  json res = PrivateQuery(_curlHandle, _apiKey, "/private/DepositMethods", {{"asset", krakenCurrency.altStr()}});
  if (res.empty()) {
    throw exception("No deposit method found on Kraken for " + string(currencyCode.str()));
  }
  // Don't keep a view on 'method' value, we will override json data just below. We can just steal the string.
  string method = std::move(res.front()["method"].get_ref<string&>());
  res = PrivateQuery(_curlHandle, _apiKey, "/private/DepositAddresses",
                     {{"asset", krakenCurrency.altStr()}, {"method", method}});
  if (res.empty()) {
    // This means user has not created a wallet yet, but it's possible to do it via DepositMethods query above.
    log::warn("No deposit address found on {} for {}, creating a new one", _exchangePublic.name(), currencyCode.str());
    res = PrivateQuery(_curlHandle, _apiKey, "/private/DepositAddresses",
                       {{"asset", krakenCurrency.altStr()}, {"method", method}, {"new", "true"}});
    if (res.empty()) {
      string err("Cannot create a new deposit address on Kraken for ");
      err.append(currencyCode.str());
      throw exception(std::move(err));
    }
  }
  PrivateExchangeName privateExchangeName(_exchangePublic.name(), _apiKey.name());

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(privateExchangeName.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  string address, tag;
  for (const json& depositDetail : res) {
    for (const auto& [keyStr, valueStr] : depositDetail.items()) {
      if (keyStr == "address") {
        address = valueStr;
      } else if (keyStr == "expiretm") {
        if (valueStr.is_number_integer()) {  // WARNING: when new = true, expiretm is not a string, but a number!
          int64_t expireTmValue = valueStr.get<int64_t>();
          if (expireTmValue != 0) {
            log::warn("{} wallet has an expire time of {}", _exchangePublic.name(), expireTmValue);
          }
        } else if (valueStr.is_string()) {
          std::string_view expireTmValue = valueStr.get<std::string_view>();
          if (expireTmValue != "0") {
            log::warn("{} wallet has an expire time of {}", _exchangePublic.name(), expireTmValue);
          }
        } else {
          throw exception("Cannot retrieve 'expiretm' field of Kraken deposit address");
        }

      } else if (keyStr == "new") {
        // Never used, it's ok, safely pass this
      } else {
        // Heuristic: this last field may change key name and is optional (tag for XRP, memo for EOS for instance)
        if (!tag.empty()) {
          throw exception("Tag already set / unknown key information for " + string(currencyCode.str()));
        }
        if (valueStr.is_number_integer()) {
          SetString(tag, static_cast<long>(valueStr));
        } else {
          tag = valueStr.get<string>();
        }
      }
    }
    if (Wallet::ValidateWallet(walletCheck, privateExchangeName, currencyCode, address, tag)) {
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
    address.clear();
    tag.clear();
  }

  Wallet w(std::move(privateExchangeName), currencyCode, std::move(address), std::move(tag), walletCheck);
  log::info("Retrieved {}", w.str());
  return w;
}

ExchangePrivate::Orders KrakenPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  json data = PrivateQuery(_curlHandle, _apiKey, "/private/OpenOrders", {{"trades", "true"}});
  auto openedPartIt = data.find("open");
  Orders openedOrders;
  if (openedPartIt != data.end()) {
    ExchangePublic::MarketSet markets;

    for (const auto& [id, orderDetails] : openedPartIt->items()) {
      std::string_view marketStr = orderDetails["descr"]["pair"].get<std::string_view>();

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

      if (!openedOrdersConstraints.validateOrderId(id)) {
        continue;
      }

      MonetaryAmount originalVolume(orderDetails["vol"].get<std::string_view>(), volumeCur);
      MonetaryAmount matchedVolume(orderDetails["vol_exec"].get<std::string_view>(), volumeCur);
      MonetaryAmount remainingVolume = originalVolume - matchedVolume;
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
      TradeSide side =
          orderDetails["descr"]["type"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

      int64_t secondsSinceEpoch = static_cast<int64_t>(orderDetails["opentm"].get<double>());

      TimePoint placedTime{std::chrono::seconds(secondsSinceEpoch)};
      if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      openedOrders.emplace_back(id, matchedVolume, remainingVolume, price, placedTime, side);
    }
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

void KrakenPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.noConstraints()) {
    PrivateQuery(_curlHandle, _apiKey, "/private/CancelAll");
    return;
  }
  for (const Order& o : queryOpenedOrders(openedOrdersConstraints)) {
    cancelOrderProcess(o.id());
  }
}

PlaceOrderInfo KrakenPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                         const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.toCur());
  const bool isTakerStrategy =
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeInfo().placeSimulateRealOrder());
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
  CurlPostData placePostData{{"pair", krakenMarket.assetsPairStrUpper()},
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

  json placeOrderRes = PrivateQuery(_curlHandle, _apiKey, "/private/AddOrder", std::move(placePostData));
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
      orderDescriptionStr.begin() + orderDescriptionStr.find(' ', orderType.size() + 1));
  MonetaryAmount krakenVolume(krakenTruncatedAmount, m.base());
  log::debug("Kraken adjusted volume: {}", krakenVolume.str());

  placeOrderInfo.orderInfo =
      queryOrderInfo(tradeInfo.createOrderRef(placeOrderInfo.orderId),
                     isTakerStrategy ? QueryOrder::kClosedThenOpened : QueryOrder::kOpenedThenClosed);

  return placeOrderInfo;
}

OrderInfo KrakenPrivate::cancelOrder(const OrderRef& orderRef) {
  cancelOrderProcess(orderRef.id);
  return queryOrderInfo(orderRef, QueryOrder::kClosedThenOpened);
}

void KrakenPrivate::cancelOrderProcess(const OrderId& id) {
  PrivateQuery(_curlHandle, _apiKey, "/private/CancelOrder", {{"txid", id}});
}

OrderInfo KrakenPrivate::queryOrderInfo(const OrderRef& orderRef, QueryOrder queryOrder) {
  const CurrencyCode fromCurrencyCode = orderRef.fromCur();
  const CurrencyCode toCurrencyCode = orderRef.toCur();
  const Market m = orderRef.m;

  json ordersRes = queryOrdersData(orderRef.userRef, orderRef.id, queryOrder);
  auto openIt = ordersRes.find("open");
  const bool orderInOpenedPart = openIt != ordersRes.end() && openIt->contains(orderRef.id);
  const json& orderJson = orderInOpenedPart ? (*openIt)[orderRef.id] : ordersRes["closed"][orderRef.id];
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

json KrakenPrivate::queryOrdersData(int64_t userRef, const OrderId& orderId, QueryOrder queryOrder) {
  static constexpr int kNbMaxRetriesQueryOrders = 10;
  int nbRetries = 0;
  CurlPostData ordersPostData{{"trades", "true"}, {"userref", userRef}};
  const bool isOpenedFirst = queryOrder == QueryOrder::kOpenedThenClosed;
  const std::string_view firstQueryFullName = isOpenedFirst ? "/private/OpenOrders" : "/private/ClosedOrders";
  do {
    json data = PrivateQuery(_curlHandle, _apiKey, firstQueryFullName, ordersPostData);
    const json& firstOrders = data[isOpenedFirst ? "open" : "closed"];
    bool foundOrder = firstOrders.contains(orderId);
    if (!foundOrder) {
      const std::string_view secondQueryFullName = isOpenedFirst ? "/private/ClosedOrders" : "/private/OpenOrders";
      data.update(PrivateQuery(_curlHandle, _apiKey, secondQueryFullName, ordersPostData));
      const json& secondOrders = data[isOpenedFirst ? "closed" : "open"];
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
  std::ranges::transform(krakenWalletName, krakenWalletName.begin(), tolower);

  json withdrawData = PrivateQuery(
      _curlHandle, _apiKey, "/private/Withdraw",
      {{"amount", grossAmount.amountStr()}, {"asset", krakenCurrency.altStr()}, {"key", krakenWalletName}});

  // {"refid":"BSH3QF5-TDIYVJ-X6U74X"}
  std::string_view withdrawId = withdrawData["refid"].get<std::string_view>();
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo KrakenPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  json trxList = PrivateQuery(_curlHandle, _apiKey, "/private/WithdrawStatus", {{"asset", krakenCurrency.altStr()}});
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
  json trxList = PrivateQuery(_curlHandle, _apiKey, "/private/DepositStatus", {{"asset", krakenCurrency.altStr()}});
  RecentDeposit::RecentDepositVector recentDeposits;
  for (const json& trx : trxList) {
    std::string_view status(trx["status"].get<std::string_view>());
    if (status != "Success") {
      log::debug("Deposit {} status {}", trx["refid"].get<std::string_view>(), status);
      continue;
    }
    MonetaryAmount amount(trx["amount"].get<std::string_view>(), currencyCode);
    int64_t secondsSinceEpoch = trx["time"].get<int64_t>();
    TimePoint timestamp{std::chrono::seconds(secondsSinceEpoch)};

    recentDeposits.emplace_back(amount, timestamp);
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  return expectedDeposit.selectClosestRecentDeposit(recentDeposits) != nullptr;
}

}  // namespace cct::api
