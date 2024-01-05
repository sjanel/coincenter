#include "bithumbprivateapi.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <thread>
#include <utility>

#include "accountowner.hpp"
#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "bithumbpublicapi.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangeinfo.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "runmodes.hpp"
#include "ssl_sha.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeoptions.hpp"
#include "tradeside.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"
#include "withdrawsordepositsconstraints.hpp"

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

constexpr int kBadRequestErrorCode = 5100;
constexpr int kOrderRelatedErrorCode = 5600;

constexpr std::string_view kWalletAddressEndpointStr = "/info/wallet_address";

std::pair<string, Nonce> GetStrData(std::string_view endpoint, std::string_view postDataStr) {
  Nonce nonce = Nonce_TimeSinceEpochInMs();
  static constexpr char kParChar = 1;
  string strData(endpoint.size() + 2U + postDataStr.size() + nonce.size(), kParChar);

  auto it = std::ranges::copy(endpoint, strData.begin()).out;
  it = std::ranges::copy(postDataStr, it + 1).out;
  it = std::ranges::copy(nonce, it + 1).out;
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
  auto strDataAndNoncePair = GetStrData(endpoint, opts.postData().str());

  string signature = B64Encode(ssl::ShaHex(ssl::ShaType::kSha512, strDataAndNoncePair.first, apiKey.privateKey()));

  SetHttpHeaders(opts, apiKey, signature, strDataAndNoncePair.second);
  return json::parse(curlHandle.query(endpoint, opts));
}

template <class ValueType>
bool LoadCurrencyInfoField(const json& currencyOrderInfoJson, std::string_view keyStr, ValueType& val, TimePoint& ts) {
  auto subPartIt = currencyOrderInfoJson.find(keyStr);
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
  data.emplace(kTimestampKeyStr, TimestampToS(ts));
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

bool IsSynchronizedTime(std::string_view msg) {
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
        log::error("Bithumb time is not synchronized with us (difference of {} s)", (reqTimeInt - nowTimeInt) / 1000);
        log::error("It can sometimes come from a Bithumb bug, retry");
        return false;
      }
    }
  }
  return false;
}

bool CheckOrderErrors(std::string_view endpoint, std::string_view msg, json& data) {
  const bool isInfoOpenedOrders = endpoint == "/info/orders";
  const bool isCancelQuery = endpoint == "/trade/cancel";
  const bool isDepositInfo = endpoint == kWalletAddressEndpointStr;
  const bool isPlaceOrderQuery = !isCancelQuery && endpoint.starts_with("/trade");

  if (isPlaceOrderQuery) {
    if (ExtractError<int8_t>("수량은 소수점 ", "자", "number of decimals", msg, kNbDecimalsStr, data) ||
        ExtractError<string>("주문금액은 ", " 입니다", "min order size", msg, kMinOrderSizeJsonKeyStr, data) ||
        ExtractError<string>("주문 가격은 ", " 이상으로 입력 가능합니다", "min order price", msg,
                             kMinOrderPriceJsonKeyStr, data) ||
        ExtractError<string>("주문 가격은 ", " 이하로 입력 가능합니다", "max order price", msg,
                             kMaxOrderPriceJsonKeyStr, data)) {
      return true;
    }
  }
  if ((isInfoOpenedOrders || isCancelQuery) &&
      msg.find("거래 진행중인 내역이 존재하지 않습니다") != std::string_view::npos) {
    // This is not really an error, it means that order has been eaten or cancelled.
    // Just return empty json in this case
    log::info("Considering Bithumb order as closed as no data received from them");
    data.clear();
    return true;
  }
  if (isDepositInfo && msg.find("잘못된 접근입니다.") != std::string_view::npos) {
    data.clear();
    return true;
  }
  return false;
}

int64_t ErrCodeFromQueryResponse(const json& queryResponse) {
  auto statusCodeIt = queryResponse.find("status");
  if (statusCodeIt != queryResponse.end()) {
    return FromString<int64_t>(statusCodeIt->get<std::string_view>());
  }
  return 0;
}

json PrivateQueryProcessWithRetries(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint,
                                    bool throwIfError, CurlOptions& opts) {
  json ret = PrivateQueryProcess(curlHandle, apiKey, endpoint, opts);

  // Example of error json: {"status":"5300","message":"Invalid Apikey"}
  constexpr int kMaxNbRetries = 5;
  int nbRetries = 0;
  int64_t errorCode = ErrCodeFromQueryResponse(ret);
  while (errorCode != 0 && ++nbRetries < kMaxNbRetries) {
    // "5300" for instance. "0000" stands for: request OK
    std::string_view msg;
    auto messageIt = ret.find("message");
    if (messageIt != ret.end()) {
      msg = messageIt->get<std::string_view>();
      switch (errorCode) {
        case kBadRequestErrorCode:
          if (!IsSynchronizedTime(msg)) {
            ret = PrivateQueryProcess(curlHandle, apiKey, endpoint, opts);
            errorCode = ErrCodeFromQueryResponse(ret);
            continue;
          }
          break;
        case kOrderRelatedErrorCode:
          if (CheckOrderErrors(endpoint, msg, ret)) {
            return ret;
          }
          break;
        default:
          break;
      }
    }
    break;
  }
  if (errorCode != 0 && throwIfError) {
    log::error("Full Bithumb json error: '{}'", ret.dump());
    throw exception("Bithumb error {}", errorCode);
  }
  return ret;
}

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint,
                  CurlPostDataT&& curlPostData = CurlPostData(), bool throwIfError = true) {
  CurlPostData postData(std::forward<CurlPostDataT>(curlPostData));
  postData.prepend("endpoint", endpoint);
  CurlOptions opts(HttpRequestType::kPost, postData.urlEncodeExceptDelimiters());

  return PrivateQueryProcessWithRetries(curlHandle, apiKey, endpoint, throwIfError, opts);
}

File GetBithumbCurrencyInfoMapCache(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "bithumbcurrencyinfocache.json", File::IfError::kNoThrow};
}

}  // namespace

BithumbPrivate::BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey)
    : ExchangePrivate(config, bithumbPublic, apiKey),
      _curlHandle(BithumbPublic::kUrlBase, config.metricGatewayPtr(),
                  PermanentCurlOptions::Builder()
                      .setMinDurationBetweenQueries(exchangeInfo().privateAPIRate())
                      .setAcceptedEncoding(exchangeInfo().acceptEncoding())
                      .setRequestCallLogLevel(exchangeInfo().requestsCallLogLevel())
                      .setRequestAnswerLogLevel(exchangeInfo().requestsAnswerLogLevel())
                      .build(),
                  config.getRunMode()),
      _currencyOrderInfoRefreshTime(exchangeInfo().getAPICallUpdateFrequency(kCurrencyInfoBithumb)),
      _depositWalletsCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, bithumbPublic) {
  if (config.getRunMode() != settings::RunMode::kQueryResponseOverriden) {
    json data = GetBithumbCurrencyInfoMapCache(_coincenterInfo.dataDir()).readAllJson();
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
}

bool BithumbPrivate::validateApiKey() {
  constexpr bool throwIfError = false;
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/balance", CurlPostData(), throwIfError);
  auto statusIt = result.find("status");
  return statusIt != result.end() && statusIt->get<std::string_view>() == BithumbPublic::kStatusOKStr;
}

BalancePortfolio BithumbPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/balance", {{"currency", "all"}})["data"];
  BalancePortfolio balancePortfolio;
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();

  static constexpr std::array<std::string_view, 2> kKnownPrefixes = {"available_", "in_use_"};
  auto endPrefixIt = kKnownPrefixes.end();
  if (balanceOptions.amountIncludePolicy() != BalanceOptions::AmountIncludePolicy::kWithBalanceInUse) {
    --endPrefixIt;
  }

  for (const auto& [key, value] : result.items()) {
    for (auto prefixIt = kKnownPrefixes.begin(); prefixIt != endPrefixIt; ++prefixIt) {
      if (key.starts_with(*prefixIt)) {
        std::string_view curStr(key.begin() + prefixIt->size(), key.end());
        MonetaryAmount amount(value.get<std::string_view>(), _coincenterInfo.standardizeCurrencyCode(curStr));
        this->addBalance(balancePortfolio, amount, equiCurrency);
        break;
      }
    }
  }
  return balancePortfolio;
}

Wallet BithumbPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  json ret = PrivateQuery(_curlHandle, _apiKey, kWalletAddressEndpointStr, {{"currency", currencyCode.str()}});
  if (ret.empty()) {
    throw exception(
        "Bithumb wallet is not created for {}, it should be done with the UI first (no way to do it via API)",
        currencyCode);
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
  Wallet wallet(ExchangeName(_exchangePublic.name(), _apiKey.name()), currencyCode, string(address), tag, walletCheck,
                _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
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
        params.append(kPaymentCurParamStr, filterMarket.quote().str());
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
          if (amount != 0) {
            orderCurrencies.push_back(cur);
          }
        }
      }
    }
  }

  Orders openedOrders;
  if (openedOrdersConstraints.isPlacedTimeAfterDefined()) {
    params.append("after", TimestampToMs(openedOrdersConstraints.placedAfter()));
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

      string id = std::move(orderDetails[kOrderIdParamStr].get_ref<string&>());
      if (!openedOrdersConstraints.validateOrderId(id)) {
        continue;
      }

      CurrencyCode priceCur(orderDetails[kPaymentCurParamStr].get<std::string_view>());
      MonetaryAmount originalVolume(orderDetails["units"].get<std::string_view>(), volumeCur);
      MonetaryAmount remainingVolume(orderDetails["units_remaining"].get<std::string_view>(), volumeCur);
      MonetaryAmount matchedVolume = originalVolume - remainingVolume;
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
      TradeSide side =
          orderDetails[kTypeParamStr].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

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
  for (const Order& order : orders) {
    TradeContext tradeContext(order.market(), order.side());
    cancelOrderProcess(order.id(), tradeContext);
  }
  return orders.size();
}

namespace {
constexpr int kSearchGbOnGoingWithdrawals = 3;
constexpr int kSearchGbDeposit = 4;
constexpr int kSearchGbProcessedWithdrawals = 5;
constexpr int kSearchGbKRWDeposits = 9;

int64_t RetrieveMicrosecondsSinceEpochFromTrxJson(const json& trx) {
  // In the official documentation, transfer_date field is an integer.
  // But in fact (as of 2022) it's a string representation of the integer timestamp.
  // Let's support both types to be safe.
  // Bithumb returns a UTC based timestamp.
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

  return microsecondsSinceEpoch;
}
string GenerateDepositIdFromTrx(int64_t microsecondsSinceEpoch, const json& trx) {
  // Bithumb does not provide any transaction id, let's generate it from currency and timestamp...
  string id{trx[kOrderCurrencyParamStr].get<std::string_view>()};
  id.push_back('-');
  AppendString(id, microsecondsSinceEpoch);
  return id;
}

string GenerateWithdrawIdFromTrx(MonetaryAmount netEmittedAmount, const json& trx) {
  // We cannot use the timestamp for the withdraw ID because it changes where is switches from the state 'withdrawing'
  // to the state 'withdraw done' (searchGb 3->5)
  // There are two fields that does not seem to change over time, and we are going to use them to generate our id:
  //  - order_balance
  //  - payment_balance
  MonetaryAmount orderBalance(trx["order_balance"].get<std::string_view>());
  MonetaryAmount paymentBalance(trx["payment_balance"].get<std::string_view>());
  string withdrawId = netEmittedAmount.str();
  withdrawId.push_back(';');
  orderBalance.appendStrTo(withdrawId);
  withdrawId.push_back(';');
  paymentBalance.appendStrTo(withdrawId);
  return B64Encode(withdrawId);
}

CurrencyCode CurrencyCodeFromTrx(const json& trx) { return {trx["order_currency"].get<std::string_view>()}; }

MonetaryAmount RetrieveAmountFromTrxJson(const json& trx) {
  // starts with "+ " for a deposit, "- " for a withdraw, return absolute
  std::string_view amountStr = trx["units"].get<std::string_view>();

  CurrencyCode currencyCode = CurrencyCodeFromTrx(trx);
  MonetaryAmount amount{amountStr, currencyCode};
  if (amount == 0) {
    // It's strange but for 'KRW' withdraws, Bithumb returns the KRW amount in the 'price' field (as negative!)
    amount = MonetaryAmount{trx["price"].get<std::string_view>(), currencyCode};
  }
  return amount.abs();
}

MonetaryAmount RetrieveFeeFromTrxJson(const json& trx) {
  std::string_view feeStr = trx["fee"].get<std::string_view>();  // starts with "+ "
  CurrencyCode currencyCode(trx["fee_currency"].get<std::string_view>());
  return {feeStr, currencyCode};
}

MonetaryAmount RetrieveNetEmittedAmountFromTrxJson(const json& trx) {
  MonetaryAmount grossEmittedAmount = RetrieveAmountFromTrxJson(trx);
  MonetaryAmount fee = RetrieveFeeFromTrxJson(trx);
  return grossEmittedAmount - fee;
}

Withdraw::Status RetrieveWithdrawStatusFromTrxJson(const json& trx) {
  int searchGb = FromString<int>(trx["search"].get<std::string_view>());

  return searchGb == kSearchGbOnGoingWithdrawals ? Withdraw::Status::kProcessing : Withdraw::Status::kSuccess;
}

}  // namespace

json BithumbPrivate::queryRecentTransactions(const WithdrawsOrDepositsConstraints& withdrawsOrDepositsConstraints,
                                             UserTransactionEnum depositOrWithdrawEnum) {
  // It's not clear what the payment currency option is for user_transactions endpoint for deposits and withdraws.
  // For withdraws it seems to have no impact, and even worse, it returns weird output when
  // querying withdraws for a specific coin, it can return KRW withdraws to user bank account.
  CurlPostData options{{"count", 50}, {kPaymentCurParamStr, "BTC"}};
  SmallVector<CurrencyCode, 1> orderCurrencies;

  if (withdrawsOrDepositsConstraints.isCurDefined()) {
    orderCurrencies.push_back(withdrawsOrDepositsConstraints.currencyCode());
  } else {
    log::warn("Retrieval of recent deposits / withdraws should be done currency by currency for {:e}", exchangeName());
    log::warn("Heuristic: only query for currencies which are present in the balance");
    log::warn("Doing such, we may miss some recent deposits in other currencies");
    for (const auto& amountWithEquivalent : queryAccountBalance()) {
      CurrencyCode currencyCode = amountWithEquivalent.amount.currencyCode();
      orderCurrencies.push_back(currencyCode);
    }
  }
  json allResults;
  FixedCapacityVector<int, 2> searchGbsVector;
  switch (depositOrWithdrawEnum) {
    case UserTransactionEnum::kDeposit:
      searchGbsVector.push_back(kSearchGbDeposit);
      break;
    case UserTransactionEnum::kOngoingWithdraws:
      searchGbsVector.push_back(kSearchGbOnGoingWithdrawals);
      break;
    case UserTransactionEnum::kProcessedWithdraws:
      searchGbsVector.push_back(kSearchGbProcessedWithdrawals);
      break;
    case UserTransactionEnum::kAllWithdraws:
      searchGbsVector.push_back(kSearchGbOnGoingWithdrawals);
      searchGbsVector.push_back(kSearchGbProcessedWithdrawals);
      break;
    default:
      throw exception("Unexpected deposit or withdraw enum value {}", static_cast<int>(depositOrWithdrawEnum));
  }

  for (CurrencyCode currencyCode : orderCurrencies) {
    CurrencyCode orderCurrencyCode = currencyCode;
    if (currencyCode == "KRW") {
      // Strange bug (or something that I did not understand):
      // - when querying explicitly an order currency KRW, we get an error, even if user has KRW deposits/withdraws
      // - when querying a coin, we get the KRW user withdraws to bank (!)
      orderCurrencyCode = "BTC";
    }
    options.set(kOrderCurrencyParamStr, orderCurrencyCode.str());
    if (depositOrWithdrawEnum == UserTransactionEnum::kDeposit) {
      if (currencyCode == "KRW") {
        searchGbsVector.resize(2, kSearchGbKRWDeposits);
      } else {
        searchGbsVector.resize(1, kSearchGbDeposit);
      }
    }

    for (int searchGb : searchGbsVector) {
      options.set("searchGb", searchGb);
      json txrList = PrivateQuery(_curlHandle, _apiKey, "/info/user_transactions", options)["data"];
      for (json& trx : txrList) {
        if (!withdrawsOrDepositsConstraints.validateCur(CurrencyCodeFromTrx(trx))) {
          continue;
        }

        int64_t microsecondsSinceEpoch = RetrieveMicrosecondsSinceEpochFromTrxJson(trx);
        TimePoint timestamp{std::chrono::microseconds(microsecondsSinceEpoch)};
        if (!withdrawsOrDepositsConstraints.validateTime(timestamp)) {
          continue;
        }

        if (depositOrWithdrawEnum == UserTransactionEnum::kDeposit) {
          if (!withdrawsOrDepositsConstraints.validateId(GenerateDepositIdFromTrx(microsecondsSinceEpoch, trx))) {
            continue;
          }
        } else {
          MonetaryAmount netEmittedAmount = RetrieveNetEmittedAmountFromTrxJson(trx);
          if (!withdrawsOrDepositsConstraints.validateId(GenerateWithdrawIdFromTrx(netEmittedAmount, trx))) {
            continue;
          }
        }

        allResults.push_back(std::move(trx));
      }
    }
  }
  return allResults;
}

DepositsSet BithumbPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;

  json txrList = queryRecentTransactions(depositsConstraints, UserTransactionEnum::kDeposit);
  deposits.reserve(txrList.size());
  for (const json& trx : txrList) {
    int64_t microsecondsSinceEpoch = RetrieveMicrosecondsSinceEpochFromTrxJson(trx);
    TimePoint timestamp{std::chrono::microseconds(microsecondsSinceEpoch)};

    string id = GenerateDepositIdFromTrx(microsecondsSinceEpoch, trx);

    // No status information returned by Bithumb. Defaulting to Success
    deposits.emplace_back(std::move(id), timestamp, RetrieveAmountFromTrxJson(trx), Deposit::Status::kSuccess);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

WithdrawsSet BithumbPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;

  json txrList = queryRecentTransactions(withdrawsConstraints, UserTransactionEnum::kAllWithdraws);
  withdraws.reserve(txrList.size());
  for (const json& trx : txrList) {
    int64_t microsecondsSinceEpoch = RetrieveMicrosecondsSinceEpochFromTrxJson(trx);
    TimePoint timestamp{std::chrono::microseconds(microsecondsSinceEpoch)};

    MonetaryAmount fee = RetrieveFeeFromTrxJson(trx);
    MonetaryAmount netEmittedAmount = RetrieveNetEmittedAmountFromTrxJson(trx);

    withdraws.emplace_back(GenerateWithdrawIdFromTrx(netEmittedAmount, trx), timestamp, netEmittedAmount,
                           RetrieveWithdrawStatusFromTrxJson(trx), fee);
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

PlaceOrderInfo BithumbPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  const bool placeSimulatedRealOrder = _exchangePublic.exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market mk = tradeInfo.tradeContext.mk;

  // It seems Bithumb uses "standard" currency codes, no need to translate them
  CurlPostData placePostData{{kOrderCurrencyParamStr, mk.base().str()}, {kPaymentCurParamStr, mk.quote().str()}};
  const std::string_view orderType = fromCurrencyCode == mk.base() ? "ask" : "bid";

  string endpoint("/trade/");
  if (isTakerStrategy) {
    endpoint.append(fromCurrencyCode == mk.base() ? "market_sell" : "market_buy");
  } else {
    endpoint.append("place");
    placePostData.append(kTypeParamStr, orderType);
    placePostData.append("price", price.amountStr());
  }

  // Volume is gross amount if from amount is in quote currency, we should remove the fees
  if (fromCurrencyCode == mk.quote()) {
    ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
    const ExchangeInfo& exchangeInfo = _coincenterInfo.exchangeInfo(_exchangePublic.name());
    volume = exchangeInfo.applyFee(volume, feeType);
  }

  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;

  auto currencyOrderInfoIt = _currencyOrderInfoMap.find(mk.base());
  auto nowTime = Clock::now();
  CurrencyOrderInfo currencyOrderInfo;
  if (currencyOrderInfoIt != _currencyOrderInfoMap.end()) {
    currencyOrderInfo = currencyOrderInfoIt->second;
    if (currencyOrderInfo.lastNbDecimalsUpdatedTime + _currencyOrderInfoRefreshTime > nowTime) {
      int8_t nbMaxDecimalsUnits = currencyOrderInfo.nbDecimals;
      volume.truncate(nbMaxDecimalsUnits);
      if (volume == 0) {
        log::warn("No trade of {} into {} because min number of decimals is {} for this market", volume, toCurrencyCode,
                  static_cast<int>(nbMaxDecimalsUnits));
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
          log::warn("No trade of {} into {} because {} is outside price bounds [{}, {}]", volume, toCurrencyCode, price,
                    minOrderPrice, maxOrderPrice);
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
        log::error("Unexpected currency for min order size {}", size);
      }
      if (size < currencyOrderInfo.minOrderSize && !isSimulationWithRealOrder) {
        log::warn("No trade of {} into {} because {} is lower than min order {}", volume, toCurrencyCode, size,
                  currencyOrderInfo.minOrderSize);
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
    auto orderIdIt = result.find(kOrderIdParamStr);
    if (orderIdIt == result.end()) {
      currencyInfoUpdated = true;
      if (LoadCurrencyInfoField(result, kNbDecimalsStr, currencyOrderInfo.nbDecimals,
                                currencyOrderInfo.lastNbDecimalsUpdatedTime)) {
        volume.truncate(currencyOrderInfo.nbDecimals);
        if (volume == 0) {
          log::warn("No trade of {} into {} because volume is 0 after truncating to {} decimals", volume,
                    toCurrencyCode, static_cast<int>(currencyOrderInfo.nbDecimals));
          break;
        }
        placePostData.set("units", volume.amountStr());
      } else if (LoadCurrencyInfoField(result, kMinOrderSizeJsonKeyStr, currencyOrderInfo.minOrderSize,
                                       currencyOrderInfo.lastMinOrderSizeUpdatedTime)) {
        if (!isSimulationWithRealOrder || currencyOrderInfo.minOrderSize.currencyCode() != price.currencyCode()) {
          log::warn("No trade of {} into {} because min order size is {} for this market", volume, toCurrencyCode,
                    currencyOrderInfo.minOrderSize);
          break;
        }
        volume = MonetaryAmount(currencyOrderInfo.minOrderSize / price, volume.currencyCode());
        placePostData.set("units", volume.amountStr());
      } else if (LoadCurrencyInfoField(result, kMinOrderPriceJsonKeyStr, currencyOrderInfo.minOrderPrice,
                                       currencyOrderInfo.lastMinOrderPriceUpdatedTime)) {
        if (isSimulationWithRealOrder) {
          if (!isTakerStrategy) {
            price = currencyOrderInfo.minOrderPrice;
            placePostData.set("price", price.amountStr());
          }
        } else {
          log::warn("No trade of {} into {} because min order price is {} for this market", volume, toCurrencyCode,
                    currencyOrderInfo.minOrderPrice);
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
          log::warn("No trade of {} into {} because max order price is {} for this market", volume, toCurrencyCode,
                    currencyOrderInfo.maxOrderPrice);
          break;
        }
      } else {
        log::error("Unexpected answer from {} place order, no data", _exchangePublic.name());
        break;
      }
    } else {
      placeOrderInfo.orderId = std::move(orderIdIt->get_ref<string&>());
      placeOrderInfo.orderInfo = queryOrderInfo(placeOrderInfo.orderId, tradeInfo.tradeContext);
      break;
    }
  }

  if (currencyInfoUpdated) {
    _currencyOrderInfoMap.insert_or_assign(mk.base(), std::move(currencyOrderInfo));
  }

  return placeOrderInfo;
}

OrderInfo BithumbPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId, tradeContext);
  return queryOrderInfo(orderId, tradeContext);
}

namespace {
CurlPostData OrderInfoPostData(Market mk, TradeSide side, OrderIdView orderId) {
  CurlPostData ret;
  auto baseStr = mk.base().str();
  auto quoteStr = mk.quote().str();
  ret.reserve(kOrderCurrencyParamStr.size() + kPaymentCurParamStr.size() + kTypeParamStr.size() +
              kOrderIdParamStr.size() + baseStr.size() + quoteStr.size() + orderId.size() + 10U);
  ret.append(kOrderCurrencyParamStr, baseStr);
  ret.append(kPaymentCurParamStr, quoteStr);
  ret.append(kTypeParamStr, side == TradeSide::kSell ? "ask" : "bid");
  ret.append(kOrderIdParamStr, orderId);
  return ret;
}
}  // namespace

void BithumbPrivate::cancelOrderProcess(OrderIdView orderId, const TradeContext& tradeContext) {
  PrivateQuery(_curlHandle, _apiKey, "/trade/cancel", OrderInfoPostData(tradeContext.mk, tradeContext.side, orderId));
}

OrderInfo BithumbPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  const Market mk = tradeContext.mk;
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();

  CurlPostData postData = OrderInfoPostData(mk, tradeContext.side, orderId);
  json result = PrivateQuery(_curlHandle, _apiKey, "/info/orders", postData)["data"];

  const bool isClosed = result.empty() || result.front()[kOrderIdParamStr].get<std::string_view>() != orderId;
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  if (isClosed) {
    postData.erase(kTypeParamStr);
    result = PrivateQuery(_curlHandle, _apiKey, "/info/order_detail", std::move(postData))["data"];

    for (const json& contractDetail : result["contract"]) {
      MonetaryAmount tradedVol(contractDetail["units"].get<std::string_view>(), mk.base());  // always in base currency
      MonetaryAmount price(contractDetail["price"].get<std::string_view>(), mk.quote());     // always in quote currency
      MonetaryAmount tradedCost = tradedVol.toNeutral() * price;
      CurrencyCode feeCurrency(contractDetail["fee_currency"].get<std::string_view>());
      MonetaryAmount fee(contractDetail["fee"].get<std::string_view>(), feeCurrency);

      if (fromCurrencyCode == mk.quote()) {
        orderInfo.tradedAmounts.from += tradedCost + fee;
        orderInfo.tradedAmounts.to += tradedVol;
      } else {
        orderInfo.tradedAmounts.from += tradedVol;
        orderInfo.tradedAmounts.to += tradedCost - fee;
      }
    }
  }
  return orderInfo;
}

namespace {
bool CompareTrxByDate(const json& lhs, const json& rhs) {
  int64_t lhsTs = RetrieveMicrosecondsSinceEpochFromTrxJson(lhs);
  int64_t rhsTs = RetrieveMicrosecondsSinceEpochFromTrxJson(rhs);
  return lhsTs < rhsTs;
}

CurlPostData ComputeLaunchWithdrawCurlPostData(MonetaryAmount netEmittedAmount, const Wallet& destinationWallet) {
  const CurrencyCode currencyCode = netEmittedAmount.currencyCode();
  const AccountOwner& desAccountOwner = destinationWallet.accountOwner();
  CurlPostData withdrawPostData{
      {"units", netEmittedAmount.amountStr()},
      {"currency", currencyCode.str()},
      {"address", destinationWallet.address()},
      {"exchange_name", destinationWallet.exchangeName().name()},
      {"cust_type_cd", "01"}  // "01" means individual withdraw, "02" corporate
  };

  // Bithumb checks the destination's account owner. It is required by the API, so in order to use this method you need
  // to have a Korean name.
  // coincenter can retrieve the account owner name automatically provided that the user filled the fields in the
  // destination api key part in the secrets json file.
  if (desAccountOwner.isFullyDefined()) {
    withdrawPostData.append("en_name", desAccountOwner.enName());
    withdrawPostData.append("ko_name", desAccountOwner.koName());
  } else {
    log::error("Bithumb withdrawal needs further information for destination account");
    log::error("it needs the English and Korean name of its owner so query will most probably fail");
  }
  if (destinationWallet.hasTag()) {
    withdrawPostData.append("destination", destinationWallet.tag());
  }
  return withdrawPostData;
}
}  // namespace

InitiatedWithdrawInfo BithumbPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFeeOrZero(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;

  // Unfortunately, Bithumb does not return any withdraw Id,
  // so we return a generated ID which will be matched with 'queryRecentWithdrawals'
  // We cannot generate an id based on the destination address either as it's not returned
  // by Bithumb for 'queryRecentWithdrawals'.
  // It's not ideal but better than nothing.

  // Hint : to retrieve the exact timestamp from Bithumb and because it's not returned by this endpoint,
  // We have to retrieve the withdraw from the other endpoint used by 'queryRecentWithdraws'.
  WithdrawsConstraints withdrawConstraints(currencyCode);

  json oldWithdraws = queryRecentTransactions(withdrawConstraints, UserTransactionEnum::kOngoingWithdraws);
  std::sort(oldWithdraws.begin(), oldWithdraws.end(), CompareTrxByDate);

  // Actually launch the withdraw
  PrivateQuery(_curlHandle, _apiKey, "/trade/btc_withdrawal",
               ComputeLaunchWithdrawCurlPostData(netEmittedAmount, destinationWallet));

  // Query the withdraws, hopefully we will be able to find our withdraw
  json newWithdrawTrx;
  TimeInS sleepingTime(1);
  static constexpr int kNbRetriesCatchWindow = 15;
  for (int retryPos = 0; retryPos < kNbRetriesCatchWindow && newWithdrawTrx.empty(); ++retryPos) {
    if (retryPos != 0) {
      log::warn("Cannot retrieve just launched withdraw, retry {}/{} in {} s...", retryPos, kNbRetriesCatchWindow,
                std::chrono::duration_cast<TimeInS>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
      sleepingTime = (3 * sleepingTime) / 2;
    }
    json currentWithdraws = queryRecentTransactions(withdrawConstraints, UserTransactionEnum::kOngoingWithdraws);
    std::sort(currentWithdraws.begin(), currentWithdraws.end(), CompareTrxByDate);

    // Isolate the new withdraws since the launch of our new withdraw
    json newWithdraws;
    std::set_difference(currentWithdraws.begin(), currentWithdraws.end(), oldWithdraws.begin(), oldWithdraws.end(),
                        std::back_inserter(newWithdraws), CompareTrxByDate);

    log::debug("Isolated {} new withdraws, one of them is probably the one just launched", newWithdraws.size());

    for (json& withdrawTrx : newWithdraws) {
      MonetaryAmount withdrawNetEmittedAmount = RetrieveNetEmittedAmountFromTrxJson(withdrawTrx);
      if (withdrawNetEmittedAmount.isCloseTo(netEmittedAmount, 0.001)) {
        log::debug("Found new withdraw {}", withdrawTrx.dump());
        newWithdrawTrx = std::move(withdrawTrx);
        break;
      }
      log::debug("Withdraw {}'s is too different from our amount {}", withdrawTrx.dump(), netEmittedAmount);
    }
  }

  if (newWithdrawTrx.empty()) {
    throw exception(
        "Unable to retrieve just launch withdraw from Bithumb - it may be pending verification, please check your "
        "emails");
  }

  return {std::move(destinationWallet), GenerateWithdrawIdFromTrx(netEmittedAmount, newWithdrawTrx), grossAmount};
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
