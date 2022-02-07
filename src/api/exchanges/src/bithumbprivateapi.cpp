#include "bithumbprivateapi.hpp"

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>

#include "apikey.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "coincenterinfo.hpp"
#include "monetaryamount.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeoptions.hpp"

namespace cct::api {
namespace {

std::pair<string, Nonce> GetStrData(std::string_view endpoint, std::string_view postDataStr) {
  Nonce nonce = Nonce_TimeSinceEpochInMs();
  string strData(endpoint);
  strData.reserve(strData.size() + 2U + postDataStr.size() + nonce.size());

  static constexpr char kParChar = 1;
  strData.push_back(kParChar);
  strData.append(postDataStr);
  strData.push_back(kParChar);

  strData.append(nonce.begin(), nonce.end());
  return std::make_pair(std::move(strData), std::move(nonce));
}

void SetHttpHeaders(CurlOptions& opts, const APIKey& apiKey, std::string_view signature, const Nonce& nonce) {
  opts.clearHttpHeaders();
  opts.appendHttpHeader("API-Key", apiKey.key());
  opts.appendHttpHeader("API-Sign", signature);
  opts.appendHttpHeader("API-Nonce", nonce);
  opts.appendHttpHeader("api-client-type", 1);
}

json PrivateQueryProcess(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint, CurlOptions& opts) {
  auto strDataAndNoncePair = GetStrData(endpoint, opts.getPostData().str());

  string signature = B64Encode(ssl::ShaHex(ssl::ShaType::kSha512, strDataAndNoncePair.first, apiKey.privateKey()));

  SetHttpHeaders(opts, apiKey, signature, strDataAndNoncePair.second);
  return json::parse(curlHandle.query(endpoint, opts));
}

json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint,
                  BithumbPrivate::MaxNbDecimalsUnitMap& maxNbDecimalsPerCurrencyCodePlace,
                  const CurlPostData& curlPostData) {
  CurlPostData postdata(curlPostData);
  postdata.prepend("endpoint", endpoint);
  CurlOptions opts(HttpRequestType::kPost, postdata.urlEncodeExceptDelimiters(), BithumbPublic::kUserAgent);
  json dataJson = PrivateQueryProcess(curlHandle, apiKey, endpoint, opts);

  // Example of error json: {"status":"5300","message":"Invalid Apikey"}
  const bool isTradeQuery = endpoint.starts_with("/trade");
  const bool isInfoOpenedOrders = endpoint == "/info/orders";
  const bool isCancelQuery = endpoint == "/trade/cancel";
  const bool isDepositInfo = endpoint == "/info/wallet_address";
  constexpr int kMaxNbRetries = 5;
  int nbRetries = 0;
  while (dataJson.contains("status") && ++nbRetries < kMaxNbRetries) {
    // "5300" for instance. "0000" stands for: request OK
    std::string_view statusCode = dataJson["status"].get<std::string_view>();
    int64_t errorCode = FromString<int64_t>(statusCode);
    if (errorCode != 0) {
      std::string_view msg;
      auto messageIt = dataJson.find("message");
      if (messageIt != dataJson.end()) {
        msg = messageIt->get<std::string_view>();
        switch (errorCode) {
          case 5100: {
            std::size_t requestTimePos = msg.find("Request Time");
            if (requestTimePos != std::string_view::npos) {
              // Bad Request.(Request Time:reqTime1638699638274/nowTime1638699977771)
              static constexpr std::string_view kReqTime = "reqTime";
              static constexpr std::string_view kNowTime = "nowTime";
              std::size_t reqTimePos = msg.find(kReqTime, requestTimePos);
              if (reqTimePos == std::string_view::npos) {
                log::warn("Unable to parse Bithumb bad request msg {}", msg);
              } else {
                reqTimePos += kReqTime.size();
                std::size_t nowTimePos = msg.find(kNowTime, reqTimePos);
                if (nowTimePos == std::string_view::npos) {
                  log::warn("Unable to parse Bithumb bad request msg {}", msg);
                } else {
                  nowTimePos += kNowTime.size();
                  static constexpr std::string_view kAllDigits = "0123456789";
                  std::size_t reqTimeEndPos = msg.find_first_not_of(kAllDigits, reqTimePos);
                  std::size_t nowTimeEndPos = msg.find_first_not_of(kAllDigits, nowTimePos);
                  if (nowTimeEndPos == std::string_view::npos) {
                    nowTimeEndPos = msg.size();
                  }
                  std::string_view reqTimeStr(msg.begin() + reqTimePos, msg.begin() + reqTimeEndPos);
                  std::string_view nowTimeStr(msg.begin() + nowTimePos, msg.begin() + nowTimeEndPos);
                  int64_t reqTimeInt = FromString<int64_t>(reqTimeStr);
                  int64_t nowTimeInt = FromString<int64_t>(nowTimeStr);
                  log::error("Bithumb time is not synchronized with us (difference of {} s)",
                             (reqTimeInt - nowTimeInt) / 1000);
                  log::error("It can sometimes come from a bug in Bithumb, retry");
                  dataJson = PrivateQueryProcess(curlHandle, apiKey, endpoint, opts);
                  continue;
                }
              }
            }
            break;
          }
          case 5600:
            if (isTradeQuery) {
              // too many decimals, you need to truncate
              static constexpr std::string_view kMagicKoreanString1 = "수량은 소수점 ";
              static constexpr std::string_view kMagicKoreanString2 = "자";
              std::size_t nbDecimalsMaxPos = msg.find(kMagicKoreanString1);
              if (nbDecimalsMaxPos != std::string_view::npos) {
                std::size_t idxFirst = nbDecimalsMaxPos + kMagicKoreanString1.size();
                auto first = msg.begin() + idxFirst;
                std::string_view maxNbDecimalsStr(first, msg.begin() + msg.find(kMagicKoreanString2, idxFirst));
                CurrencyCode currencyCode(std::string_view(msg.begin(), msg.begin() + msg.find(' ')));
                // I did not find the way via the API to get the maximum precision of Bithumb assets,
                // so I get them this way, by parsing the Korean error message of the response
                log::warn("Bithumb told us that maximum precision of {} is {} decimals", currencyCode.str(),
                          maxNbDecimalsStr);
                int8_t maxNbDecimals = FromString<int8_t>(maxNbDecimalsStr);

                maxNbDecimalsPerCurrencyCodePlace.insert_or_assign(
                    currencyCode, BithumbPrivate::NbDecimalsTimeValue{maxNbDecimals, Clock::now()});

                // Perform a second time the query here with truncated decimals.
                CurlPostData updatedPostData(curlPostData);
                MonetaryAmount volume(opts.getPostData().get("units"));
                volume.truncate(maxNbDecimals);
                if (volume.isZero()) {
                  return {};
                }
                updatedPostData.set("units", volume.amountStr());
                return PrivateQuery(curlHandle, apiKey, endpoint, maxNbDecimalsPerCurrencyCodePlace, updatedPostData);
              }
            }
            if ((isInfoOpenedOrders || isCancelQuery) &&
                msg.find("거래 진행중인 내역이 존재하지 않습니다") != string::npos) {
              // This is not really an error, it means that order has been eaten or cancelled.
              // Just return empty json in this case
              log::info("Considering Bithumb order as closed as no data received from them");
              dataJson.clear();
              return dataJson;
            }
            if (isDepositInfo && msg.find("잘못된 접근입니다.") != string::npos) {
              dataJson["wallet_address"] = "";
              return dataJson;
            }
            break;
          default:
            break;
        }
      }
      string ex("Bithumb::query error: ");
      ex.append(statusCode).append(" \"").append(msg).append("\"");
      throw exception(std::move(ex));
    }
  }
  return isTradeQuery ? dataJson : dataJson["data"];
}

File GetBithumbDecimalsCache(std::string_view dataDir) {
  return File(dataDir, File::Type::kCache, "bithumbdecimalscache.json", File::IfNotFound::kNoThrow);
}

}  // namespace

BithumbPrivate::BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey)
    : ExchangePrivate(config, bithumbPublic, apiKey),
      _curlHandle(BithumbPublic::kUrlBase, config.metricGatewayPtr(), exchangeInfo().privateAPIRate(),
                  config.getRunMode()),
      _nbDecimalsRefreshTime(exchangeInfo().getAPICallUpdateFrequency(kNbDecimalsUnitsBithumb)),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, _maxNbDecimalsUnitMap, bithumbPublic) {
  json data = GetBithumbDecimalsCache(_coincenterInfo.dataDir()).readJson();
  _maxNbDecimalsUnitMap.reserve(data.size());
  for (const auto& [currencyStr, nbDecimalsAndTimeData] : data.items()) {
    CurrencyCode currencyCode(currencyStr);
    int8_t nbDecimals = nbDecimalsAndTimeData["nbdecimals"];
    int64_t timeepoch = nbDecimalsAndTimeData["timeepoch"];
    log::debug("Stored {} decimals for {} from cache file", nbDecimals, currencyStr);
    _maxNbDecimalsUnitMap.insert_or_assign(currencyCode,
                                           NbDecimalsTimeValue{nbDecimals, TimePoint(std::chrono::seconds(timeepoch))});
  }
}

BalancePortfolio BithumbPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/balance", _maxNbDecimalsUnitMap, {{"currency", "all"}});
  BalancePortfolio balancePortfolio;
  for (const auto& [key, value] : result.items()) {
    static constexpr std::string_view kPrefixKey = "available_";
    if (key.starts_with(kPrefixKey)) {
      CurrencyCode currencyCode(std::string_view(key.begin() + kPrefixKey.size(), key.end()));
      MonetaryAmount amount(value.get<std::string_view>(), currencyCode);
      this->addBalance(balancePortfolio, amount, equiCurrency);
    }
  }
  return balancePortfolio;
}

Wallet BithumbPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/wallet_address", _maxNbDecimalsUnitMap,
                             {{"currency", currencyCode.str()}});
  std::string_view addressAndTag = result["wallet_address"].get<std::string_view>();
  if (addressAndTag.empty()) {
    string err("Bithumb wallet is not created for ");
    err.append(currencyCode.str());
    err.append(", it should be done with the UI first (no way to do it via API).");
    throw exception(std::move(err));
  }
  std::size_t tagPos = addressAndTag.find('&');
  std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
  std::string_view tag(
      tagPos != std::string_view::npos
          ? (addressAndTag.begin() +
             std::min(addressAndTag.find('=', std::min(tagPos + 1, addressAndTag.size())) + 1U, addressAndTag.size()))
          : addressAndTag.end(),
      addressAndTag.end());
  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  PrivateExchangeName privateExchangeName(_exchangePublic.name(), _apiKey.name());
  bool doCheckWallet = coincenterInfo.exchangeInfo(privateExchangeName.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  Wallet w(std::move(privateExchangeName), currencyCode, address, tag, walletCheck);
  log::info("Retrieved {}", w.str());
  return w;
}

ExchangePrivate::Orders BithumbPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params;

  SmallVector<CurrencyCode, 1> orderCurrencies;

  if (openedOrdersConstraints.isCur1Defined()) {
    ExchangePublic::MarketSet markets;
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (!filterMarket.base().isNeutral()) {
      orderCurrencies.push_back(filterMarket.base());
      if (!filterMarket.quote().isNeutral()) {
        params.append("payment_currency", filterMarket.quoteStr());
      }
    }
  } else {
    // Trick: let's use balance query to guess where we can search for opened orders,
    // by looking at "is_use" amounts. The only drawback is that we need to make one query for each currency,
    // but it's better than nothing.
    json result = PrivateQuery(_curlHandle, _apiKey, "/info/balance", _maxNbDecimalsUnitMap, {{"currency", "all"}});
    for (const auto& [key, value] : result.items()) {
      static constexpr std::string_view kPrefixKey = "in_use_";
      if (key.starts_with(kPrefixKey)) {
        CurrencyCode cur(std::string_view(key.begin() + kPrefixKey.size(), key.end()));
        if (cur != "KRW") {
          MonetaryAmount amount(value.get<std::string_view>(), cur);
          if (!amount.isZero()) {
            orderCurrencies.push_back(cur);
          }
        }
      }
    }
  }

  Orders openedOrders;
  if (openedOrdersConstraints.isPlacedTimeAfterDefined()) {
    params.append("after", std::chrono::duration_cast<std::chrono::milliseconds>(
                               openedOrdersConstraints.placedAfter().time_since_epoch())
                               .count());
  }
  if (orderCurrencies.size() > 1) {
    log::info("Will make {} opened order requests", orderCurrencies.size());
  }
  for (CurrencyCode volumeCur : orderCurrencies) {
    params.set("order_currency", volumeCur.str());
    json result = PrivateQuery(_curlHandle, _apiKey, "/info/orders", _maxNbDecimalsUnitMap, params);

    for (json& orderDetails : result) {
      int64_t microsecondsSinceEpoch = FromString<int64_t>(orderDetails["order_date"].get<std::string_view>());

      TimePoint placedTime{std::chrono::microseconds(microsecondsSinceEpoch)};
      if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      string id = std::move(orderDetails["order_id"].get_ref<string&>());
      if (!openedOrdersConstraints.validateOrderId(id)) {
        continue;
      }

      CurrencyCode priceCur(orderDetails["payment_currency"].get<std::string_view>());
      MonetaryAmount originalVolume(orderDetails["units"].get<std::string_view>(), volumeCur);
      MonetaryAmount remainingVolume(orderDetails["units_remaining"].get<std::string_view>(), volumeCur);
      MonetaryAmount matchedVolume = originalVolume - remainingVolume;
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
      TradeSide side = orderDetails["type"].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

      openedOrders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
    }
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

void BithumbPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  // No faster way to cancel several orders at once with Bithumb, doing a simple for loop
  for (const Order& o : queryOpenedOrders(openedOrdersConstraints)) {
    cancelOrderProcess(OrderRef(o.id(), 0 /*userRef, unused*/, o.market(), o.side()));
  }
}

PlaceOrderInfo BithumbPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  const bool isTakerStrategy =
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeInfo().placeSimulateRealOrder());
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.toCur());
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market m = tradeInfo.m;

  // It seems Bithumb uses "standard" currency codes, no need to translate them
  CurlPostData placePostData{{"order_currency", m.baseStr()}, {"payment_currency", m.quoteStr()}};
  const std::string_view orderType = fromCurrencyCode == m.base() ? "ask" : "bid";

  string endpoint("/trade/");
  if (isTakerStrategy) {
    endpoint.append(fromCurrencyCode == m.base() ? "market_sell" : "market_buy");
  } else {
    endpoint.append("place");
    placePostData.append("type", orderType);
    placePostData.append("price", price.amountStr());
  }

  // Volume is gross amount if from amount is in quote currency, we should remove the fees
  if (fromCurrencyCode == m.quote()) {
    ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
    const ExchangeInfo& exchangeInfo = _coincenterInfo.exchangeInfo(_exchangePublic.name());
    volume = exchangeInfo.applyFee(volume, feeType);
  }

  MaxNbDecimalsUnitMap::const_iterator maxUnitNbDecimalsIt = _maxNbDecimalsUnitMap.find(m.base());
  int8_t nbMaxDecimalsUnits = std::numeric_limits<MonetaryAmount::AmountType>::digits10;
  if (maxUnitNbDecimalsIt != _maxNbDecimalsUnitMap.end() &&
      maxUnitNbDecimalsIt->second.lastUpdatedTime + _nbDecimalsRefreshTime > Clock::now()) {
    nbMaxDecimalsUnits = maxUnitNbDecimalsIt->second.nbDecimals;
    volume.truncate(nbMaxDecimalsUnits);
  }

  if (volume.isZero()) {
    log::warn("No trade of {} into {} because min number of decimals is {} for this market", volume.str(),
              toCurrencyCode.str(), static_cast<int>(nbMaxDecimalsUnits));
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  placePostData.append("units", volume.amountStr());

  json result = PrivateQuery(_curlHandle, _apiKey, endpoint, _maxNbDecimalsUnitMap, placePostData);
  //{"status" : "0000","order_id" : "1428646963419"}
  if (result.contains("order_id")) {
    placeOrderInfo.orderId = result["order_id"];
    placeOrderInfo.orderInfo = queryOrderInfo(tradeInfo.createOrderRef(placeOrderInfo.orderId));
  } else {
    log::warn("No trade of {} into {} because min number of decimals is {} for this market", volume.str(),
              toCurrencyCode.str(), static_cast<int>(nbMaxDecimalsUnits));
    placeOrderInfo.setClosed();
  }

  return placeOrderInfo;
}

OrderInfo BithumbPrivate::cancelOrder(const OrderRef& orderRef) {
  cancelOrderProcess(orderRef);
  return queryOrderInfo(orderRef);
}

namespace {
CurlPostData OrderInfoPostData(Market m, TradeSide side, std::string_view id) {
  CurlPostData ret;
  ret.append("order_currency", m.baseStr());
  ret.append("payment_currency", m.quoteStr());
  ret.append("type", side == TradeSide::kSell ? "ask" : "bid");
  ret.append("order_id", id);
  return ret;
}
}  // namespace

void BithumbPrivate::cancelOrderProcess(const OrderRef& orderRef) {
  PrivateQuery(_curlHandle, _apiKey, "/trade/cancel", _maxNbDecimalsUnitMap,
               OrderInfoPostData(orderRef.m, orderRef.side, orderRef.id));
}

OrderInfo BithumbPrivate::queryOrderInfo(const OrderRef& orderRef) {
  const Market m = orderRef.m;
  const CurrencyCode fromCurrencyCode = orderRef.fromCur();
  const CurrencyCode toCurrencyCode = orderRef.toCur();

  CurlPostData postData = OrderInfoPostData(m, orderRef.side, orderRef.id);
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/orders", _maxNbDecimalsUnitMap, postData);

  const bool isClosed = result.empty() || result.front()["order_id"] != orderRef.id;
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  if (isClosed) {
    postData.erase("type");
    result = PrivateQuery(_curlHandle, _apiKey, "/info/order_detail", _maxNbDecimalsUnitMap, postData);

    for (const json& contractDetail : result["contract"]) {
      MonetaryAmount tradedVol(contractDetail["units"].get<std::string_view>(), m.base());  // always in base currency
      MonetaryAmount price(contractDetail["price"].get<std::string_view>(), m.quote());     // always in quote currency
      MonetaryAmount tradedCost = tradedVol.toNeutral() * price;
      CurrencyCode feeCurrency(contractDetail["fee_currency"].get<std::string_view>());
      MonetaryAmount fee(contractDetail["fee"].get<std::string_view>(), feeCurrency);

      if (fromCurrencyCode == m.quote()) {
        orderInfo.tradedAmounts.tradedFrom += tradedCost + fee;
        orderInfo.tradedAmounts.tradedTo += tradedVol;
      } else {
        orderInfo.tradedAmounts.tradedFrom += tradedVol;
        orderInfo.tradedAmounts.tradedTo += tradedCost - fee;
      }
    }
  }
  return orderInfo;
}

InitiatedWithdrawInfo BithumbPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  MonetaryAmount netWithdrawAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{
      {"units", netWithdrawAmount.amountStr()}, {"currency", currencyCode.str()}, {"address", wallet.address()}};
  if (wallet.hasTag()) {
    withdrawPostData.append("destination", wallet.tag());
  }
  PrivateQuery(_curlHandle, _apiKey, "/trade/btc_withdrawal", _maxNbDecimalsUnitMap, withdrawPostData);
  return InitiatedWithdrawInfo(std::move(wallet), "", grossAmount);
}

SentWithdrawInfo BithumbPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  CurlPostData checkWithdrawPostData{{"order_currency", currencyCode.str()}, {"payment_currency", "BTC"}};
  static constexpr int kSearchGbs[] = {3, 5};
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  for (int searchGb : kSearchGbs) {
    checkWithdrawPostData.set("searchGb", searchGb);
    json trxList =
        PrivateQuery(_curlHandle, _apiKey, "/info/user_transactions", _maxNbDecimalsUnitMap, checkWithdrawPostData);
    for (const json& trx : trxList) {
      assert(trx["order_currency"].get<std::string_view>() == currencyCode);
      std::string_view unitsStr = trx["units"].get<std::string_view>();  // "- 151.0"
      MonetaryAmount realFee(trx["fee"].get<std::string_view>(), currencyCode);
      if (realFee != withdrawFee) {
        log::warn("Bithumb withdraw fee is {} instead of parsed {}", realFee.str(), withdrawFee.str());
      }
      std::size_t first = unitsStr.find_first_of("0123456789");
      if (first == std::string_view::npos) {
        throw exception("Bithumb: cannot parse amount " + string(unitsStr));
      }
      MonetaryAmount consumedAmt(std::string_view(unitsStr.begin() + first, unitsStr.end()), currencyCode);
      if (consumedAmt == initiatedWithdrawInfo.grossEmittedAmount()) {
        bool isWithdrawSuccess = searchGb == 5;
        return SentWithdrawInfo(initiatedWithdrawInfo.grossEmittedAmount() - realFee, isWithdrawSuccess);
      }
      // TODO: Could we have rounding issues in case Bithumb returns to us a string representation of an amount coming
      // from a double? In this case, we should offer a security interval, for instance, accepting +- 1 % error.
      // Let's not implement this for now unless it becomes an issue
      log::debug("Bithumb: similar withdraw found with different amount {} (expected {})", consumedAmt.str(),
                 initiatedWithdrawInfo.grossEmittedAmount().str());
    }
  }
  throw exception("Bithumb: unable to find withdrawal confirmation of " +
                  initiatedWithdrawInfo.grossEmittedAmount().str());
}

bool BithumbPrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                        const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  CurlPostData checkDepositPostData{
      {"order_currency", currencyCode.str()}, {"payment_currency", "BTC"}, {"searchGb", 4}};
  json trxList =
      PrivateQuery(_curlHandle, _apiKey, "/info/user_transactions", _maxNbDecimalsUnitMap, checkDepositPostData);

  RecentDeposit::RecentDepositVector recentDeposits;
  for (const json& trx : trxList) {
    CurrencyCode trxCur(trx["order_currency"].get<std::string_view>());
    if (trxCur != currencyCode) {
      continue;
    }
    MonetaryAmount amountReceived(trx["units"].get<std::string_view>(), currencyCode);
    int64_t microsecondsSinceEpoch = trx["transfer_date"].get<int64_t>();

    TimePoint timestamp{std::chrono::microseconds(microsecondsSinceEpoch)};

    recentDeposits.emplace_back(amountReceived, timestamp);
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  return expectedDeposit.selectClosestRecentDeposit(recentDeposits) != nullptr;
}

void BithumbPrivate::updateCacheFile() const {
  json data;
  for (const auto& [currency, nbDecimalsTimeValue] : _maxNbDecimalsUnitMap) {
    string currencyStr(currency.str());
    data[currencyStr]["nbdecimals"] = nbDecimalsTimeValue.nbDecimals;
    data[currencyStr]["timeepoch"] =
        std::chrono::duration_cast<std::chrono::seconds>(nbDecimalsTimeValue.lastUpdatedTime.time_since_epoch())
            .count();
  }
  GetBithumbDecimalsCache(_coincenterInfo.dataDir()).write(data);
}

}  // namespace cct::api
