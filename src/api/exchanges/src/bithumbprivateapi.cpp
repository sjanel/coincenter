#include "bithumbprivateapi.hpp"

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>

#include "apikey.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "recentdeposit.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeoptions.hpp"

namespace cct::api {
namespace {

constexpr std::string_view kValueKeyStr = "val";
constexpr std::string_view kTimestampKeyStr = "ts";

constexpr std::string_view kMaxOrderPriceJsonKeyStr = "maxOrderPrice";
constexpr std::string_view kMinOrderPriceJsonKeyStr = "minOrderPrice";
constexpr std::string_view kMinOrderSizeJsonKeyStr = "minOrderSize";
constexpr std::string_view kNbDecimalsStr = "nbDecimals";

// Bithumb API parameter constants
constexpr std::string_view kOrderCurrencyParamStr = "order_currency";
constexpr std::string_view kPaymentCurParamStr = "payment_currency";
constexpr std::string_view kOrderIdParamStr = "order_id";
constexpr std::string_view kTypeParamStr = "type";

constexpr std::string_view kWalletAddressEndpointStr = "/info/wallet_address";

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

template <class ValueType>
bool LoadCurrencyInfoField(const json& currencyOrderInfoJson, std::string_view keyStr, ValueType& val, TimePoint& ts) {
  auto subPartIt = currencyOrderInfoJson.find(keyStr.data());
  if (subPartIt != currencyOrderInfoJson.end()) {
    auto valIt = subPartIt->find(kValueKeyStr);
    auto tsIt = subPartIt->find(kTimestampKeyStr);
    if (valIt == subPartIt->end() || tsIt == subPartIt->end()) {
      log::warn("Unexpected format of Bithumb cache detected - do not use (will be automatically updated)");
      return false;
    }
    if constexpr (std::is_same_v<ValueType, MonetaryAmount>) {
      val = MonetaryAmount(valIt->get<std::string_view>());
      log::debug("Loaded {} for '{}' from cache file", val.str(), keyStr);
    } else {
      val = valIt->get<ValueType>();
      log::debug("Loaded {} for '{}' from cache file", val, keyStr);
    }
    ts = TimePoint(std::chrono::seconds(tsIt->get<int64_t>()));
    return true;
  }
  return false;
}

template <class ValueType>
json CurrencyOrderInfoField2Json(const ValueType& val, TimePoint ts) {
  json data;
  if constexpr (std::is_same_v<ValueType, MonetaryAmount>) {
    data.emplace(kValueKeyStr, val.str());
  } else {
    data.emplace(kValueKeyStr, val);
  }
  data.emplace(kTimestampKeyStr, std::chrono::duration_cast<std::chrono::seconds>(ts.time_since_epoch()).count());
  return data;
}

template <class ValueType>
bool ExtractError(std::string_view findStr1, std::string_view findStr2, std::string_view logStr, std::string_view msg,
                  std::string_view jsonKeyStr, json& jsonData) {
  std::size_t startPos = msg.find(findStr1);
  if (startPos != std::string_view::npos) {
    static_assert(std::is_integral_v<ValueType> || std::is_same_v<ValueType, string>,
                  "ValueType should be a possible json value, not a view");
    std::size_t idxFirst = startPos + findStr1.size();
    std::size_t endPos = msg.find(findStr2, idxFirst);
    if (endPos == std::string_view::npos) {
      return false;
    }
    std::string_view valueStr(msg.begin() + idxFirst, msg.begin() + endPos);
    // I did not find the way via the API to get various important information for some currency bound values,
    // so I get them this way, by parsing the Korean error message of the response
    log::warn("Bithumb told us that {} is {}", logStr, valueStr);
    ValueType val;
    if constexpr (std::is_integral_v<ValueType>) {
      val = FromString<ValueType>(valueStr);
    } else {
      val = ValueType(valueStr);
    }
    jsonData.clear();
    jsonData.emplace(jsonKeyStr, CurrencyOrderInfoField2Json(val, Clock::now()));
    return true;
  }
  return false;
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint,
                  CurlPostDataT&& curlPostData) {
  CurlPostData postdata(std::forward<CurlPostDataT>(curlPostData));
  postdata.prepend("endpoint", endpoint);
  CurlOptions opts(HttpRequestType::kPost, postdata.urlEncodeExceptDelimiters(), BithumbPublic::kUserAgent);
  json ret = PrivateQueryProcess(curlHandle, apiKey, endpoint, opts);

  // Example of error json: {"status":"5300","message":"Invalid Apikey"}
  const bool isInfoOpenedOrders = endpoint == "/info/orders";
  const bool isCancelQuery = endpoint == "/trade/cancel";
  const bool isDepositInfo = endpoint == kWalletAddressEndpointStr;
  const bool isPlaceOrderQuery = !isCancelQuery && endpoint.starts_with("/trade");
  constexpr int kMaxNbRetries = 5;
  int nbRetries = 0;
  while (ret.contains("status") && ++nbRetries < kMaxNbRetries) {
    // "5300" for instance. "0000" stands for: request OK
    std::string_view statusCode = ret["status"].get<std::string_view>();
    int64_t errorCode = FromString<int64_t>(statusCode);
    if (errorCode != 0) {
      std::string_view msg;
      auto messageIt = ret.find("message");
      if (messageIt != ret.end()) {
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
                  ret = PrivateQueryProcess(curlHandle, apiKey, endpoint, opts);
                  continue;
                }
              }
            }
            break;
          }
          case 5600:
            if (isPlaceOrderQuery) {
              if (ExtractError<int8_t>("????????? ????????? ", "???", "number of decimals", msg, kNbDecimalsStr, ret) ||
                  ExtractError<string>("??????????????? ", " ?????????", "min order size", msg, kMinOrderSizeJsonKeyStr, ret) ||
                  ExtractError<string>("?????? ????????? ", " ???????????? ?????? ???????????????", "min order price", msg,
                                       kMinOrderPriceJsonKeyStr, ret) ||
                  ExtractError<string>("?????? ????????? ", " ????????? ?????? ???????????????", "max order price", msg,
                                       kMaxOrderPriceJsonKeyStr, ret)) {
                return ret;
              }
            }
            if ((isInfoOpenedOrders || isCancelQuery) &&
                msg.find("?????? ???????????? ????????? ???????????? ????????????") != std::string_view::npos) {
              // This is not really an error, it means that order has been eaten or cancelled.
              // Just return empty json in this case
              log::info("Considering Bithumb order as closed as no data received from them");
              ret.clear();
              return ret;
            }
            if (isDepositInfo && msg.find("????????? ???????????????.") != std::string_view::npos) {
              ret.clear();
              return ret;
            }
            break;
          default:
            break;
        }
      }
      log::error("Full Bithumb json error: '{}'", ret.dump());
      string ex("Bithumb error: ");
      ex.append(statusCode).append(" \"").append(msg).append("\"");
      throw exception(std::move(ex));
    }
  }
  return ret;
}

File GetBithumbCurrencyInfoMapCache(std::string_view dataDir) {
  return File(dataDir, File::Type::kCache, "bithumbcurrencyinfocache.json", File::IfError::kNoThrow);
}

}  // namespace

BithumbPrivate::BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey)
    : ExchangePrivate(config, bithumbPublic, apiKey),
      _curlHandle(BithumbPublic::kUrlBase, config.metricGatewayPtr(), exchangeInfo().privateAPIRate(),
                  config.getRunMode()),
      _currencyOrderInfoRefreshTime(exchangeInfo().getAPICallUpdateFrequency(kCurrencyInfoBithumb)),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, bithumbPublic) {
  json data = GetBithumbCurrencyInfoMapCache(_coincenterInfo.dataDir()).readJson();
  for (const auto& [currencyCodeStr, currencyOrderInfoJson] : data.items()) {
    CurrencyOrderInfo currencyOrderInfo;

    LoadCurrencyInfoField(currencyOrderInfoJson, kNbDecimalsStr, currencyOrderInfo.nbDecimals,
                          currencyOrderInfo.lastNbDecimalsUpdatedTime);
    LoadCurrencyInfoField(currencyOrderInfoJson, kMinOrderSizeJsonKeyStr, currencyOrderInfo.minOrderSize,
                          currencyOrderInfo.lastMinOrderSizeUpdatedTime);
    LoadCurrencyInfoField(currencyOrderInfoJson, kMinOrderPriceJsonKeyStr, currencyOrderInfo.minOrderPrice,
                          currencyOrderInfo.lastMinOrderPriceUpdatedTime);
    LoadCurrencyInfoField(currencyOrderInfoJson, kMaxOrderPriceJsonKeyStr, currencyOrderInfo.maxOrderPrice,
                          currencyOrderInfo.lastMaxOrderPriceUpdatedTime);
    _currencyOrderInfoMap.insert_or_assign(CurrencyCode(currencyCodeStr), std::move(currencyOrderInfo));
  }
}

BalancePortfolio BithumbPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/balance", {{"currency", "all"}})["data"];
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
  json ret = PrivateQuery(_curlHandle, _apiKey, kWalletAddressEndpointStr, {{"currency", currencyCode.str()}});
  if (ret.empty()) {
    string err("Bithumb wallet is not created for ");
    err.append(currencyCode.str());
    err.append(", it should be done with the UI first (no way to do it via API).");
    throw exception(std::move(err));
  }
  std::string_view addressAndTag = ret["data"]["wallet_address"].get<std::string_view>();
  std::size_t tagPos = addressAndTag.find('&');
  std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
  std::string_view tag(
      tagPos != std::string_view::npos
          ? (addressAndTag.begin() +
             std::min(addressAndTag.find('=', std::min(tagPos + 1, addressAndTag.size())) + 1U, addressAndTag.size()))
          : addressAndTag.end(),
      addressAndTag.end());
  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeInfo(_exchangePublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet w(ExchangeName(_exchangePublic.name(), _apiKey.name()), currencyCode, address, tag, walletCheck);
  log::info("Retrieved {}", w.str());
  return w;
}

Orders BithumbPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  CurlPostData params;

  SmallVector<CurrencyCode, 1> orderCurrencies;

  if (openedOrdersConstraints.isCur1Defined()) {
    MarketSet markets;
    Market filterMarket = _exchangePublic.determineMarketFromFilterCurrencies(markets, openedOrdersConstraints.cur1(),
                                                                              openedOrdersConstraints.cur2());

    if (!filterMarket.base().isNeutral()) {
      orderCurrencies.push_back(filterMarket.base());
      if (!filterMarket.quote().isNeutral()) {
        params.append(kPaymentCurParamStr, filterMarket.quoteStr());
      }
    }
  } else {
    // Trick: let's use balance query to guess where we can search for opened orders,
    // by looking at "is_use" amounts. The only drawback is that we need to make one query for each currency,
    // but it's better than nothing.
    json result = PrivateQuery(_curlHandle, _apiKey, "/info/balance", {{"currency", "all"}})["data"];
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
    params.set(kOrderCurrencyParamStr, volumeCur.str());
    json result = PrivateQuery(_curlHandle, _apiKey, "/info/orders", params)["data"];

    for (json& orderDetails : result) {
      int64_t microsecondsSinceEpoch = FromString<int64_t>(orderDetails["order_date"].get<std::string_view>());

      TimePoint placedTime{std::chrono::microseconds(microsecondsSinceEpoch)};
      if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      string id = std::move(orderDetails[kOrderIdParamStr.data()].get_ref<string&>());
      if (!openedOrdersConstraints.validateOrderId(id)) {
        continue;
      }

      CurrencyCode priceCur(orderDetails[kPaymentCurParamStr.data()].get<std::string_view>());
      MonetaryAmount originalVolume(orderDetails["units"].get<std::string_view>(), volumeCur);
      MonetaryAmount remainingVolume(orderDetails["units_remaining"].get<std::string_view>(), volumeCur);
      MonetaryAmount matchedVolume = originalVolume - remainingVolume;
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
      TradeSide side =
          orderDetails[kTypeParamStr.data()].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

      openedOrders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
    }
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

int BithumbPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  // No faster way to cancel several orders at once with Bithumb, doing a simple for loop
  Orders orders = queryOpenedOrders(openedOrdersConstraints);
  for (const Order& o : orders) {
    cancelOrderProcess(OrderRef(o.id(), 0 /*userRef, unused*/, o.market(), o.side()));
  }
  return orders.size();
}

PlaceOrderInfo BithumbPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  const bool placeSimulatedRealOrder = _exchangePublic.exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.toCur());
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market m = tradeInfo.m;

  // It seems Bithumb uses "standard" currency codes, no need to translate them
  CurlPostData placePostData{{kOrderCurrencyParamStr, m.baseStr()}, {kPaymentCurParamStr, m.quoteStr()}};
  const std::string_view orderType = fromCurrencyCode == m.base() ? "ask" : "bid";

  string endpoint("/trade/");
  if (isTakerStrategy) {
    endpoint.append(fromCurrencyCode == m.base() ? "market_sell" : "market_buy");
  } else {
    endpoint.append("place");
    placePostData.append(kTypeParamStr, orderType);
    placePostData.append("price", price.amountStr());
  }

  // Volume is gross amount if from amount is in quote currency, we should remove the fees
  if (fromCurrencyCode == m.quote()) {
    ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
    const ExchangeInfo& exchangeInfo = _coincenterInfo.exchangeInfo(_exchangePublic.name());
    volume = exchangeInfo.applyFee(volume, feeType);
  }

  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;

  auto currencyOrderInfoIt = _currencyOrderInfoMap.find(m.base());
  auto nowTime = Clock::now();
  CurrencyOrderInfo currencyOrderInfo;
  if (currencyOrderInfoIt != _currencyOrderInfoMap.end()) {
    currencyOrderInfo = currencyOrderInfoIt->second;
    if (currencyOrderInfo.lastNbDecimalsUpdatedTime + _currencyOrderInfoRefreshTime > nowTime) {
      int8_t nbMaxDecimalsUnits = currencyOrderInfo.nbDecimals;
      volume.truncate(nbMaxDecimalsUnits);
      if (volume.isZero()) {
        log::warn("No trade of {} into {} because min number of decimals is {} for this market", volume.str(),
                  toCurrencyCode.str(), static_cast<int>(nbMaxDecimalsUnits));
        placeOrderInfo.setClosed();
        return placeOrderInfo;
      }
    }
    if (!isTakerStrategy) {
      MonetaryAmount minOrderPrice = price;
      MonetaryAmount maxOrderPrice = price;
      if (currencyOrderInfo.lastMinOrderPriceUpdatedTime + _currencyOrderInfoRefreshTime > nowTime &&
          currencyOrderInfo.minOrderPrice.currencyCode() == price.currencyCode()) {
        minOrderPrice = currencyOrderInfo.minOrderPrice;
      }
      if (currencyOrderInfo.lastMaxOrderPriceUpdatedTime + _currencyOrderInfoRefreshTime > nowTime &&
          currencyOrderInfo.maxOrderPrice.currencyCode() == price.currencyCode()) {
        maxOrderPrice = currencyOrderInfo.maxOrderPrice;
      }
      if (price < minOrderPrice || price > maxOrderPrice) {
        if (isSimulationWithRealOrder) {
          if (price < minOrderPrice) {
            price = minOrderPrice;
          } else {
            price = maxOrderPrice;
          }
          placePostData.set("price", price.amountStr());
        } else {
          log::warn("No trade of {} into {} because {} is outside price bounds [{}, {}]", volume.str(),
                    toCurrencyCode.str(), price.str(), minOrderPrice.str(), maxOrderPrice.str());
          placeOrderInfo.setClosed();
          return placeOrderInfo;
        }
      }
    }

    if (currencyOrderInfo.lastMinOrderSizeUpdatedTime + _currencyOrderInfoRefreshTime > nowTime) {
      MonetaryAmount size = currencyOrderInfo.minOrderSize;
      CurrencyCode minOrderSizeCur = size.currencyCode();
      if (volume.currencyCode() == minOrderSizeCur) {
        size = volume;
      } else if (price.currencyCode() == minOrderSizeCur) {
        size = volume.toNeutral() * price;
      } else {
        log::error("Unexpected currency for min order size {}", size.str());
      }
      if (size < currencyOrderInfo.minOrderSize && !isSimulationWithRealOrder) {
        log::warn("No trade of {} into {} because {} is lower than min order {}", volume.str(), toCurrencyCode.str(),
                  size.str(), currencyOrderInfo.minOrderSize.str());
        placeOrderInfo.setClosed();
        return placeOrderInfo;
      }
    }
  }

  placePostData.append("units", volume.amountStr());

  placeOrderInfo.setClosed();

  static constexpr int kNbMaxRetries = 3;
  bool currencyInfoUpdated = false;
  for (int nbRetries = 0; nbRetries < kNbMaxRetries; ++nbRetries) {
    json result = PrivateQuery(_curlHandle, _apiKey, endpoint, placePostData);
    auto orderIdIt = result.find(kOrderIdParamStr.data());
    if (orderIdIt == result.end()) {
      currencyInfoUpdated = true;
      if (LoadCurrencyInfoField(result, kNbDecimalsStr, currencyOrderInfo.nbDecimals,
                                currencyOrderInfo.lastNbDecimalsUpdatedTime)) {
        volume.truncate(currencyOrderInfo.nbDecimals);
        placePostData.set("units", volume.amountStr());
      } else if (LoadCurrencyInfoField(result, kMinOrderSizeJsonKeyStr, currencyOrderInfo.minOrderSize,
                                       currencyOrderInfo.lastMinOrderSizeUpdatedTime)) {
        if (isSimulationWithRealOrder && currencyOrderInfo.minOrderSize.currencyCode() == price.currencyCode()) {
          volume = MonetaryAmount(currencyOrderInfo.minOrderSize / price, volume.currencyCode());
          placePostData.set("units", volume.amountStr());
        } else {
          log::warn("No trade of {} into {} because min order size is {} for this market", volume.str(),
                    toCurrencyCode.str(), currencyOrderInfo.minOrderSize.str());
          break;
        }
      } else if (LoadCurrencyInfoField(result, kMinOrderPriceJsonKeyStr, currencyOrderInfo.minOrderPrice,
                                       currencyOrderInfo.lastMinOrderPriceUpdatedTime)) {
        if (isSimulationWithRealOrder) {
          if (!isTakerStrategy) {
            price = currencyOrderInfo.minOrderPrice;
            placePostData.set("price", price.amountStr());
          }
        } else {
          log::warn("No trade of {} into {} because min order price is {} for this market", volume.str(),
                    toCurrencyCode.str(), currencyOrderInfo.minOrderPrice.str());
          break;
        }
      } else if (LoadCurrencyInfoField(result, kMaxOrderPriceJsonKeyStr, currencyOrderInfo.maxOrderPrice,
                                       currencyOrderInfo.lastMaxOrderPriceUpdatedTime)) {
        if (isSimulationWithRealOrder) {
          if (!isTakerStrategy) {
            price = currencyOrderInfo.maxOrderPrice;
            placePostData.set("price", price.amountStr());
          }
        } else {
          log::warn("No trade of {} into {} because max order price is {} for this market", volume.str(),
                    toCurrencyCode.str(), currencyOrderInfo.maxOrderPrice.str());
          break;
        }
      } else {
        log::error("Unexpected answer from {} place order, no data", _exchangePublic.name());
        break;
      }
    } else {
      placeOrderInfo.orderId = std::move(orderIdIt->get_ref<string&>());
      placeOrderInfo.orderInfo = queryOrderInfo(tradeInfo.createOrderRef(placeOrderInfo.orderId));
      break;
    }
  }

  if (currencyInfoUpdated) {
    _currencyOrderInfoMap.insert_or_assign(m.base(), std::move(currencyOrderInfo));
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
  std::string_view baseStr = m.baseStr();
  std::string_view quoteStr = m.quoteStr();
  ret.reserve(kOrderCurrencyParamStr.size() + kPaymentCurParamStr.size() + kTypeParamStr.size() +
              kOrderIdParamStr.size() + baseStr.size() + quoteStr.size() + id.size() + 10U);
  ret.append(kOrderCurrencyParamStr, baseStr);
  ret.append(kPaymentCurParamStr, quoteStr);
  ret.append(kTypeParamStr, side == TradeSide::kSell ? "ask" : "bid");
  ret.append(kOrderIdParamStr, id);
  return ret;
}
}  // namespace

void BithumbPrivate::cancelOrderProcess(const OrderRef& orderRef) {
  PrivateQuery(_curlHandle, _apiKey, "/trade/cancel", OrderInfoPostData(orderRef.m, orderRef.side, orderRef.id));
}

OrderInfo BithumbPrivate::queryOrderInfo(const OrderRef& orderRef) {
  const Market m = orderRef.m;
  const CurrencyCode fromCurrencyCode = orderRef.fromCur();
  const CurrencyCode toCurrencyCode = orderRef.toCur();

  CurlPostData postData = OrderInfoPostData(m, orderRef.side, orderRef.id);
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/orders", postData)["data"];

  const bool isClosed = result.empty() || result.front()[kOrderIdParamStr.data()] != orderRef.id;
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  if (isClosed) {
    postData.erase(kTypeParamStr);
    result = PrivateQuery(_curlHandle, _apiKey, "/info/order_detail", std::move(postData))["data"];

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
  PrivateQuery(_curlHandle, _apiKey, "/trade/btc_withdrawal", std::move(withdrawPostData));
  return InitiatedWithdrawInfo(std::move(wallet), "", grossAmount);
}

SentWithdrawInfo BithumbPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  CurlPostData checkWithdrawPostData{{kOrderCurrencyParamStr, currencyCode.str()}, {kPaymentCurParamStr, "BTC"}};
  static constexpr int kSearchGbs[] = {3, 5};
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFee(currencyCode);
  for (int searchGb : kSearchGbs) {
    checkWithdrawPostData.set("searchGb", searchGb);
    json trxList = PrivateQuery(_curlHandle, _apiKey, "/info/user_transactions", checkWithdrawPostData)["data"];
    for (const json& trx : trxList) {
      assert(trx[kOrderCurrencyParamStr.data()].get<std::string_view>() == currencyCode);
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
      {kOrderCurrencyParamStr, currencyCode.str()}, {kPaymentCurParamStr, "BTC"}, {"searchGb", 4}};
  json trxList = PrivateQuery(_curlHandle, _apiKey, "/info/user_transactions", std::move(checkDepositPostData))["data"];

  RecentDeposit::RecentDepositVector recentDeposits;
  for (const json& trx : trxList) {
    CurrencyCode trxCur(trx[kOrderCurrencyParamStr.data()].get<std::string_view>());
    if (trxCur != currencyCode) {
      continue;
    }
    MonetaryAmount amountReceived(trx["units"].get<std::string_view>(), currencyCode);
    // In the official documentation, transfer_date field is an integer.
    // But in fact (as of 2022) it's a string. Let's support both types to be safe.
    auto transferDateIt = trx.find("transfer_date");
    if (transferDateIt == trx.end()) {
      throw exception("Was expecting 'transfer_date' parameter");
    }
    int64_t microsecondsSinceEpoch;
    if (transferDateIt->is_string()) {
      microsecondsSinceEpoch = FromString<int64_t>(transferDateIt->get<std::string_view>());
    } else if (transferDateIt->is_number_integer()) {
      microsecondsSinceEpoch = transferDateIt->get<int64_t>();
    } else {
      throw exception("Cannot understand 'transfer_date' parameter type");
    }

    TimePoint timestamp{std::chrono::microseconds(microsecondsSinceEpoch)};

    recentDeposits.emplace_back(amountReceived, timestamp);
  }
  RecentDeposit expectedDeposit(sentWithdrawInfo.netEmittedAmount(), Clock::now());
  return expectedDeposit.selectClosestRecentDeposit(recentDeposits) != nullptr;
}

void BithumbPrivate::updateCacheFile() const {
  json data;
  for (const auto& [currencyCode, currencyOrderInfo] : _currencyOrderInfoMap) {
    json curData;

    curData.emplace(kNbDecimalsStr, CurrencyOrderInfoField2Json(currencyOrderInfo.nbDecimals,
                                                                currencyOrderInfo.lastNbDecimalsUpdatedTime));
    curData.emplace(
        kMinOrderSizeJsonKeyStr,
        CurrencyOrderInfoField2Json(currencyOrderInfo.minOrderSize, currencyOrderInfo.lastMinOrderSizeUpdatedTime));
    curData.emplace(
        kMinOrderPriceJsonKeyStr,
        CurrencyOrderInfoField2Json(currencyOrderInfo.minOrderPrice, currencyOrderInfo.lastMinOrderPriceUpdatedTime));
    curData.emplace(
        kMaxOrderPriceJsonKeyStr,
        CurrencyOrderInfoField2Json(currencyOrderInfo.maxOrderPrice, currencyOrderInfo.lastMaxOrderPriceUpdatedTime));

    data.emplace(currencyCode.str(), std::move(curData));
  }
  GetBithumbCurrencyInfoMapCache(_coincenterInfo.dataDir()).write(data);
}

}  // namespace cct::api
