#include "bithumbprivateapi.hpp"

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>

#include "apikey.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_nonce.hpp"
#include "coincenterinfo.hpp"
#include "jsonhelpers.hpp"
#include "monetaryamount.hpp"
#include "ssl_sha.hpp"
#include "tradeoptionsapi.hpp"

namespace cct {
namespace api {
namespace {

constexpr char kNbDecimalsUnitsCacheFile[] = ".bithumbdecimalscache";

std::string UrlEncode(const char* str) {
  std::string ret;
  for (int i = 0; str[i]; ++i) {
    unsigned char c = str[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '@' || c == '.' ||
        c == '=' || c == '\\' || c == '-' || c == '_' || c == ':' || c == '&') {
      ret.push_back(c);
    } else {
      char buf[2 + 1];
      sprintf(buf, "%02X", c);
      ret.push_back('%');
      ret.push_back(buf[0]);
      ret.push_back(buf[1]);
    }
  }
  return ret;
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view methodName,
                  BithumbPrivate::MaxNbDecimalsUnitMap& maxNbDecimalsPerCurrencyCodePlace,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  std::string method_url = BithumbPublic::kUrlBase;
  method_url.push_back('/');
  method_url.append(methodName);

  std::string strPost = "endpoint=/";
  strPost.append(methodName);
  strPost.push_back('&');
  strPost.append(curlPostData.toStringView());

  CurlOptions opts(CurlOptions::RequestType::kPost, CurlPostData(UrlEncode(strPost.c_str())));

  std::string strData;
  strData.reserve(100);
  strData.push_back('/');
  strData.append(methodName);

  const char parChar = 1;
  strData.push_back(parChar);
  strData.append(opts.postdata.c_str());
  strData.push_back(parChar);

  Nonce nonce = Nonce_TimeSinceEpoch();
  strData.append(nonce.begin(), nonce.end());

  std::string signature = cct::B64Encode(ssl::ShaHex(ssl::ShaType::kSha512, strData, apiKey.privateKey()));

  opts.userAgent = BithumbPublic::kUserAgent;

  opts.httpHeaders.reserve(4);
  opts.httpHeaders.emplace_back("API-Key: ").append(apiKey.key());
  opts.httpHeaders.emplace_back("API-Sign: " + signature);
  opts.httpHeaders.emplace_back("API-Nonce: " + nonce);
  opts.httpHeaders.emplace_back("api-client-type: 1");

  json dataJson = json::parse(curlHandle.query(method_url, opts));

  // Example of error json: {"status":"5300","message":"Invalid Apikey"}
  const bool isTradeQuery = methodName.starts_with("trade");
  const bool isInfoOpenedOrders = methodName == "info/orders";
  const bool isCancelQuery = methodName == "trade/cancel";
  constexpr int kMaxNbRetries = 3;
  int nbRetries = 0;
  while (dataJson.contains("status") && ++nbRetries < kMaxNbRetries) {
    std::string_view statusCode = dataJson["status"].get<std::string_view>();  // "5300" for instance
    if (statusCode != "0000") {                                                // "0000" stands for: request OK
      std::string_view msg;
      if (dataJson.contains("message")) {
        msg = dataJson["message"].get<std::string_view>();
        if (statusCode == "5600") {
          if (isTradeQuery) {
            // too many decimals, you need to truncate
            constexpr char kMagicKoreanString1[] = "수량은 소수점 ";
            constexpr char kMagicKoreanString2[] = "자";
            std::size_t nbDecimalsMaxPos = msg.find(kMagicKoreanString1);
            if (nbDecimalsMaxPos != std::string_view::npos) {
              std::size_t idxFirst = nbDecimalsMaxPos + strlen(kMagicKoreanString1);
              auto first = msg.begin() + idxFirst;
              std::string_view maxNbDecimalsStr(first, msg.begin() + msg.find(kMagicKoreanString2, idxFirst));
              CurrencyCode currencyCode(std::string_view(msg.begin(), msg.begin() + msg.find_first_of(' ')));
              // I did not find the way via the API to get the maximum precision of Bithumb assets,
              // so I get them this way, by parsing the response Korean error message
              log::warn("Bithumb told us that maximum precision of {} is {} decimals", currencyCode.str(),
                        maxNbDecimalsStr);
              const int8_t maxNbDecimals = std::stoi(std::string(maxNbDecimalsStr));
              using Clock = std::chrono::high_resolution_clock;
              maxNbDecimalsPerCurrencyCodePlace.insert_or_assign(
                  currencyCode, BithumbPrivate::NbDecimalsTimeValue{maxNbDecimals, Clock::now()});

              // Perform a second time the query here with truncated decimals.
              MonetaryAmount volume(opts.postdata.get("units"));
              volume.truncate(maxNbDecimals);
              opts.postdata.set("units", volume.amountStr());

              // Recalculate the nonce and update the signature
              // Nonce is most likely to be the same size, but let's consider that it may not.
              // Signature length however is always the same.
              const int oldNonceSize = nonce.size();
              nonce = Nonce_TimeSinceEpoch();
              strData.replace(strData.end() - oldNonceSize, strData.end(), nonce.begin(), nonce.end());
              signature = cct::B64Encode(ssl::ShaHex(ssl::ShaType::kSha512, strData, apiKey.privateKey()));
              opts.httpHeaders[1].replace(opts.httpHeaders[1].end() - signature.size(), opts.httpHeaders[1].end(),
                                          signature.begin(), signature.end());
              opts.httpHeaders[2].replace(opts.httpHeaders[2].end() - oldNonceSize, opts.httpHeaders[2].end(),
                                          nonce.begin(), nonce.end());

              // Update dataJson and check again the result (should be OK from now)
              dataJson = json::parse(curlHandle.query(method_url, opts));
              continue;
            }
          }
          if ((isInfoOpenedOrders || isCancelQuery) &&
              msg.find("거래 진행중인 내역이 존재하지 않습니다") != std::string::npos) {
            // This is not really an error, it means that order has been eaten or cancelled.
            // Just return empty json in this case
            log::info("Considering Bithumb order as closed as no data received from them");
            dataJson.clear();
            return dataJson;
          }
        }
      }
      throw exception(std::string("Bithumb::query error: ").append(statusCode).append(" \"").append(msg).append("\""));
    }
  }
  return methodName.starts_with("trade") ? dataJson : dataJson["data"];
}

}  // namespace

BithumbPrivate::BithumbPrivate(CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey)
    : ExchangePrivate(apiKey),
      _curlHandle(config.exchangeInfo(bithumbPublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _maxNbDecimalsPerCurrencyCodePlace(),
      _nbDecimalsRefreshTime(config.getAPICallUpdateFrequency(QueryTypeEnum::kNbDecimalsUnitsBithumb)),
      _config(config),
      _bithumbPublic(bithumbPublic),
      _balanceCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAccountBalance), _cachedResultVault),
          _curlHandle, _apiKey, _maxNbDecimalsPerCurrencyCodePlace, bithumbPublic),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, _maxNbDecimalsPerCurrencyCodePlace, bithumbPublic) {
  json data = OpenJsonFile(kNbDecimalsUnitsCacheFile, FileNotFoundMode::kNoThrow);
  _maxNbDecimalsPerCurrencyCodePlace.reserve(data.size());
  for (const auto& [currencyStr, nbDecimalsAndTimeData] : data.items()) {
    CurrencyCode currencyCode(currencyStr);
    int8_t nbDecimals = nbDecimalsAndTimeData["nbdecimals"];
    int64_t timeepoch = nbDecimalsAndTimeData["timeepoch"];
    log::debug("Stored {} decimals for {} from cache file", nbDecimals, currencyStr);
    _maxNbDecimalsPerCurrencyCodePlace.insert_or_assign(
        currencyCode, NbDecimalsTimeValue{nbDecimals, TimePoint(std::chrono::seconds(timeepoch))});
  }
}

CurrencyExchangeFlatSet BithumbPrivate::queryTradableCurrencies() { return _bithumbPublic.queryTradableCurrencies(); }

BalancePortfolio BithumbPrivate::AccountBalanceFunc::operator()(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, "info/balance", _maxNbDecimalsUnitMap, {{"currency", "all"}});
  BalancePortfolio ret;
  for (const auto& [key, value] : result.items()) {
    /*
              "total_btc" : "665.40127447",
              "total_krw" : "305507280",
              "in_use_btc" : "127.43629364",
              "in_use_krw" : "8839047.0000000000",
              "available_btc" : "537.96498083",
              "available_krw" : "294932685.000000000000",
              "xcoin_last_btc": "505000"
    */
    constexpr std::string_view prefixKey = "available_";
    if (key.starts_with(prefixKey)) {
      std::string_view keyCurrencyCode(key);
      keyCurrencyCode.remove_prefix(prefixKey.size());
      CurrencyCode currencyCode(keyCurrencyCode);
      MonetaryAmount available(value.get<std::string_view>(), currencyCode);
      if (!available.isZero()) {
        if (equiCurrency == CurrencyCode::kNeutral) {
          log::info("{} Balance {}", _bithumbPublic.name(), available.str());
          ret.add(available, MonetaryAmount("0", equiCurrency));
        } else {
          MonetaryAmount equivalentInMainCurrency =
              _bithumbPublic.computeEquivalentInMainCurrency(available, equiCurrency);
          ret.add(available, equivalentInMainCurrency);
        }
      }
    }
  }

  return ret;
}

Wallet BithumbPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, "info/wallet_address", _maxNbDecimalsUnitMap,
                             {{"currency", currencyCode.str()}});
  // {"currency": "XRP","wallet_address": "xXXXxXXXXXxxxXXXxxxXXX&dt=123456789"}
  // {"currency": "QTUM","wallet_address": "QMFxxxXXXXxxxxXXXXXxxxx"}
  // {"currency":"EOS","wallet_address":"bithumbrecv1&memo=123456789"}
  std::string_view addressAndTag = result["wallet_address"].get<std::string_view>();
  std::size_t tagPos = addressAndTag.find_first_of('&');
  std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
  std::string_view tag(
      tagPos != std::string_view::npos
          ? (addressAndTag.begin() +
             std::min(addressAndTag.find_first_of('=', std::min(tagPos + 1, addressAndTag.size())) + 1U,
                      addressAndTag.size()))
          : addressAndTag.end(),
      addressAndTag.end());

  Wallet w(PrivateExchangeName(_bithumbPublic.name(), _apiKey.name()), currencyCode, address, tag);
  log::info("Retrieved {}", w.str());
  return w;
}

MonetaryAmount BithumbPrivate::trade(MonetaryAmount& from, CurrencyCode toCurrencyCode, const TradeOptions& options) {
  // Documentation: https://apidocs.bithumb.com/docs/place
  using Clock = TradeOptions::Clock;
  using TimePoint = TradeOptions::TimePoint;
  TimePoint timerStart = Clock::now();
  const bool isTakerStrategy = options.strategy() == TradeOptions::Strategy::kTaker;
  Market m = _bithumbPublic.retrieveMarket(from.currencyCode(), toCurrencyCode);

  // I think Bithumb uses what I call "standard" currency codes, no need to translate them
  CurlPostData placePostData{{"order_currency", m.base().str()}, {"payment_currency", m.quote().str()}};
  const std::string_view orderType = from.currencyCode() == m.base() ? "ask" : "bid";
  // Bithumb fees seem to be in quote currency for sell, base currency for buy (to be confirmed)
  MonetaryAmount volume = from;
  MonetaryAmount price = _bithumbPublic.computeAvgOrderPrice(m, volume, isTakerStrategy);
  if (options.simulation()) {
    // Bithumb does not have a simulation options in their API, we should get out here in simulation mode
    // In simulation mode, just assume all was eaten (for simplicity)
    MonetaryAmount convertedAmount =
        volume.currencyCode() == m.quote() ? MonetaryAmount(from / price.toNeutral(), m.base()) : from.convertTo(price);
    convertedAmount =
        _config.exchangeInfo(_bithumbPublic._name)
            .applyFee(convertedAmount, isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker);
    from = MonetaryAmount("0", from.currencyCode());
    return convertedAmount;
  }
  if (volume.currencyCode() == m.quote()) {
    volume /= price;
  }

  std::string methodName = "trade/";
  if (isTakerStrategy) {
    methodName.append(from.currencyCode() == m.base() ? "market_sell" : "market_buy");
  } else {
    methodName.append("place");
    placePostData.append("type", orderType);
    placePostData.append("price", price.amountStr());
  }

  MaxNbDecimalsUnitMap::const_iterator maxUnitNbDecimalsIt = _maxNbDecimalsPerCurrencyCodePlace.find(m.base());
  int8_t nbMaxDecimalsUnits = std::numeric_limits<MonetaryAmount::AmountType>::digits10;
  if (maxUnitNbDecimalsIt != _maxNbDecimalsPerCurrencyCodePlace.end() &&
      maxUnitNbDecimalsIt->second.lastUpdatedTime + _nbDecimalsRefreshTime > timerStart) {
    nbMaxDecimalsUnits = maxUnitNbDecimalsIt->second.nbDecimals;
    volume.truncate(nbMaxDecimalsUnits);
  }

  placePostData.append("units", volume.amountStr());

  json placeOrderRes =
      PrivateQuery(_curlHandle, _apiKey, methodName, _maxNbDecimalsPerCurrencyCodePlace, placePostData);
  //{"status" : "0000","order_id" : "1428646963419"}
  std::string txIdString = placeOrderRes["order_id"];
  using CreatedOrders = cct::SmallVector<std::string, 8>;
  CreatedOrders createdOrders(1, txIdString);

  TimePoint lastPriceUpdateTime = Clock::now();

  // Now, we need to follow our order's life, until either:
  //  - All its amount is eated before the timeout
  //  - Timeout is reached and order will expire naturally.
  //  - If Maker strategy and limit price changes, update order to new limit price (cancel then new)
  MonetaryAmount lastPrice = price;
  MonetaryAmount remFrom = from;
  CurlPostData orderIdPostData{{"order_currency", m.base().str()},
                               {"payment_currency", m.quote().str()},
                               {"type", orderType},
                               {"order_id", txIdString}};
  do {
    json openOrdersRes =
        PrivateQuery(_curlHandle, _apiKey, "info/orders", _maxNbDecimalsPerCurrencyCodePlace, orderIdPostData);
    /*
    [
      {
        "order_currency": "ETH",
        "order_date": "1617461818551132",
        "order_id": "C0102000000181830176",
        "payment_currency": "KRW",
        "price": "2670000",
        "type": "ask",
        "units": "0.0003",
        "units_remaining": "0.0003",
        "watch_price": "0"
      }
    ]
    */
    if (openOrdersRes.empty() || openOrdersRes.front()["order_id"] != txIdString) {
      break;
    }
    auto updateOrder = [this, m, &placePostData, isTakerStrategy, nbMaxDecimalsUnits, &methodName, &lastPrice](
                           MonetaryAmount& remFrom, std::string& txIdString, CreatedOrders& createdOrders,
                           CurlPostData& orderIdPostData) {
      TradedOrdersInfo closedOrderInfo =
          queryClosedOrders(m, remFrom.currencyCode(), std::span<const std::string>(&txIdString, 1));

      if (closedOrderInfo.isZero()) {
        createdOrders.pop_back();  // No need to keep this order
      } else {
        remFrom -= closedOrderInfo.tradedFrom;
      }

      // Add a new order at market price (to make it matched immediately)
      // We need to recalculate the volume in this case, it's possible that order book has changed.
      MonetaryAmount price = _bithumbPublic.computeAvgOrderPrice(m, remFrom, isTakerStrategy);
      if (!isTakerStrategy && price != lastPrice) {
        log::info("Limit price changed from {} to {}, update order", lastPrice.str(), price.str());
      }
      lastPrice = price;

      MonetaryAmount volume = remFrom;
      if (remFrom.currencyCode() == m.quote()) {
        volume /= price;
      }
      volume.truncate(nbMaxDecimalsUnits);
      placePostData.set("units", volume.amountStr());
      placePostData.set("price", price.amountStr());
      json newPlaceOrderRes =
          PrivateQuery(_curlHandle, _apiKey, methodName, _maxNbDecimalsPerCurrencyCodePlace, placePostData);
      txIdString = newPlaceOrderRes["order_id"];
      orderIdPostData.set("order_id", txIdString);
      createdOrders.push_back(txIdString);
    };

    TimePoint t = Clock::now();
    if (timerStart + options.maxTradeTime() < t + options.emergencyBufferTime()) {
      // timeout. Action depends on Strategy
      if (isTakerStrategy) {
        log::error("Bithumb taker order was not matched immediately, try again");
      }
      PrivateQuery(_curlHandle, _apiKey, "trade/cancel", _maxNbDecimalsPerCurrencyCodePlace, orderIdPostData);

      // { "status" : "0000"}
      if (timerStart + options.maxTradeTime() < t) {
        break;
      }
      if (isTakerStrategy || options.strategy() == TradeOptions::Strategy::kMakerThenTaker) {
        methodName = "trade/";
        methodName.append(from.currencyCode() == m.base() ? "market_sell" : "market_buy");
        updateOrder(remFrom, txIdString, createdOrders, orderIdPostData);
      }
    } else if (!isTakerStrategy && lastPriceUpdateTime + options.minTimeBetweenPriceUpdates() < Clock::now()) {
      // Let's see if we need to change the price if limit price has changed.
      price = _bithumbPublic.computeAvgOrderPrice(m, remFrom, isTakerStrategy);
      if ((from.currencyCode() == m.base() && price < lastPrice) ||
          (from.currencyCode() == m.quote() && price > lastPrice)) {
        PrivateQuery(_curlHandle, _apiKey, "trade/cancel", _maxNbDecimalsPerCurrencyCodePlace, orderIdPostData);
        updateOrder(remFrom, txIdString, createdOrders, orderIdPostData);
        lastPriceUpdateTime = Clock::now();
      }
    }

  } while (true);

  // Final call just to confirm the traded amount. At this point, all orders made by this function should be closed.
  TradedOrdersInfo closedOrdersInfo = queryClosedOrders(m, from.currencyCode(), createdOrders);
  from -= closedOrdersInfo.tradedFrom;
  return closedOrdersInfo.tradedTo;
}

TradedOrdersInfo BithumbPrivate::queryClosedOrders(Market m, CurrencyCode fromCurrencyCode,
                                                   std::span<const std::string> createdOrdersId) {
  // info/order_detail does not accept several orders at the same time.
  // Let's query all of them in parallel
  assert(!createdOrdersId.empty());
  cct::SmallVector<json, 8> closedOrdersRes(createdOrdersId.size());  // Allocate memory for all json objects

  if (createdOrdersId.size() > 1) {
    // Launch requests in parallel to decrease response time here. We need to allocate new CurlHandle, one for each
    // query.
    cct::SmallVector<CurlHandle, 8> curlHandles(createdOrdersId.size());
    std::transform(
        std::execution::par, createdOrdersId.begin(), createdOrdersId.end(), curlHandles.begin(),
        closedOrdersRes.begin(), [this, m](const std::string& orderId, CurlHandle& curlHandle) -> json {
          return PrivateQuery(
              curlHandle, _apiKey, "info/order_detail", _maxNbDecimalsPerCurrencyCodePlace,
              {{"order_currency", m.base().str()}, {"payment_currency", m.quote().str()}, {"order_id", orderId}});
        });
  } else {
    closedOrdersRes.front() =
        PrivateQuery(_curlHandle, _apiKey, "info/order_detail", _maxNbDecimalsPerCurrencyCodePlace,
                     {{"order_currency", m.base().str()},
                      {"payment_currency", m.quote().str()},
                      {"order_id", createdOrdersId.front()}});
  }

  TradedOrdersInfo ret(fromCurrencyCode, fromCurrencyCode == m.base() ? m.quote() : m.base());

  for (const json& closedOrderJsonData : closedOrdersRes) {
    const json& contractDetails = closedOrderJsonData["contract"];
    for (const json& contractDetail : contractDetails) {
      MonetaryAmount tradedVol(contractDetail["units"].get<std::string_view>(), m.base());  // always in base currency
      MonetaryAmount price(contractDetail["price"].get<std::string_view>(), m.quote());     // always in quote currency
      MonetaryAmount tradedCost = tradedVol.toNeutral() * price;
      CurrencyCode feeCurrency(contractDetail["fee_currency"].get<std::string_view>());
      MonetaryAmount fee(contractDetail["fee"].get<std::string_view>(), feeCurrency);

      if (fromCurrencyCode == m.quote()) {
        ret.tradedFrom += tradedCost + fee;
        ret.tradedTo += tradedVol;
      } else {
        ret.tradedFrom += tradedVol;
        ret.tradedTo += tradedCost - fee;
      }
    }
  }
  return ret;
}

WithdrawInfo BithumbPrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) {
  CurrencyCode currencyCode = grossAmount.currencyCode();
  Wallet destinationWallet = targetExchange.queryDepositWallet(currencyCode);
  MonetaryAmount withdrawFee = _bithumbPublic.queryWithdrawalFees(currencyCode);
  MonetaryAmount netWithdrawAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{{"units", netWithdrawAmount.amountStr()},
                                {"currency", currencyCode.str()},
                                {"address", destinationWallet.address()}};
  if (destinationWallet.hasDestinationTag()) {
    withdrawPostData.append("destination", destinationWallet.destinationTag());
  }
  PrivateQuery(_curlHandle, _apiKey, "trade/btc_withdrawal", _maxNbDecimalsPerCurrencyCodePlace,
               std::move(withdrawPostData));
  // empty return means that withdraw request is OK
  auto withdrawTime = WithdrawInfo::Clock::now();
  log::info("Withdraw of {} to {} initiated...", netWithdrawAmount.str(), destinationWallet.str());
  CurlPostData checkWithdrawPostData{
      {"searchGb", "3"}, {"order_currency", currencyCode.str()}, {"payment_currency", "BTC"}};
  bool withdrawInProgress = true;
  bool withdrawSuccess = false;
  do {
    std::this_thread::sleep_for(kWithdrawInfoRefreshTime);
    json trxList = PrivateQuery(_curlHandle, _apiKey, "info/user_transactions", _maxNbDecimalsPerCurrencyCodePlace,
                                checkWithdrawPostData);
    if (withdrawInProgress) {
      if (trxList.empty()) {
        checkWithdrawPostData.set("searchGb", "5");  // confirm in a second transaction
        withdrawInProgress = false;
      } else {
        log::info("Still in progress...");
      }
      continue;
    }
    for (const json& trx : trxList) {
      CurrencyCode cur(trx["order_currency"].get<std::string_view>());
      if (cur == currencyCode) {
        std::string_view unitsStr = trx["units"].get<std::string_view>();  // "- 151.0"
        MonetaryAmount realFee(trx["fee"].get<std::string_view>(), cur);
        if (realFee != withdrawFee) {
          log::warn("Bithumb withdraw fee is {} instead of parsed {}", realFee.str(), withdrawFee.str());
        }
        std::size_t first = unitsStr.find_first_of("0123456789");
        if (first == std::string_view::npos) {
          throw exception("Bithumb: cannot parse amount " + std::string(unitsStr));
        }
        MonetaryAmount consumedAmt(std::string_view(unitsStr.begin() + first, unitsStr.end()), cur);
        if (consumedAmt == grossAmount) {
          withdrawSuccess = true;
          break;
        }
        // TODO: Could we have rounding issues in case Bithumb returns to us a string representation of an amount coming
        // from a double? In this case, we should offer a security interval, for instance, accepting +- 1 % error.
        // Let's not implement this for now unless it becomes an issue
        log::warn("Bithumb: similar withdraw found with different amount {} (expected {})", consumedAmt.str(),
                  grossAmount.str());
      }
    }
    if (withdrawSuccess) {
      break;
    }
    throw exception("Bithumb: unable to find withdrawal confirmation of " + grossAmount.str());
  } while (true);
  log::warn("Confirmed withdrawal of {} to {} {}", netWithdrawAmount.str(),
            destinationWallet.privateExchangeName().str(), destinationWallet.address());
  return WithdrawInfo(std::move(destinationWallet), withdrawTime, netWithdrawAmount);
}

void BithumbPrivate::updateCacheFile() const {
  json data;
  for (const auto& [currency, nbDecimalsTimeValue] : _maxNbDecimalsPerCurrencyCodePlace) {
    std::string currencyStr(currency.str());
    data[currencyStr]["nbdecimals"] = nbDecimalsTimeValue.nbDecimals;
    data[currencyStr]["timeepoch"] =
        std::chrono::duration_cast<std::chrono::seconds>(nbDecimalsTimeValue.lastUpdatedTime.time_since_epoch())
            .count();
  }
  WriteJsonFile(kNbDecimalsUnitsCacheFile, data);
}

}  // namespace api
}  // namespace cct
