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
#include "tradeoptionsapi.hpp"

namespace cct {
namespace api {
namespace {

std::string PrivateSignature(const APIKey& apiKey, std::string data, const Nonce& nonce, std::string_view postdata) {
  // concatenate nonce and postdata and compute SHA256
  std::string noncePostData(nonce.begin(), nonce.end());
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
  std::string path;
  path.reserve(method.size() + 11);
  path.push_back('/');
  path.push_back(KrakenPublic::kVersion);
  path.append("/private/");
  path.append(method);

  std::string method_url = KrakenPublic::kUrlBase + path;

  CurlOptions opts(CurlOptions::RequestType::kPost, std::forward<CurlPostDataT>(curlPostData));
  opts.userAgent = KrakenPublic::kUserAgent;

  Nonce nonce = Nonce_TimeSinceEpoch();
  opts.postdata.append("nonce", std::string_view(nonce.begin(), nonce.end()));
  opts.httpHeaders.reserve(2);
  opts.httpHeaders.emplace_back("API-Key: ").append(apiKey.key());
  opts.httpHeaders.emplace_back("API-Sign: " + PrivateSignature(apiKey, path, nonce, opts.postdata.toStringView()));

  std::string ret = curlHandle.query(method_url, opts);
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
    opts.httpHeaders.back() = "API-Sign: " + PrivateSignature(apiKey, path, nonce, opts.postdata.toStringView());
    ret = curlHandle.query(method_url, opts);
    jsonData = json::parse(std::move(ret));
  }
  if (jsonData.contains("error") && !jsonData["error"].empty()) {
    if (method == "CancelOrder" && jsonData["error"].front() == "EOrder:Unknown order") {
      log::warn("Unknown order from Kraken CancelOrder. Assuming closed order");
      jsonData = "{\" error \":[],\" result \":{\" count \":1}}"_json;
    } else {
      throw exception("Kraken private query error: " + std::string(jsonData["error"].front()));
    }
  }
  return jsonData["result"];
}
}  // namespace

KrakenPrivate::KrakenPrivate(CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey)
    : ExchangePrivate(apiKey),
      _curlHandle(config.exchangeInfo("kraken").minPrivateQueryDelay(), config.getRunMode()),
      _config(config),
      _krakenPublic(krakenPublic),
      _balanceCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAccountBalance), _cachedResultVault),
          _curlHandle, config, _apiKey, krakenPublic),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, krakenPublic) {}

CurrencyExchangeFlatSet KrakenPrivate::queryTradableCurrencies() { return _krakenPublic.queryTradableCurrencies(); }

BalancePortfolio KrakenPrivate::AccountBalanceFunc::operator()(CurrencyCode equiCurrency) {
  BalancePortfolio ret;
  json res = PrivateQuery(_curlHandle, _apiKey, "Balance");
  // Kraken returns an empty array in case of account with no balance at all
  for (const auto& [curCode, amountStr] : res.items()) {
    std::string amount = amountStr;
    CurrencyCode currencyCode(_config.standardizeCurrencyCode(curCode));
    MonetaryAmount a(std::move(amount), currencyCode);
    if (!a.isZero()) {
      if (equiCurrency == CurrencyCode::kNeutral) {
        log::info("Kraken Balance {}", a.str());
        ret.add(a, MonetaryAmount("0", equiCurrency));
      } else {
        MonetaryAmount equivalentInMainCurrency = _krakenPublic.computeEquivalentInMainCurrency(a, equiCurrency);
        ret.add(a, equivalentInMainCurrency);
      }
    }
  }
  return ret;
}

Wallet KrakenPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurrencyExchange krakenCurrency = _krakenPublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  json res = PrivateQuery(_curlHandle, _apiKey, "DepositMethods", {{"asset", krakenCurrency.altStr()}});
  // [ { "fee": "0.0000000000", "gen-address": true, "limit": false, "method": "Bitcoin"}]
  if (res.empty()) {
    throw exception("No deposit method found on Kraken for " + std::string(currencyCode.str()));
  }
  const std::string method = res.front()["method"];
  res =
      PrivateQuery(_curlHandle, _apiKey, "DepositAddresses", {{"asset", krakenCurrency.altStr()}, {"method", method}});
  if (res.empty()) {
    // This means user has not created a wallet yet, but it's possible to do it via DepositMethods query above.
    log::warn("No deposit address found on {} for {}, creating a new one...", _krakenPublic.name(), currencyCode.str());
    res = PrivateQuery(_curlHandle, _apiKey, "DepositAddresses",
                       {{"asset", krakenCurrency.altStr()}, {"method", method}, {"new", "true"}});
    if (res.empty()) {
      throw exception("Cannot create a new deposit address on Kraken for " + std::string(currencyCode.str()));
    }
  }
  std::string address, tag;
  for (const auto& [keyStr, valueStr] : res.front().items()) {
    if (keyStr == "address") {
      address = valueStr;
    } else if (keyStr == "expiretm") {
      if (valueStr != "0") {
        log::error("{} wallet has an expire time of {}", _krakenPublic.name(), valueStr);
      }
    } else if (keyStr == "new") {
      // Never used, it's ok, safely pass this
    } else {
      // Heuristic: this last field may change key name and is optional (tag for XRP, memo for EOS for instance)
      if (!tag.empty()) {
        throw exception("Tag already set / unknown key information for " + std::string(currencyCode.str()));
      }
      if (valueStr.is_number_integer()) {
        tag = std::to_string(static_cast<long>(valueStr));
      } else {
        tag = valueStr.get<std::string>();
      }
    }
  }
  Wallet w(PrivateExchangeName(_krakenPublic.name(), _apiKey.name()), currencyCode, address, tag);
  log::info("Retrieved {}", w.str());
  return w;
}

MonetaryAmount KrakenPrivate::trade(MonetaryAmount& from, CurrencyCode toCurrencyCode, const TradeOptions& options) {
  // Documentation: https://www.kraken.com/fr-fr/features/api#add-standard-order
  using Clock = TradeOptions::Clock;
  using TimePoint = TradeOptions::TimePoint;
  TimePoint timerStart = Clock::now();
  const bool isTakerStrategy = options.isTakerStrategy();
  const Market m = _krakenPublic.retrieveMarket(from.currencyCode(), toCurrencyCode);
  const MonetaryAmount orderMin = _krakenPublic.queryVolumeOrderMin(m);
  // Make input market 'Kraken' like.
  CurrencyExchange krakenCurrencyBase = _krakenPublic.convertStdCurrencyToCurrencyExchange(m.base());
  CurrencyExchange krakenCurrencyQuote = _krakenPublic.convertStdCurrencyToCurrencyExchange(m.quote());
  Market krakenMarket(krakenCurrencyBase.altStr(), krakenCurrencyQuote.altStr());
  const std::string_view orderType = from.currencyCode() == m.base() ? "sell" : "buy";
  // Mandatory options
  MonetaryAmount price = _krakenPublic.computeAvgOrderPrice(m, from, isTakerStrategy);

  auto volAndPriNbDecimals = _krakenPublic._marketsCache.get().second.find(m)->second.volAndPriNbDecimals;

  price.truncate(volAndPriNbDecimals.priNbDecimals);

  // volume in quote currency (viqc) is not available (as of March 2021), receiving error 'EAPI:Feature disabled:viqc'
  // We have to compute the amount manually (always in base currency)
  MonetaryAmount volume = from.currencyCode() == m.quote() ? MonetaryAmount(from / price, m.base()) : from;

  volume.truncate(volAndPriNbDecimals.volNbDecimals);

  if (volume < orderMin) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(), toCurrencyCode.str(),
              orderMin.str());
    return MonetaryAmount("0", toCurrencyCode);
  }

  // minimum expire time tested on my side was 5 seconds. I chose 10 seconds just to be sure that we will not have any
  // problem.
  const int maxTradeTimeInSeconds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(options.maxTradeTime()).count());
  const int expireTimeInSeconds = std::max(10, maxTradeTimeInSeconds);

  const auto nbSecondsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();

  // oflags: Ask fee in destination currency.
  // This will not work if user has enough Kraken Fee Credits (in this case, they will be used instead).
  // Warning: this does not change the currency of the returned fee from Kraken in the get Closed / Opened orders,
  // which will be always in quote currency (as per the documentation)

  // Below 32 bits int acts as a hash: it is maybe not unique, so we should filter again based on the txid.
  // we will keep it unique for this whole trade
  const int32_t orderId32Bits = static_cast<int32_t>(nbSecondsSinceEpoch);

  CurlPostData placePostData{{"pair", krakenMarket.assetsPairStr()},
                             {"type", orderType},
                             {"ordertype", isTakerStrategy ? "market" : "limit"},
                             {"price", price.amountStr()},
                             {"volume", volume.amountStr()},
                             {"oflags", from.currencyCode() == m.quote() ? "fcib" : "fciq"},
                             {"expiretm", std::to_string(nbSecondsSinceEpoch + expireTimeInSeconds)},
                             {"userref", std::to_string(orderId32Bits)}};
  if (options.simulation()) {
    placePostData.append("validate", "true");  // validate inputs only. do not submit order (optional)
  }

  json placeOrderRes = PrivateQuery(_placeCancelOrder, _apiKey, "AddOrder", placePostData);
  TimePoint lastPriceUpdateTime = Clock::now();
  // {"error":[],"result":{"descr":{"order":"buy 24.69898116 XRPETH @ limit 0.0003239"},"txid":["OWBA44-TQZQ7-EEYSXA"]}}
  // Kraken will automatically truncate the decimals to the maximum allowed for the trade assets. Get this information
  // and adjust our amount.
  std::string orderDescriptionStr = placeOrderRes["descr"]["order"];
  std::string_view krakenTruncatedAmount(
      orderDescriptionStr.begin() + orderType.size() + 1,
      orderDescriptionStr.begin() + orderDescriptionStr.find_first_of(' ', orderType.size() + 1));
  MonetaryAmount krakenVolume(krakenTruncatedAmount, m.base());
  log::debug("Kraken adjusted volume: {}", krakenVolume.str());

  if (options.simulation()) {
    // In simulation mode, there is no txid returned. If we arrived here (after CollectResults) we assume that the call
    // to api was a success.
    // In simulation mode, just assume all was eaten (for simplicity)
    MonetaryAmount toAmount =
        volume.currencyCode() == m.quote() ? MonetaryAmount(from / price.toNeutral(), m.base()) : from.convertTo(price);
    toAmount = _config.exchangeInfo(_krakenPublic._name)
                   .applyFee(toAmount, isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker);
    from = MonetaryAmount("0", from.currencyCode());
    return toAmount;
  }
  std::string txIdString = placeOrderRes["txid"].front();
  using CreatedOrders = cct::SmallVector<std::string, 8>;
  CreatedOrders createdOrders(1, txIdString);

  // Now, we need to follow our order's life, until either:
  //  - All its amount is eated before the timeout
  //  - Timeout is reached and order will expire naturally.
  //  - If Maker strategy and limit price changes, update order to new limit price (cancel then new)
  MonetaryAmount lastPrice = price;
  MonetaryAmount remFrom = from;
  do {
    json ordersRes =
        queryOrdersData(m, from.currencyCode(), orderId32Bits, createdOrders, QueryOrder::kOpenedThenClosed);

    const json& openedOrdersMap = ordersRes["open"];
    // TODO: Simplify below lambda
    auto updateOrder = [this, m, orderId32Bits, &placePostData, expireTimeInSeconds, volAndPriNbDecimals,
                        isTakerStrategy,
                        orderMin](MonetaryAmount& remFrom, const std::string& txIdString) -> std::string {
      TradedOrdersInfo closedOrderInfo =
          queryOrders(m, remFrom.currencyCode(), orderId32Bits,
                      std::span<const std::string>(std::addressof(txIdString), 1), QueryOrder::kClosedThenOpened);

      remFrom -= closedOrderInfo.tradedFrom;

      auto nbSecondsSinceEpoch =
          std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();
      placePostData.set("expiretm", std::to_string(nbSecondsSinceEpoch + expireTimeInSeconds));
      // Add a new order at market price (to make it matched immediately)
      // We need to recalculate the volume in this case, it's possible that order book has changed.
      MonetaryAmount price = _krakenPublic.computeAvgOrderPrice(m, remFrom, isTakerStrategy);
      MonetaryAmount volume = remFrom.currencyCode() == m.quote() ? MonetaryAmount(remFrom / price, m.base()) : remFrom;
      volume.truncate(volAndPriNbDecimals.volNbDecimals);
      if (volume < orderMin) {
        log::warn("Do not trade remaining {} because min vol order is {} for this market", volume.str(),
                  orderMin.str());
        return "";
      }

      placePostData.set("volume", volume.amountStr());
      price.truncate(volAndPriNbDecimals.priNbDecimals);
      placePostData.set("price", price.amountStr());
      json newPlaceOrderRes = PrivateQuery(_placeCancelOrder, _apiKey, "AddOrder", placePostData);
      return newPlaceOrderRes["txid"].front();
    };

    if (openedOrdersMap.contains(txIdString)) {
      TimePoint t = Clock::now();
      if (timerStart + options.maxTradeTime() < t + options.emergencyBufferTime()) {
        // timeout. Action depends on Strategy
        if (isTakerStrategy) {
          log::error("Kraken taker order was not matched immediately, try again");
        }
        PrivateQuery(_placeCancelOrder, _apiKey, "CancelOrder", {{"txid", txIdString}});
        // {"error":[],"result":{"count":1}}

        if (isTakerStrategy || options.strategy() == TradeOptions::Strategy::kMakerThenTaker) {
          if (timerStart + options.maxTradeTime() < t) {
            break;
          }
          placePostData.set("ordertype", "market");
          txIdString = updateOrder(remFrom, txIdString);
          if (txIdString.empty()) {
            break;
          } else {
            createdOrders.push_back(txIdString);
          }
        }
      } else if (!isTakerStrategy && lastPriceUpdateTime + options.minTimeBetweenPriceUpdates() < Clock::now()) {
        // Let's see if we need to change the price if limit price has changed.
        price = _krakenPublic.computeAvgOrderPrice(m, remFrom, isTakerStrategy);
        if ((from.currencyCode() == m.base() && price < lastPrice) ||
            (from.currencyCode() == m.quote() && price > lastPrice)) {
          log::info("Limit price changed from {} to {}, update order", lastPrice.str(), price.str());
          PrivateQuery(_placeCancelOrder, _apiKey, "CancelOrder", {{"txid", txIdString}});
          txIdString = updateOrder(remFrom, txIdString);
          if (txIdString.empty()) {
            break;
          } else {
            createdOrders.push_back(txIdString);
          }
          lastPrice = price;
          lastPriceUpdateTime = Clock::now();
        }
      }
    } else {
      break;
    }
  } while (true);

  // Final call just to confirm the traded amount. At this point, all orders made by this function should be closed.
  TradedOrdersInfo closedOrdersInfo =
      queryOrders(m, from.currencyCode(), orderId32Bits, createdOrders, QueryOrder::kClosedThenOpened);
  from -= closedOrdersInfo.tradedFrom;
  return closedOrdersInfo.tradedTo;
}

json KrakenPrivate::queryOrdersData(Market m, CurrencyCode fromCurrencyCode, int32_t orderId32Bits,
                                    std::span<const std::string> createdOrdersId, QueryOrder queryOrder) {
  CurrencyCode toCurrencyCode(fromCurrencyCode == m.quote() ? m.base() : m.quote());
  constexpr int kNbMaxRetriesQueryOrders = 10;
  int nbRetries = 0;
  CurlPostData ordersPostData{{"trades", "true"}, {"userref", std::to_string(orderId32Bits)}};
  const bool kOpenedFirst = queryOrder == QueryOrder::kOpenedThenClosed;
  const std::string_view kFirstQueryFullName = kOpenedFirst ? "OpenOrders" : "ClosedOrders";
  const std::string kFirstQueryMapName = kOpenedFirst ? "open" : "closed";
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
    const json& firstOrders = data[kFirstQueryMapName];
    auto notFoundIt =
        std::find_if_not(createdOrdersId.begin(), createdOrdersId.end(),
                         [&firstOrders](const std::string& orderId) { return firstOrders.contains(orderId); });
    if (notFoundIt != createdOrdersId.end()) {
      const std::string_view kSecondQueryFullName = kOpenedFirst ? "ClosedOrders" : "OpenOrders";
      const std::string kSecondQueryMapName = kOpenedFirst ? "closed" : "open";

      data.update(PrivateQuery(_curlHandle, _apiKey, kSecondQueryFullName, ordersPostData));
      const json& secondOrders = data[kSecondQueryMapName];
      notFoundIt = std::find_if_not(createdOrdersId.begin(), createdOrdersId.end(),
                                    [&firstOrders, &secondOrders](const std::string& orderId) {
                                      return firstOrders.contains(orderId) || secondOrders.contains(orderId);
                                    });
    }

    if (notFoundIt != createdOrdersId.end()) {
      if (++nbRetries < kNbMaxRetriesQueryOrders) {
        log::warn("{} is not present in opened nor closed orders, retry {}", *notFoundIt, nbRetries);
        continue;
      }
      throw exception("I lost contact with Kraken order " + *notFoundIt);
    }
    return data;

  } while (true);
}

TradedOrdersInfo KrakenPrivate::queryOrders(Market m, CurrencyCode fromCurrencyCode, int32_t orderId32Bits,
                                            std::span<const std::string> createdOrdersId, QueryOrder queryOrder) {
  CurrencyCode toCurrencyCode(fromCurrencyCode == m.quote() ? m.base() : m.quote());
  TradedOrdersInfo ret(fromCurrencyCode, toCurrencyCode);
  json ordersRes = queryOrdersData(m, fromCurrencyCode, orderId32Bits, createdOrdersId, queryOrder);
  for (const std::string& orderTxIdStr : createdOrdersId) {
    const json& closedOrder = ordersRes.contains("open") && ordersRes["open"].contains(orderTxIdStr)
                                  ? ordersRes["open"][orderTxIdStr]
                                  : ordersRes["closed"][orderTxIdStr];
    MonetaryAmount tradedVol(closedOrder["vol_exec"].get<std::string_view>(), m.base());  // always in base currency
    // Avoid division by 0 as the price is returned as 0.
    if (tradedVol.isZero()) {
      continue;
    }
    MonetaryAmount tradedCost(closedOrder["cost"].get<std::string_view>(), m.quote());  // always in quote currency
    MonetaryAmount fee(closedOrder["fee"].get<std::string_view>(), m.quote());          // always in quote currency

    if (fromCurrencyCode == m.quote()) {
      MonetaryAmount price(closedOrder["price"].get<std::string_view>(), m.base());
      ret.tradedFrom += tradedCost;
      ret.tradedTo += (tradedCost - fee).toNeutral() / price;
    } else {
      ret.tradedFrom += tradedVol;
      ret.tradedTo += tradedCost - fee;
    }
  }
  return ret;
}

WithdrawInfo KrakenPrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) {
  CurrencyCode currencyCode = grossAmount.currencyCode();
  Wallet destinationWallet = targetExchange.queryDepositWallet(currencyCode);
  MonetaryAmount withdrawFee = _krakenPublic.queryWithdrawalFees(currencyCode);
  CurrencyExchange krakenCurrency = _krakenPublic.convertStdCurrencyToCurrencyExchange(currencyCode);

  std::string krakenWalletName(destinationWallet.exchangeName());
  krakenWalletName.push_back('_');
  krakenWalletName.append(currencyCode.str());
  std::transform(std::begin(krakenWalletName), std::end(krakenWalletName), krakenWalletName.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  json withdrawData = PrivateQuery(_curlHandle, _apiKey, "Withdraw",
                                   {{"amount", grossAmount.amountStr()},  // amount to withdraw, including fees
                                    {"asset", krakenCurrency.altStr()},
                                    {"key", krakenWalletName}});
  auto withdrawTime = WithdrawInfo::Clock::now();
  log::info("Withdraw of {} to {} initiated...", (grossAmount - withdrawFee).str(), destinationWallet.str());

  // // {"refid":"BSH3QF5-TDIYVJ-X6U74X"}
  std::string_view txnRefid = withdrawData["refid"].get<std::string_view>();
  CurlPostData checkWithdrawPostData{{"asset", krakenCurrency.altStr()}};
  MonetaryAmount netWithdrawAmount;
  bool withdrawSuccess = false;
  do {
    std::this_thread::sleep_for(kWithdrawInfoRefreshTime);
    json trxList = PrivateQuery(_curlHandle, _apiKey, "WithdrawStatus", checkWithdrawPostData);
    /*
    [
      {
        "aclass": "currency",
        "amount": "99.990000",
        "asset": "TRX",
        "fee": "0.010000",
        "info": "TDunWDzakWwgeopzT1LYYnwPDri37EeGUK",
        "method": "Tron",
        "refid": "BSH3QF5-TDIYVJ-X6U74X",
        "status": "Initial|Settled|Success",
        "time": 1618260686,
        "txid": null
      }
    ]
    */
    bool withdrawRefFound = false;
    for (const json& trx : trxList) {
      std::string_view trxRefId = trx["refid"].get<std::string_view>();
      if (trxRefId == txnRefid) {
        MonetaryAmount realFee(trx["fee"].get<std::string_view>(), currencyCode);
        if (realFee != withdrawFee) {
          log::warn("Kraken withdraw fee is {} instead of parsed {}", realFee.str(), withdrawFee.str());
        }
        std::string_view status = trx["status"].get<std::string_view>();
        if (status == "Success") {
          netWithdrawAmount = MonetaryAmount(trx["amount"].get<std::string_view>(), currencyCode);
          withdrawSuccess = true;
        } else {
          log::info("Still in progress... (Kraken status: {})", status);
        }
        withdrawRefFound = true;
        break;
      }
    }
    if (!withdrawRefFound) {
      throw exception("Kraken: unable to find withdrawal confirmation of " + grossAmount.str());
    }
  } while (!withdrawSuccess);
  log::warn("Confirmed withdrawal of {} to {} {}", netWithdrawAmount.str(), destinationWallet.exchangeName(),
            destinationWallet.address());
  return WithdrawInfo(std::move(destinationWallet), withdrawTime, netWithdrawAmount);
}

}  // namespace api
}  // namespace cct
