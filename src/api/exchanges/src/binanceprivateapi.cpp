#include "binanceprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "binancepublicapi.hpp"
#include "cct_nonce.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "tradeinfo.hpp"

namespace cct {
namespace api {

namespace {

/// Binance is often slow to update its databases of open / closed orders once it gives us a new order.
/// The number of retries should be sufficiently high to avoid program to crash because of this.
/// It can happen to retry 10 times
constexpr int kNbOrderRequestsRetries = 15;

void SetNonceAndSignature(const APIKey& apiKey, CurlPostData& postData) {
  Nonce nonce = Nonce_TimeSinceEpoch();
  postData.set("timestamp", nonce);
  // Erase + append signature as it should be computed without the old signature itself
  postData.erase("signature");
  postData.append("signature", ssl::ShaHex(ssl::ShaType::kSha256, postData.str(), apiKey.privateKey()));
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, CurlOptions::RequestType requestType,
                  std::string_view baseURL, std::string_view method, CurlPostDataT&& curlPostData = CurlPostData()) {
  string url(baseURL);
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
    Clock::duration sleepingTime = curlHandle.minDurationBetweenQueries();
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
    string ex("Error: ");
    ex.append(MonetaryAmount(statusCode).amountStr());
    ex.append(", msg: ");
    ex.append(dataJson["msg"].get<std::string_view>());
    throw exception(std::move(ex));
  }
  return dataJson;
}

}  // namespace

BinancePrivate::BinancePrivate(const CoincenterInfo& config, BinancePublic& binancePublic, const APIKey& apiKey)
    : ExchangePrivate(binancePublic, config, apiKey),
      _curlHandle(config.metricGatewayPtr(), config.exchangeInfo(binancePublic.name()).minPrivateQueryDelay(),
                  config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic),
      _allWithdrawFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic),
      _withdrawFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, binancePublic) {}

CurrencyExchangeFlatSet BinancePrivate::TradableCurrenciesCache::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, _public._commonInfo.getBestBaseURL(),
                             "sapi/v1/capital/config/getall");
  return _public.queryTradableCurrencies(result);
}

BalancePortfolio BinancePrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet,
                             binancePublic._commonInfo.getBestBaseURL(), "api/v3/account");
  BalancePortfolio balancePortfolio;
  for (const json& balance : result["balances"]) {
    CurrencyCode currencyCode(balance["asset"].get<std::string_view>());
    MonetaryAmount amount(balance["free"].get<std::string_view>(), currencyCode);

    addBalance(balancePortfolio, amount, equiCurrency);
  }
  return balancePortfolio;
}

Wallet BinancePrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, _public._commonInfo.getBestBaseURL(),
                             "sapi/v1/capital/deposit/address", {{"coin", currencyCode.str()}});
  std::string_view address(result["address"].get<std::string_view>());
  std::string_view tag(result["tag"].get<std::string_view>());
  std::string_view url(result["url"].get<std::string_view>());
  const CoincenterInfo& coincenterInfo = _public.coincenterInfo();
  PrivateExchangeName privateExchangeName(_public.name(), _apiKey.name());
  bool doCheckWallet = coincenterInfo.exchangeInfo(privateExchangeName.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet w(std::move(privateExchangeName), currencyCode, address, tag, walletCheck);
  log::info("Retrieved {} (URL: '{}')", w.str(), url);
  return w;
}

ExchangePublic::WithdrawalFeeMap BinancePrivate::AllWithdrawFeesFunc::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet,
                             _exchangePublic._commonInfo.getBestBaseURL(), "sapi/v1/asset/assetDetail");
  WithdrawalFeeMap ret;
  for (const auto& [curCodeStr, withdrawFeeDetails] : result.items()) {
    if (withdrawFeeDetails["withdrawStatus"].get<bool>()) {
      CurrencyCode cur(curCodeStr);
      ret.insert_or_assign(cur, MonetaryAmount(withdrawFeeDetails["withdrawFee"].get<std::string_view>(), cur));
    }
  }
  return ret;
}

MonetaryAmount BinancePrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  json result =
      PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, _exchangePublic._commonInfo.getBestBaseURL(),
                   "sapi/v1/asset/assetDetail", {{"asset", currencyCode.str()}});
  if (!result.contains(currencyCode.str())) {
    throw exception("Unable to find asset information in assetDetail query to Binance");
  }
  const json& withdrawFeeDetails = result[string(currencyCode.str())];
  if (!withdrawFeeDetails["withdrawStatus"].get<bool>()) {
    log::error("{} is currently unavailable for withdraw from {}", currencyCode.str(), _exchangePublic.name());
  }
  return MonetaryAmount(withdrawFeeDetails["withdrawFee"].get<std::string_view>(), currencyCode);
}

PlaceOrderInfo BinancePrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  const CurrencyCode toCurrencyCode(tradeInfo.toCurrencyCode);
  const Market m = tradeInfo.m;
  const std::string_view buyOrSell = fromCurrencyCode == m.base() ? "SELL" : "BUY";
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy();
  const std::string_view orderType = isTakerStrategy ? "MARKET" : "LIMIT";
  const bool isSimulation = tradeInfo.options.isSimulation();

  price = binancePublic.sanitizePrice(m, price);

  MonetaryAmount sanitizedVol = binancePublic.sanitizeVolume(m, volume, price, isTakerStrategy);

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));
  if (volume < sanitizedVol) {
    constexpr CurrencyCode kBinanceCoinCur("BNB");
    if (!isSimulation && m.canTrade(kBinanceCoinCur) && from.currencyCode() != kBinanceCoinCur) {
      // Use special Binance Dust transfer
      log::info("Volume too low for standard trade, but we can use Dust transfer to trade to {}",
                kBinanceCoinCur.str());
      json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost,
                                 binancePublic._commonInfo.getBestBaseURL(), "/sapi/v1/asset/dust",
                                 {{"asset", from.currencyStr()}});
      if (!result.contains("transferResult") || result["transferResult"].empty()) {
        throw exception("Unexpected dust transfer result");
      }
      const json& res = result["transferResult"].front();
      SetString(placeOrderInfo.orderId, res["tranId"].get<std::size_t>());
      MonetaryAmount netTransferredAmount(res["transferedAmount"].get<std::string_view>(), kBinanceCoinCur);
      placeOrderInfo.tradedAmounts() += TradedAmounts(from, netTransferredAmount);
    } else {
      log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(),
                toCurrencyCode.str(), sanitizedVol.str());
    }

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

  const std::string_view methodName = isSimulation ? "api/v3/order/test" : "api/v3/order";

  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost,
                             binancePublic._commonInfo.getBestBaseURL(), methodName, placePostData);
  if (isSimulation) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }
  SetString(placeOrderInfo.orderId, result["orderId"].get<size_t>());
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
  const string assetsStr = m.assetsPairStr();
  const std::string_view assets(assetsStr);
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  json result = PrivateQuery(_curlHandle, _apiKey, requestType, binancePublic._commonInfo.getBestBaseURL(),
                             "api/v3/order", {{"symbol", assets}, {"orderId", orderId}});
  const std::string_view status = result["status"].get<std::string_view>();
  bool isClosed = false;
  bool queryClosedOrder = false;
  if (status == "FILLED" || status == "CANCELED") {
    isClosed = true;
    queryClosedOrder = true;
  } else if (status == "REJECTED" || status == "EXPIRED") {
    log::error("{} rejected our order {} with status {}", _exchangePublic.name(), orderId, status);
    isClosed = true;
  }
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  if (queryClosedOrder) {
    CurlPostData myTradesOpts{{"symbol", assets}};
    auto timeIt = result.find("time");
    if (timeIt != result.end()) {
      myTradesOpts.append("startTime", timeIt->get<int64_t>() - 100L);  // -100 just to be sure
    }
    result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet,
                          binancePublic._commonInfo.getBestBaseURL(), "api/v3/myTrades", myTradesOpts);
    int64_t integralOrderId = FromString<int64_t>(orderId);
    for (const json& tradeDetails : result) {
      if (tradeDetails["orderId"].get<int64_t>() == integralOrderId) {
        orderInfo.tradedAmounts += parseTrades(m, fromCurrencyCode, tradeDetails);
      }
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
      ret += parseTrades(m, fromCurrencyCode, fillDetail);
    }
  }

  return ret;
}

TradedAmounts BinancePrivate::parseTrades(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const {
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
    log::warn("Fee is deduced from {} which is outside {}, do not count it in this trade", fee.currencyStr(), m.str());
  }
  return detailTradedInfo;
}

InitiatedWithdrawInfo BinancePrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  CurlPostData withdrawPostData{
      {"coin", currencyCode.str()}, {"address", wallet.address()}, {"amount", grossAmount.amountStr()}};
  if (wallet.hasTag()) {
    withdrawPostData.append("addressTag", wallet.tag());
  }
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  json result =
      PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, binancePublic._commonInfo.getBestBaseURL(),
                   "sapi/v1/capital/withdraw/apply", withdrawPostData);
  std::string_view withdrawId(result["id"].get<std::string_view>());
  return InitiatedWithdrawInfo(std::move(wallet), withdrawId, grossAmount);
}

SentWithdrawInfo BinancePrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  json withdrawStatus =
      PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, binancePublic._commonInfo.getBestBaseURL(),
                   "sapi/v1/capital/withdraw/history", {{"coin", currencyCode.str()}});
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  MonetaryAmount netEmittedAmount;
  bool isWithdrawSent = false;
  for (const json& withdrawDetail : withdrawStatus) {
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
  BinancePublic& binancePublic = dynamic_cast<BinancePublic&>(_exchangePublic);
  json depositStatus =
      PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, binancePublic._commonInfo.getBestBaseURL(),
                   "sapi/v1/capital/deposit/hisrec", {{"coin", currencyCode.str()}});
  for (const json& depositDetail : depositStatus) {
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
