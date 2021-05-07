#include "binanceprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "binancepublicapi.hpp"
#include "cct_nonce.hpp"
#include "ssl_sha.hpp"
#include "tradeoptions.hpp"

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

BinancePrivate::BinancePrivate(const CoincenterInfo& config, BinancePublic& binancePublic, const APIKey& apiKey)
    : ExchangePrivate(binancePublic, config, apiKey),
      _curlHandle(config.exchangeInfo(binancePublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic) {}

CurrencyExchangeFlatSet BinancePrivate::queryTradableCurrencies() { return _exchangePublic.queryTradableCurrencies(); }

BalancePortfolio BinancePrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "api/v3/account");
  BalancePortfolio balancePortfolio;
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
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
        log::info("{} Balance {}", _exchangePublic.name(), available.str());
        balancePortfolio.add(available, MonetaryAmount("0", equiCurrency));
      } else {
        MonetaryAmount equivalentInMainCurrency =
            binancePublic.computeEquivalentInMainCurrency(available, equiCurrency);
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

PlaceOrderInfo BinancePrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  const CurrencyCode toCurrencyCode(tradeInfo.toCurrencyCode);
  const Market m = tradeInfo.m;
  const std::string_view buyOrSell = fromCurrencyCode == m.base() ? "SELL" : "BUY";
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy();
  const std::string_view orderType = isTakerStrategy ? "MARKET" : "LIMIT";

  price = binancePublic.sanitizePrice(m, price);

  MonetaryAmount sanitizedVol = binancePublic.sanitizeVolume(m, volume, price, isTakerStrategy);

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));
  if (volume < sanitizedVol) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(), toCurrencyCode.str(),
              sanitizedVol.str());
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
  volume = sanitizedVol;
  CurlPostData placePostData{
      {"symbol", m.assetsPairStr()}, {"side", buyOrSell}, {"type", orderType}, {"quantity", volume.amountStr()}};

  if (!isTakerStrategy) {
    placePostData.append("timeInForce", "GTC");
    placePostData.append("price", price.amountStr());
  }

  const bool isSimulation = tradeInfo.options.isSimulation();
  const std::string_view methodName = isSimulation ? "api/v3/order/test" : "api/v3/order";

  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, methodName, placePostData);
  if (isSimulation) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
  placeOrderInfo.orderId = std::to_string(result["orderId"].get<long>());
  std::string_view status = result["status"].get<std::string_view>();
  if (status == "FILLED" || status == "REJECTED" || status == "EXPIRED") {
    if (status == "FILLED") {
      placeOrderInfo.tradedAmounts() += queryOrdersAfterPlace(m, fromCurrencyCode, result);
    } else {
      log::error("{} rejected our place order with status {}", _exchangePublic.name(), status);
    }

    placeOrderInfo.setClosed();
  }
  return placeOrderInfo;
}

OrderInfo BinancePrivate::queryOrder(const OrderId& orderId, const TradeInfo& tradeInfo, bool isCancel) {
  const CurrencyCode fromCurrencyCode = tradeInfo.fromCurrencyCode;
  const CurrencyCode toCurrencyCode = tradeInfo.toCurrencyCode;
  const Market m = tradeInfo.m;
  const CurlOptions::RequestType requestType =
      isCancel ? CurlOptions::RequestType::kDelete : CurlOptions::RequestType::kGet;
  json result = PrivateQuery(_curlHandle, _apiKey, requestType, "api/v3/order",
                             {{"symbol", m.assetsPairStr()}, {"orderId", orderId}});
  std::string_view status = result["status"].get<std::string_view>();
  bool isClosed = false;
  if (status == "FILLED" || status == "CANCELED") {
    isClosed = true;
  } else if (status == "REJECTED" || status == "EXPIRED") {
    log::error("{} rejected our order {} with status {}", _exchangePublic.name(), orderId, status);
    isClosed = true;
  }
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  MonetaryAmount executedVol(result["executedQty"].get<std::string_view>(), m.base());
  if (!executedVol.isZero()) {
    MonetaryAmount executedPri(result["price"].get<std::string_view>(), m.quote());
    if (fromCurrencyCode == m.quote()) {
      orderInfo.tradedAmounts.tradedFrom += executedVol.toNeutral() * executedPri;
      orderInfo.tradedAmounts.tradedTo += executedVol;
    } else {
      orderInfo.tradedAmounts.tradedFrom += executedVol;
      orderInfo.tradedAmounts.tradedTo += executedVol.toNeutral() * executedPri;
    }
  }
  return orderInfo;
}

TradedAmounts BinancePrivate::queryOrdersAfterPlace(Market m, CurrencyCode fromCurrencyCode,
                                                    const json& orderJson) const {
  CurrencyCode toCurrencyCode(fromCurrencyCode == m.quote() ? m.base() : m.quote());
  TradedAmounts ret(fromCurrencyCode, toCurrencyCode);

  if (orderJson.contains("fills")) {
    for (const json& fillDetail : orderJson["fills"]) {
      ret += queryOrder(m, fromCurrencyCode, fillDetail);
    }
  }

  return ret;
}

TradedAmounts BinancePrivate::queryOrder(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const {
  MonetaryAmount price(fillDetail["price"].get<std::string_view>(), m.quote());
  MonetaryAmount quantity(fillDetail["qty"].get<std::string_view>(), m.base());
  MonetaryAmount quantityTimesPrice = quantity.toNeutral() * price;
  TradedAmounts detailTradedInfo(fromCurrencyCode == m.quote() ? quantityTimesPrice : quantity,
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

InitiatedWithdrawInfo BinancePrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  CurlPostData withdrawPostData{
      {"asset", currencyCode.str()}, {"amount", grossAmount.amountStr()}, {"address", wallet.address()}};
  if (wallet.hasDestinationTag()) {
    withdrawPostData.append("addressTag", wallet.destinationTag());
  }
  json result =
      PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "wapi/v3/withdraw.html", withdrawPostData);
  bool isSuccess = result["success"].get<bool>();
  if (!isSuccess) {
    std::string_view msg = result.contains("msg") ? result["msg"].get<std::string_view>() : "";
    throw exception("Unsuccessful withdraw request of " + std::string(currencyCode.str()) +
                    ", msg = " + std::string(msg));
  }
  std::string_view withdrawId(result["id"].get<std::string_view>());
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo BinancePrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json withdrawStatus = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet,
                                     "wapi/v3/withdrawHistory.html", {{"asset", currencyCode.str()}});
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  MonetaryAmount netEmittedAmount;
  bool isWithdrawSent = false;
  for (const json& withdrawDetail : withdrawStatus["withdrawList"]) {
    std::string_view withdrawDetailId(withdrawDetail["id"].get<std::string_view>());
    if (withdrawDetailId == withdrawId) {
      int withdrawStatusInt = withdrawDetail["status"].get<int>();
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
          isWithdrawSent = true;
          break;
        default:
          log::error("unknown status value {}", withdrawStatusInt);
          break;
      }
      netEmittedAmount = MonetaryAmount(withdrawDetail["amount"].get<double>(), currencyCode);
      MonetaryAmount fee(withdrawDetail["transactionFee"].get<double>(), currencyCode);
      if (netEmittedAmount + fee != initiatedWithdrawInfo.grossEmittedAmount()) {
        log::error("{} + {} != {}, maybe a change in API", netEmittedAmount.amountStr(), fee.amountStr(),
                   initiatedWithdrawInfo.grossEmittedAmount().amountStr());
      }
      break;
    }
  }
  return SentWithdrawInfo(netEmittedAmount, isWithdrawSent);
}

bool BinancePrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                        const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  json depositStatus = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "wapi/v3/depositHistory.html",
                                    {{"asset", currencyCode.str()}});
  for (const json& depositDetail : depositStatus["depositList"]) {
    std::string_view depositAddress(depositDetail["address"].get<std::string_view>());
    if (depositAddress == initiatedWithdrawInfo.receivingWallet().address()) {
      MonetaryAmount amountReceived(depositDetail["amount"].get<double>(), currencyCode);
      if (amountReceived == sentWithdrawInfo.netEmittedAmount()) {
        return true;
      }
      log::debug("{}: similar deposit found with different amount {} (expected {})", _exchangePublic.name(),
                 amountReceived.str(), sentWithdrawInfo.netEmittedAmount().str());
    }
  }
  return false;
}

}  // namespace api
}  // namespace cct
