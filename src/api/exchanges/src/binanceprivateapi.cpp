#include "binanceprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "binancepublicapi.hpp"
#include "cct_nonce.hpp"
#include "ssl_sha.hpp"
#include "tradeoptionsapi.hpp"

namespace cct {
namespace api {

namespace {

/// Binance is often slow to update its databases of open / closed orders once it gives us a new order.
/// The number of retries should be sufficiently high to avoid program to crash because of this.
/// It can happen to retry 10 times
constexpr int kNbOrderRequestsRetries = 15;

void SetNonceAndSignature(const APIKey& apiKey, CurlPostData& postData) {
  Nonce nonce = Nonce_TimeSinceEpoch();
  postData.set("timestamp", std::string_view(nonce.begin(), nonce.end()));
  postData.erase("signature");
  postData.append("signature", ssl::ShaHex(ssl::ShaType::kSha256, postData.toString(), apiKey.privateKey()));
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, CurlOptions::RequestType requestType,
                  std::string_view method, CurlPostDataT&& curlPostData = CurlPostData()) {
  std::string url = BinancePublic::kUrlBase;
  url.push_back('/');
  url.append(method);

  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData), BinancePublic::kUserAgent);
  SetNonceAndSignature(apiKey, opts.postdata);
  opts.httpHeaders.emplace_back("X-MBX-APIKEY: ").append(apiKey.key());

  json dataJson = json::parse(curlHandle.query(url, opts));
  auto binanceError = [](const json& j) { return j.contains("code") && j.contains("msg"); };
  if (binanceError(dataJson)) {
    int statusCode = dataJson["code"];  // "1100" for instance
    int nbRetries = 0;
    CurlHandle::Clock::duration sleepingTime = curlHandle.minDurationBetweenQueries();
    while (++nbRetries < kNbOrderRequestsRetries && (statusCode == -2013 || statusCode == -2011)) {
      // Order does not exist : this may be possible when we query an order info too fast
      log::warn("Binance cannot find order yet");
      sleepingTime = (3 * sleepingTime) / 2;
      log::trace("Wait {} ms...", std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
      SetNonceAndSignature(apiKey, opts.postdata);
      dataJson = json::parse(curlHandle.query(url, opts));
      if (!binanceError(dataJson)) {
        return dataJson;
      }
      statusCode = dataJson["code"];
    }
    const std::string_view errorMessage = dataJson["msg"].get<std::string_view>();
    throw exception("error " + std::to_string(statusCode) + ", msg: " + std::string(errorMessage));
  }
  return dataJson;
}

}  // namespace

BinancePrivate::BinancePrivate(CoincenterInfo& config, BinancePublic& binancePublic, const APIKey& apiKey)
    : ExchangePrivate(apiKey),
      _curlHandle(config.exchangeInfo(binancePublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _config(config),
      _public(binancePublic),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic) {}

BalancePortfolio BinancePrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "api/v3/account");
  BalancePortfolio balancePortfolio;
  for (const json& balance : result["balances"]) {
    /*
    {
      "asset": "BTC",
      "free": "0.01136184",
      "locked": "0.00000000"
    },
    */
    CurrencyCode currencyCode(balance["asset"].get<std::string_view>());
    MonetaryAmount available(balance["free"].get<std::string_view>(), currencyCode);

    if (!available.isZero()) {
      if (equiCurrency == CurrencyCode::kNeutral) {
        log::info("{} Balance {}", _public.name(), available.str());
        balancePortfolio.add(available, MonetaryAmount("0", equiCurrency));
      } else {
        MonetaryAmount equivalentInMainCurrency = _public.computeEquivalentInMainCurrency(available, equiCurrency);
        balancePortfolio.add(available, equivalentInMainCurrency);
      }
    }
  }

  return balancePortfolio;
}

Wallet BinancePrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "wapi/v3/depositAddress.html",
                             {{"asset", currencyCode.str()}});
  bool isSuccess = result["success"].get<bool>();
  if (!isSuccess) {
    throw exception("Unsuccessful deposit wallet for currency " + std::string(currencyCode.str()));
  }
  std::string_view address(result["address"].get<std::string_view>());
  std::string_view tag(result["addressTag"].get<std::string_view>());
  Wallet w(PrivateExchangeName(_public.name(), _apiKey.name()), currencyCode, address, tag);
  log::info("Retrieved {}", w.str());
  return w;
}

MonetaryAmount BinancePrivate::trade(MonetaryAmount& from, CurrencyCode toCurrencyCode, const TradeOptions& options) {
  using Clock = TradeOptions::Clock;
  using TimePoint = TradeOptions::TimePoint;
  TimePoint timerStart = Clock::now();
  const bool isTakerStrategy = options.isTakerStrategy();
  Market m = _public.retrieveMarket(from.currencyCode(), toCurrencyCode);
  const std::string_view buyOrSell = from.currencyCode() == m.base() ? "SELL" : "BUY";
  const std::string_view orderType = options.isTakerStrategy() ? "MARKET" : "LIMIT";

  MonetaryAmount price = _public.sanitizePrice(m, _public.computeAvgOrderPrice(m, from, isTakerStrategy, 100));
  MonetaryAmount volume = from.currencyCode() == m.quote() ? MonetaryAmount(from / price, m.base()) : from;
  MonetaryAmount sanitizedVol = _public.sanitizeVolume(m, volume, price, isTakerStrategy);

  if (volume < sanitizedVol) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", from.str(), toCurrencyCode.str(),
              sanitizedVol.str());
    return MonetaryAmount("0", toCurrencyCode);
  }

  volume = sanitizedVol;

  CurlPostData placePostData{
      {"symbol", m.assetsPairStr()}, {"side", buyOrSell}, {"type", orderType}, {"quantity", volume.amountStr()}};

  if (!isTakerStrategy) {
    placePostData.append("timeInForce", "GTC");
    placePostData.append("price", price.amountStr());
  }

  const std::string_view methodName = options.simulation() ? "api/v3/order/test" : "api/v3/order";

  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, methodName, placePostData);
  if (options.simulation()) {
    MonetaryAmount toAmount = from.currencyCode() == m.quote() ? volume : volume.convertTo(price);
    if (isTakerStrategy) {
      toAmount = _config.exchangeInfo(_public._name).applyTakerFee(toAmount);
    } else {
      toAmount = _config.exchangeInfo(_public._name).applyMakerFee(toAmount);
    }
    from -= from.currencyCode() == m.quote() ? volume.toNeutral() * price : volume;

    return toAmount;
  }

  TimePoint lastPriceUpdateTime = Clock::now();
  MonetaryAmount lastPrice = price;

  long orderId = result["orderId"].get<long>();
  bool queryOrdersInfo = false;
  TradedOrdersInfo globalTradedInfo(from.currencyCode(), toCurrencyCode);

  using OrdersIdToCheck = cct::SmallVector<long, 4>;
  OrdersIdToCheck ordersIdToCheck(1, orderId);

  MonetaryAmount remFrom = from;

  do {
    if (queryOrdersInfo) {
      // Query Order Info
      result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, methodName,
                            {{"symbol", m.assetsPairStr()}, {"orderId", std::to_string(orderId)}});
    }
    std::string_view status = result["status"].get<std::string_view>();
    if (status == "FILLED" || status == "REJECTED") {
      if (status == "REJECTED") {
        log::error("{} rejected our order", _public.name());
      } else {
        log::debug("Order filled!");
      }
      if (queryOrdersInfo) {
        if (status == "FILLED") {
          updateRemainingVolume(m, result, remFrom);
        } else {
          ordersIdToCheck.pop_back();
        }
      } else {
        // In this case result comes from the first place order - no need to double check in trade history as we have
        // the 'filled' information
        globalTradedInfo += queryOrdersAfterPlace(m, from.currencyCode(), result);
        ordersIdToCheck.pop_back();
      }

      break;
    }

    queryOrdersInfo = true;

    TimePoint t = Clock::now();

    enum class NextAction { kPlaceMarketOrder, kNewOrderLimitPrice, kWait };

    NextAction nextAction = NextAction::kWait;

    bool reachedEmergencyTime = timerStart + options.maxTradeTime() < t + options.emergencyBufferTime();
    bool updatePriceNeeded = false;
    if (!reachedEmergencyTime && lastPriceUpdateTime + options.minTimeBetweenPriceUpdates() < Clock::now()) {
      // Let's see if we need to change the price if limit price has changed.
      price = _public.computeLimitOrderPrice(m, remFrom);
      updatePriceNeeded = (from.currencyCode() == m.base() && price < lastPrice) ||
                          (from.currencyCode() == m.quote() && price > lastPrice);
    }
    if (reachedEmergencyTime || updatePriceNeeded) {
      // Cancel
      result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kDelete, methodName,
                            {{"symbol", m.assetsPairStr()}, {"orderId", std::to_string(orderId)}});
      status = result["status"].get<std::string_view>();
      if (status == "FILLED" || status == "PARTIALLY_FILLED") {
        updateRemainingVolume(m, result, remFrom);
      } else {
        // No eaten part, no need to double check trades history
        ordersIdToCheck.pop_back();
      }
      if (status == "FILLED" || status == "REJECTED") {
        if (status == "REJECTED") {
          log::error("{} rejected our order", _public.name());
        } else {
          log::debug("Order filled while we asked for cancel!");
        }
        break;
      }

      if (reachedEmergencyTime) {
        // timeout. Action depends on Strategy
        if (timerStart + options.maxTradeTime() < t) {
          log::warn("Time out reached, stop from there");
          break;
        }
        if (options.strategy() == TradeOptions::Strategy::kMakerThenTaker) {
          nextAction = NextAction::kPlaceMarketOrder;
        }
      } else {
        nextAction = NextAction::kNewOrderLimitPrice;
      }
      if (nextAction != NextAction::kWait) {
        // Compute new volume (price is either not needed in taker order, or already recomputed)
        volume = remFrom.currencyCode() == m.quote() ? MonetaryAmount(remFrom / price, m.base()) : remFrom;
        sanitizedVol = _public.sanitizeVolume(m, volume, price, isTakerStrategy);

        if (volume < sanitizedVol) {
          log::warn("No trade of {} into {} because min vol order is {} for this market", from.str(),
                    toCurrencyCode.str(), sanitizedVol.str());
          break;
        }

        volume = sanitizedVol;
        placePostData.set("quantity", volume.amountStr());
        if (nextAction == NextAction::kPlaceMarketOrder) {
          placePostData.erase("timeInForce");
          placePostData.erase("price");
          placePostData.set("type", "MARKET");
          log::warn("Reaching emergency time, make a last order at market price");
        } else {
          placePostData.set("price", price.amountStr());

          lastPriceUpdateTime = Clock::now();
          log::info("Limit price changed from {} to {}, update order", lastPrice.str(), price.str());
          lastPrice = price;
        }
        result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, methodName, placePostData);

        status = result["status"].get<std::string_view>();
        orderId = result["orderId"].get<long>();

        if (status == "FILLED" || status == "REJECTED") {
          if (status == "REJECTED") {
            log::error("{} rejected our order", _public.name());
          } else {
            log::debug("Order filled!");
          }
          TradedOrdersInfo tradeOrdersInfo = queryOrdersAfterPlace(m, from.currencyCode(), result);
          remFrom -= tradeOrdersInfo.tradedFrom;
          globalTradedInfo += tradeOrdersInfo;
          break;
        }
        ordersIdToCheck.push_back(orderId);
      }
    }

  } while (true);

  int nbRetries = 0;
  CurlHandle::Clock::duration sleepingTime = _curlHandle.minDurationBetweenQueries();
  while (!ordersIdToCheck.empty() && ++nbRetries < kNbOrderRequestsRetries) {
    // We need an additional call to trade history to get the fees, quantities matched as well
    result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "api/v3/myTrades",
                          {{"symbol", m.assetsPairStr()}});

    for (const json& fillDetail : result) {
      long tradeOrderId = fillDetail["orderId"].get<long>();
      auto findIt = std::find(ordersIdToCheck.begin(), ordersIdToCheck.end(), tradeOrderId);
      if (findIt != ordersIdToCheck.end()) {
        globalTradedInfo += queryOrder(m, from.currencyCode(), fillDetail);
        ordersIdToCheck.erase(findIt);
      }
    }
    if (!ordersIdToCheck.empty()) {
      log::warn("Binance cannot find order {} in trades history yet", ordersIdToCheck.front());
      sleepingTime = (3 * sleepingTime) / 2;
      log::trace("Wait {} ms...", std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
    }
  }

  from -= globalTradedInfo.tradedFrom;
  return globalTradedInfo.tradedTo;
}

TradedOrdersInfo BinancePrivate::queryOrdersAfterPlace(Market m, CurrencyCode fromCurrencyCode,
                                                       const json& orderJson) const {
  CurrencyCode toCurrencyCode(fromCurrencyCode == m.quote() ? m.base() : m.quote());
  TradedOrdersInfo ret(fromCurrencyCode, toCurrencyCode);

  if (orderJson.contains("fills")) {
    for (const json& fillDetail : orderJson["fills"]) {
      ret += queryOrder(m, fromCurrencyCode, fillDetail);
    }
  }

  return ret;
}

TradedOrdersInfo BinancePrivate::queryOrder(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const {
  MonetaryAmount price(fillDetail["price"].get<std::string_view>(), m.quote());
  MonetaryAmount quantity(fillDetail["qty"].get<std::string_view>(), m.base());
  MonetaryAmount quantityTimesPrice = quantity.toNeutral() * price;
  TradedOrdersInfo detailTradedInfo(fromCurrencyCode == m.quote() ? quantityTimesPrice : quantity,
                                    fromCurrencyCode == m.quote() ? quantity : quantityTimesPrice);
  MonetaryAmount fee(fillDetail["commission"].get<std::string_view>(),
                     fillDetail["commissionAsset"].get<std::string_view>());
  log::debug("Gross {} has been matched at {} price, with a fee of {}", quantity.str(), price.str(), fee.str());
  if (fee.currencyCode() == detailTradedInfo.tradedFrom.currencyCode()) {
    detailTradedInfo.tradedFrom += fee;
  } else if (fee.currencyCode() == detailTradedInfo.tradedTo.currencyCode()) {
    detailTradedInfo.tradedTo -= fee;
  } else {
    log::warn("Fee is deduced from {} which is outside {}, do not count it in this trade", fee.currencyCode().str(),
              m.str());
  }
  return detailTradedInfo;
}

void BinancePrivate::updateRemainingVolume(Market m, const json& result, MonetaryAmount& remFrom) const {
  MonetaryAmount executedVol(result["executedQty"].get<std::string_view>(), m.base());
  if (!executedVol.isZero()) {
    if (remFrom.currencyCode() == m.quote()) {
      MonetaryAmount executedPri(result["price"].get<std::string_view>(), m.quote());
      remFrom -= executedVol.toNeutral() * executedPri;
    } else {
      remFrom -= executedVol;
    }
  }
}

WithdrawInfo BinancePrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) {
  CurrencyCode currencyCode = grossAmount.currencyCode();
  Wallet destinationWallet = targetExchange.queryDepositWallet(currencyCode);
  CurlPostData withdrawPostData{
      {"asset", currencyCode.str()}, {"amount", grossAmount.amountStr()}, {"address", destinationWallet.address()}};
  if (destinationWallet.hasDestinationTag()) {
    withdrawPostData.append("addressTag", destinationWallet.destinationTag());
  }
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "wapi/v3/withdraw.html",
                             std::move(withdrawPostData));
  bool isSuccess = result["success"].get<bool>();
  std::string_view msg;
  if (result.contains("msg")) {
    msg = result["msg"].get<std::string_view>();
  }
  if (!isSuccess) {
    throw exception("Unsuccessful withdraw request of " + std::string(currencyCode.str()) +
                    ", msg = " + std::string(msg));
  }
  auto withdrawTime = WithdrawInfo::Clock::now();
  std::string_view withdrawId(result["id"].get<std::string_view>());
  log::info("Withdraw of {} to {} initiated with id {}", grossAmount.str(), destinationWallet.str(), withdrawId);
  json withdrawStatus;
  int withdrawStatusInt = -1;
  MonetaryAmount netWithdrawAmount;

  do {
    std::this_thread::sleep_for(kWithdrawInfoRefreshTime);
    withdrawStatus = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "wapi/v3/withdrawHistory.html",
                                  {{"asset", currencyCode.str()}});
    bool isSuccess = withdrawStatus["success"].get<bool>();
    std::string_view msg;
    if (withdrawStatus.contains("msg")) {
      msg = withdrawStatus["msg"].get<std::string_view>();
    }
    if (!isSuccess) {
      throw exception("Unsuccessful withdraw info request of " + std::string(currencyCode.str()) +
                      ", msg = " + std::string(msg));
    }
    for (const json& withdrawDetail : withdrawStatus["withdrawList"]) {
      std::string_view withdrawDetailId(withdrawDetail["id"].get<std::string_view>());
      if (withdrawDetailId == withdrawId) {
        withdrawStatusInt = withdrawDetail["status"].get<int>();
        switch (withdrawStatusInt) {
          case 0:
            log::warn("Email was sent");
            break;
          case 1:
            log::warn("Withdraw cancelled");
            break;
          case 2:
            log::warn("Awaiting Approval");
            break;
          case 3:
            log::error("Withdraw rejected");
            break;
          case 4:
            log::info("Processing withdraw...");
            break;
          case 5:
            log::error("Withdraw failed");
            break;
          case 6:
            log::warn("Withdraw completed!");
            break;
          default:
            log::error("unknown status value {}", withdrawStatusInt);
            break;
        }
        netWithdrawAmount = MonetaryAmount(withdrawDetail["amount"].get<std::string_view>());
        MonetaryAmount fee(withdrawDetail["transactionFee"].get<std::string_view>());
        if (netWithdrawAmount + fee != grossAmount) {
          log::error("{} + {} != {}, maybe a change in API", netWithdrawAmount.amountStr(), fee.amountStr(),
                     grossAmount.amountStr());
        }
        break;
      }
    }
  } while (withdrawStatusInt != 6);
  log::warn("Confirmed withdrawal of {} to {} {}", netWithdrawAmount.str(),
            destinationWallet.privateExchangeName().str(), destinationWallet.address());
  return WithdrawInfo(std::move(destinationWallet), withdrawTime, netWithdrawAmount);
}

}  // namespace api
}  // namespace cct