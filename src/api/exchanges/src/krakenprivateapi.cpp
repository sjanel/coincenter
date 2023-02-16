#include "krakenprivateapi.hpp"

#include <cassert>
#include <thread>

#include "apikey.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
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

  json ret = json::parse(curlHandle.query(method, opts));
  Duration sleepingTime = curlHandle.minDurationBetweenQueries();

  static constexpr std::string_view kErrorKey = "error";

  auto errorIt = ret.find(kErrorKey);
  while (errorIt != ret.end() && !errorIt->empty() &&
         errorIt->front().get<std::string_view>() == "EAPI:Rate limit exceeded") {
    log::error("Kraken private API rate limit exceeded");
    sleepingTime *= 2;
    log::debug("Wait {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
    std::this_thread::sleep_for(sleepingTime);

    // We need to update the nonce
    nonce = Nonce_TimeSinceEpochInMs();
    opts.getPostData().set("nonce", nonce);
    opts.setHttpHeader("API-Sign", PrivateSignature(apiKey, path, nonce, opts.getPostData().str()));
    ret = json::parse(curlHandle.query(method, opts));
    errorIt = ret.find(kErrorKey);
  }
  if (errorIt != ret.end() && !errorIt->empty()) {
    std::string_view msg = errorIt->front().get<std::string_view>();
    if (method.ends_with("CancelOrder") && msg == "EOrder:Unknown order") {
      log::warn("Unknown order from Kraken CancelOrder. Assuming closed order");
      ret = json::parse(R"({" error ":[]," result ":{" count ":1}})");
    } else {
      log::error("Full Kraken json error: '{}'", ret.dump());
      throw exception("Kraken error: {}", msg);
    }
  }
  return ret["result"];
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

BalancePortfolio KrakenPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  BalancePortfolio balancePortfolio;
  json res = PrivateQuery(_curlHandle, _apiKey, "/private/Balance");
  // Kraken returns an empty array in case of account with no balance at all
  vector<MonetaryAmount> balanceAmounts;
  balanceAmounts.reserve(res.size());
  for (const auto& [curCode, amountStr] : res.items()) {
    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(curCode));

    balanceAmounts.emplace_back(amountStr.get<std::string_view>(), currencyCode);
  }
  const auto compByCurrency = [](MonetaryAmount lhs, MonetaryAmount rhs) {
    return lhs.currencyCode() < rhs.currencyCode();
  };
  std::ranges::sort(balanceAmounts, compByCurrency);

  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();

  // Kraken returns total balance, including the amounts in use
  if (!withBalanceInUse) {
    // We need to query the opened orders to remove the balance in use
    for (const Order& order : queryOpenedOrders()) {
      MonetaryAmount remVolume = order.remainingVolume();
      switch (order.side()) {
        case TradeSide::kBuy: {
          MonetaryAmount price = order.price();
          auto lb = std::ranges::lower_bound(balanceAmounts, price, compByCurrency);
          if (lb != balanceAmounts.end() && lb->currencyCode() == price.currencyCode()) {
            *lb -= remVolume.toNeutral() * price;
          } else {
            log::error("Was expecting at least {} in Kraken balance", remVolume.toNeutral() * price);
          }
          break;
        }
        case TradeSide::kSell: {
          auto lb = std::ranges::lower_bound(balanceAmounts, remVolume, compByCurrency);
          if (lb != balanceAmounts.end() && lb->currencyCode() == remVolume.currencyCode()) {
            *lb -= remVolume;
          } else {
            log::error("Was expecting at least {} in Kraken balance", remVolume);
          }
          break;
        }
        default:
          throw exception("unknown trade side");
      }
    }
  }
  for (MonetaryAmount amount : balanceAmounts) {
    addBalance(balancePortfolio, amount, equiCurrency);
  }
  return balancePortfolio;
}

Wallet KrakenPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  json res = PrivateQuery(_curlHandle, _apiKey, "/private/DepositMethods", {{"asset", krakenCurrency.altStr()}});
  ExchangeName eName(_exchangePublic.name(), _apiKey.name());
  if (res.empty()) {
    throw exception("No deposit method found on {} for {}", eName, currencyCode);
  }
  // Don't keep a view on 'method' value, we will override json data just below. We can just steal the string.
  string method = std::move(res.front()["method"].get_ref<string&>());
  res = PrivateQuery(_curlHandle, _apiKey, "/private/DepositAddresses",
                     {{"asset", krakenCurrency.altStr()}, {"method", method}});
  if (res.empty()) {
    // This means user has not created a wallet yet, but it's possible to do it via DepositMethods query above.
    log::warn("No deposit address found on {} for {}, creating a new one", eName, currencyCode);
    res = PrivateQuery(_curlHandle, _apiKey, "/private/DepositAddresses",
                       {{"asset", krakenCurrency.altStr()}, {"method", method}, {"new", "true"}});
    if (res.empty()) {
      throw exception("Cannot create a new deposit address on {} for {}", eName, currencyCode);
    }
  }

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(_exchangePublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  string address;
  string tag;
  for (const json& depositDetail : res) {
    for (const auto& [keyStr, valueStr] : depositDetail.items()) {
      if (keyStr == "address") {
        address = valueStr;
      } else if (keyStr == "expiretm") {
        if (valueStr.is_number_integer()) {  // WARNING: when new = true, expiretm is not a string, but a number!
          int64_t expireTmValue = valueStr.get<int64_t>();
          if (expireTmValue != 0) {
            log::warn("{} wallet has an expire time of {}", eName, expireTmValue);
          }
        } else if (valueStr.is_string()) {
          std::string_view expireTmValue = valueStr.get<std::string_view>();
          if (expireTmValue != "0") {
            log::warn("{} wallet has an expire time of {}", eName, expireTmValue);
          }
        } else {
          throw exception("Cannot retrieve 'expiretm' field of {} deposit address", eName);
        }

      } else if (keyStr == "new") {
        // Never used, it's ok, safely pass this
      } else {
        // Heuristic: this last field may change key name and is optional (tag for XRP, memo for EOS for instance)
        if (!tag.empty()) {
          throw exception("Tag already set / unknown key information for {}", currencyCode);
        }
        if (valueStr.is_number_integer()) {
          SetString(tag, static_cast<long>(valueStr));
        } else {
          tag = valueStr.get<string>();
        }
      }
    }
    if (Wallet::ValidateWallet(walletCheck, eName, currencyCode, address, tag)) {
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
    address.clear();
    tag.clear();
  }

  Wallet w(std::move(eName), currencyCode, std::move(address), std::move(tag), walletCheck);
  log::info("Retrieved {}", w);
  return w;
}

Orders KrakenPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  json data = PrivateQuery(_curlHandle, _apiKey, "/private/OpenOrders", {{"trades", "true"}});
  auto openedPartIt = data.find("open");
  Orders openedOrders;
  if (openedPartIt != data.end()) {
    MarketSet markets;

    for (const auto& [id, orderDetails] : openedPartIt->items()) {
      const json& descrPart = orderDetails["descr"];
      std::string_view marketStr = descrPart["pair"].get<std::string_view>();

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
      MonetaryAmount price(descrPart["price"].get<std::string_view>(), priceCur);
      TradeSide side = descrPart["type"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

      int64_t secondsSinceEpoch = static_cast<int64_t>(orderDetails["opentm"].get<double>());

      TimePoint placedTime{std::chrono::seconds(secondsSinceEpoch)};
      if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      openedOrders.emplace_back(id, matchedVolume, remainingVolume, price, placedTime, side);
    }
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), exchangeName());
  return openedOrders;
}

int KrakenPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.noConstraints()) {
    json cancelledOrders = PrivateQuery(_curlHandle, _apiKey, "/private/CancelAll");
    return cancelledOrders["count"].get<int>();
  }
  Orders openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const Order& o : openedOrders) {
    cancelOrderProcess(o.id());
  }
  return openedOrders.size();
}

Deposits KrakenPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  SmallVector<CurrencyCode, 1> currencies;
  if (depositsConstraints.isCurDefined()) {
    currencies.push_back(depositsConstraints.currencyCode());
  } else {
    log::warn("Retrieval of recent deposits should be done currency by currency for {:e}", exchangeName());
    log::warn("Heuristic: only query for currencies which are present in the balance");
    log::warn("Doing such, we may miss some recent deposits in other currencies");
    for (const auto& amountWithEquivalent : queryAccountBalance()) {
      currencies.push_back(amountWithEquivalent.amount.currencyCode());
    }
  }
  CurlPostData options;
  for (CurrencyCode currencyCode : currencies) {
    options.set("asset", _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode).exchangeStr());
    for (const json& trx : PrivateQuery(_curlHandle, _apiKey, "/private/DepositStatus", options)) {
      std::string_view status(trx["status"].get<std::string_view>());
      if (status != "Success") {
        log::debug("Deposit {} status {}", trx["refid"].get<std::string_view>(), status);
        continue;
      }
      auto additionalNoteIt = trx.find("status-prop");
      if (additionalNoteIt != trx.end()) {
        std::string_view statusNote(additionalNoteIt->get<std::string_view>());
        if (statusNote == "onhold") {
          log::debug("Additional status is {}", statusNote);
          continue;
        }
      }

      MonetaryAmount amount(trx["amount"].get<std::string_view>(), currencyCode);
      int64_t secondsSinceEpoch = trx["time"].get<int64_t>();
      std::string_view id = trx["txid"].get<std::string_view>();
      TimePoint timestamp{std::chrono::seconds(secondsSinceEpoch)};

      if (!depositsConstraints.validateReceivedTime(timestamp)) {
        continue;
      }
      if (depositsConstraints.isDepositIdDefined() && !depositsConstraints.depositIdSet().contains(id)) {
        continue;
      }

      deposits.emplace_back(id, timestamp, amount);
    }
  }
  log::info("Retrieved {} recent deposits for {}", deposits.size(), exchangeName());
  return deposits;
}

PlaceOrderInfo KrakenPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                         const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  const bool isTakerStrategy =
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeInfo().placeSimulateRealOrder());
  const bool isSimulation = tradeInfo.options.isSimulation();
  const Market m = tradeInfo.tradeContext.m;
  KrakenPublic& krakenPublic = dynamic_cast<KrakenPublic&>(_exchangePublic);
  const MonetaryAmount orderMin = krakenPublic.queryVolumeOrderMin(m);
  CurrencyExchange krakenCurrencyBase = _exchangePublic.convertStdCurrencyToCurrencyExchange(m.base());
  CurrencyExchange krakenCurrencyQuote = _exchangePublic.convertStdCurrencyToCurrencyExchange(m.quote());
  Market krakenMarket(krakenCurrencyBase.altCode(), krakenCurrencyQuote.altCode());
  const std::string_view orderType = fromCurrencyCode == m.base() ? "sell" : "buy";

  auto volAndPriNbDecimals = krakenPublic._marketsCache.get().second.find(m)->second.volAndPriNbDecimals;

  price.truncate(volAndPriNbDecimals.priNbDecimals);

  // volume in quote currency (viqc) is not available (as of March 2021), receiving error 'EAPI:Feature disabled:viqc'
  // We have to compute the amount manually (always in base currency)
  volume.truncate(volAndPriNbDecimals.volNbDecimals);

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));
  if (volume < orderMin) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode, orderMin);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  // minimum expire time tested on my side was 5 seconds. I chose 10 seconds just to be sure that we will not have any
  // problem.
  const int maxTradeTimeInSeconds =
      static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(tradeInfo.options.maxTradeTime()).count());
  const int expireTimeInSeconds = std::max(10, maxTradeTimeInSeconds);

  const auto nbSecondsSinceEpoch = TimestampToS(Clock::now());

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
                             {"userref", tradeInfo.tradeContext.userRef}};
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

  placeOrderInfo.orderId = std::move(placeOrderRes["txid"].front().get_ref<string&>());

  // Kraken will automatically truncate the decimals to the maximum allowed for the trade assets. Get this information
  // and adjust our amount.
  std::string_view orderDescriptionStr = placeOrderRes["descr"]["order"].get<std::string_view>();
  std::string_view krakenTruncatedAmount(
      orderDescriptionStr.begin() + orderType.size() + 1,
      orderDescriptionStr.begin() + orderDescriptionStr.find(' ', orderType.size() + 1));
  MonetaryAmount krakenVolume(krakenTruncatedAmount, m.base());
  log::debug("Kraken adjusted volume: {}", krakenVolume);

  placeOrderInfo.orderInfo =
      queryOrderInfo(placeOrderInfo.orderId, tradeInfo.tradeContext,
                     isTakerStrategy ? QueryOrder::kClosedThenOpened : QueryOrder::kOpenedThenClosed);

  return placeOrderInfo;
}

OrderInfo KrakenPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId);
  return queryOrderInfo(orderId, tradeContext, QueryOrder::kClosedThenOpened);
}

void KrakenPrivate::cancelOrderProcess(OrderIdView id) {
  PrivateQuery(_curlHandle, _apiKey, "/private/CancelOrder", {{"txid", id}});
}

OrderInfo KrakenPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext, QueryOrder queryOrder) {
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();
  const Market m = tradeContext.m;

  json ordersRes = queryOrdersData(tradeContext.userRef, orderId, queryOrder);
  auto openIt = ordersRes.find("open");
  const bool orderInOpenedPart = openIt != ordersRes.end() && openIt->contains(orderId);
  const json& orderJson = orderInOpenedPart ? (*openIt)[orderId] : ordersRes["closed"][orderId];
  MonetaryAmount vol(orderJson["vol"].get<std::string_view>(), m.base());             // always in base currency
  MonetaryAmount tradedVol(orderJson["vol_exec"].get<std::string_view>(), m.base());  // always in base currency
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode), !orderInOpenedPart);
  // Avoid division by 0 as the price is returned as 0.
  if (tradedVol != 0) {
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

json KrakenPrivate::queryOrdersData(int64_t userRef, OrderIdView orderId, QueryOrder queryOrder) {
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
      throw exception("I lost contact with Kraken order {}", orderId);
    }
    return data;

  } while (true);
}

InitiatedWithdrawInfo KrakenPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);

  string krakenWalletName(wallet.exchangeName().str());
  krakenWalletName.push_back('_');
  currencyCode.appendStr(krakenWalletName);
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
  json trxList = PrivateQuery(_curlHandle, _apiKey, "/private/WithdrawStatus", {{"asset", krakenCurrency.altStr()}});
  for (const json& trx : trxList) {
    std::string_view withdrawId = trx["refid"].get<std::string_view>();
    if (withdrawId == initiatedWithdrawInfo.withdrawId()) {
      std::string_view status = trx["status"].get<std::string_view>();
      log::info("{} withdraw status: {}", exchangeName(), status);
      MonetaryAmount netWithdrawAmount(trx["amount"].get<std::string_view>(), currencyCode);
      MonetaryAmount fee(trx["fee"].get<std::string_view>(), currencyCode);
      return SentWithdrawInfo(netWithdrawAmount, fee, status == "Success");
    }
  }
  throw exception("Kraken: unable to find withdrawal confirmation of {}", initiatedWithdrawInfo.grossEmittedAmount());
}

}  // namespace cct::api
