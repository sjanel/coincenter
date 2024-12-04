#include "bithumbprivateapi.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "accountowner.hpp"
#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "base64.hpp"
#include "bithumb-schema.hpp"
#include "bithumbpublicapi.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "closed-order.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "durationstring.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ndigits.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "read-json.hpp"
#include "request-retry.hpp"
#include "runmodes.hpp"
#include "ssl_sha.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeoptions.hpp"
#include "tradeside.hpp"
#include "utf8.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"
#include "write-json.hpp"

namespace cct::api {
namespace {

// Bithumb API parameter constants
constexpr std::string_view kOrderCurrencyParamStr = "order_currency";
constexpr std::string_view kPaymentCurParamStr = "payment_currency";
constexpr std::string_view kOrderIdParamStr = "order_id";
constexpr std::string_view kTypeParamStr = "type";

constexpr int kBadRequestErrorCode = 5100;
constexpr int kOrderRelatedErrorCode = 5600;

constexpr std::string_view kWalletAddressEndpointStr = "/info/wallet_address";

auto GetStrData(std::string_view endpoint, std::string_view postDataStr) {
  Nonce nonce = Nonce_TimeSinceEpochInMs();

  static constexpr char kParChar = 1;
  string strData(endpoint.size() + 2U + postDataStr.size() + nonce.size(), kParChar);

  auto it = std::ranges::copy(endpoint, strData.begin()).out;
  it = std::ranges::copy(postDataStr, it + 1).out;
  it = std::ranges::copy(nonce, it + 1).out;
  return std::make_pair(std::move(strData), std::move(nonce));
}

void SetHttpHeaders(CurlOptions& opts, const APIKey& apiKey, const auto& signature, const Nonce& nonce) {
  static constexpr std::string_view kApiKey = "API-Key";
  static constexpr std::string_view kApiSign = "API-Sign";
  static constexpr std::string_view kApiNonce = "API-Nonce";
  static constexpr std::string_view kApiClientType = "api-client-type";

  static constexpr auto kApiClientTypeValue = 1;

  static constexpr std::size_t kNbHeaders = 4;

  static constexpr auto kFixedSizePart = kApiKey.size() + kApiSign.size() + kApiNonce.size() + kApiClientType.size() +
                                         ndigits(kApiClientTypeValue) + (kNbHeaders * 2) - 1U;

  auto& httpHeaders = opts.mutableHttpHeaders();
  httpHeaders.clear();
  httpHeaders.underlyingBufferReserve(kFixedSizePart + apiKey.key().size() + signature.size() + nonce.size());

  httpHeaders.emplace_back(kApiKey, apiKey.key());
  httpHeaders.emplace_back(kApiSign, signature);
  httpHeaders.emplace_back(kApiNonce, nonce);
  httpHeaders.emplace_back(kApiClientType, kApiClientTypeValue);
}

bool LoadCurrencyInfoField(const auto& extraField, auto& valTs) {
  using ValueType = std::remove_cvref_t<decltype(valTs.val)>;

  if (extraField.ts.ts == decltype(extraField.ts.ts){}) {
    return false;
  }

  if constexpr (std::is_same_v<ValueType, MonetaryAmount>) {
    valTs.val = MonetaryAmount(extraField.val);
  } else {
    valTs.val = extraField.val;
  }
  valTs.ts = extraField.ts;
  return true;
}

template <class T>
bool ExtractError(std::string_view findStr1, std::string_view findStr2, std::string_view logStr, std::string_view msg,
                  T& extraField) {
  std::size_t startPos = msg.find(findStr1);

  if (startPos != std::string_view::npos) {
    std::size_t idxFirst = startPos + findStr1.size();
    std::size_t endPos = msg.find(findStr2, idxFirst);
    if (endPos == std::string_view::npos) {
      return false;
    }
    std::string_view valueStr(msg.begin() + idxFirst, msg.begin() + endPos);
    // I did not find the way via the API to get various important information for some currency bound values,
    // so I get them this way, by parsing the Korean error message of the response
    log::warn("Bithumb told us that {} is {}", logStr, valueStr);

    if constexpr (std::is_same_v<schema::bithumb::Trade::ExtraStringField, T>) {
      extraField.val = string(valueStr);
    } else if constexpr (std::is_same_v<schema::bithumb::Trade::ExtraIntField, T>) {
      extraField.val = StringToIntegral<decltype(extraField.val)>(valueStr);
    } else {
      static_assert(std::is_same_v<schema::bithumb::Trade::ExtraStringField, T> ||
                    std::is_same_v<schema::bithumb::Trade::ExtraIntField, T>);
    }

    extraField.ts.ts = Clock::now();

    return true;
  }
  return false;
}

void CheckAndLogSynchronizedTime(std::string_view msg) {
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

        int64_t reqTimeInt = StringToIntegral<int64_t>(reqTimeStr);
        int64_t nowTimeInt = StringToIntegral<int64_t>(nowTimeStr);

        log::error("Bithumb time is not synchronized with us (difference of {} s)", (reqTimeInt - nowTimeInt) / 1000);
        log::error("It can sometimes come from a Bithumb bug, retry");
      }
    }
  }
}

template <class T>
bool CheckOrderErrors(std::string_view endpoint, std::string_view msg, T& data) {
  const bool isInfoOpenedOrders = endpoint == "/info/orders";
  const bool isCancelQuery = endpoint == "/trade/cancel";
  const bool isDepositInfo = endpoint == kWalletAddressEndpointStr;

  if constexpr (std::is_same_v<T, schema::bithumb::Trade>) {
    auto& extraData = data.extra_data;
    if (ExtractError("수량은 소수점 ", "자", "number of decimals", msg, extraData.nbDecimals) ||
        ExtractError("주문금액은 ", " 입니다", "min order size", msg, extraData.minOrderSize) ||
        ExtractError("주문 가격은 ", " 이상으로 입력 가능합니다", "min order price", msg, extraData.minOrderSize) ||
        ExtractError("주문 가격은 ", " 이하로 입력 가능합니다", "max order price", msg, extraData.maxOrderPrice)) {
      return true;
    }
  }
  if ((isInfoOpenedOrders || isCancelQuery) &&
      msg.find("거래 진행중인 내역이 존재하지 않습니다") != std::string_view::npos) {
    // This is not really an error, it means that order has been eaten or cancelled.
    // Just return empty object in this case
    log::info("Considering Bithumb order as closed as no data received from them");
    data = T();
    return true;
  }
  if (isDepositInfo && msg.find("잘못된 요청 입니다. 요청 정보를 확인 하세요.") != std::string_view::npos) {
    // This means that address is not generated for asked currency. It's useless to retry in these cases.
    data = T();
    return true;
  }
  return false;
}

template <class T>
T PrivateQueryProcessWithRetries(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint,
                                 CurlOptions&& opts) {
  RequestRetry requestRetry(curlHandle, std::move(opts));

  return requestRetry.query<T, json::opts{.error_on_unknown_keys = false, .minified = true, .raw_string = true}>(
      endpoint,
      [endpoint](T& jsonResponse) {
        auto statusCode = StringToIntegral<int64_t>(jsonResponse.status);
        if (statusCode == 0 || statusCode == BithumbPublic::kStatusNotPresentError) {
          return RequestRetry::Status::kResponseOK;
        }

        decode_utf8(jsonResponse.message);
        std::string_view msg = jsonResponse.message;

        switch (statusCode) {
          case kBadRequestErrorCode:
            CheckAndLogSynchronizedTime(msg);
            break;
          case kOrderRelatedErrorCode:
            if (CheckOrderErrors(endpoint, msg, jsonResponse)) {
              return RequestRetry::Status::kResponseOK;
            }
            break;
          default:
            break;
        }

        return RequestRetry::Status::kResponseError;
      },
      [endpoint, &apiKey](CurlOptions& curlOptions) {
        auto [strData, nonce] = GetStrData(endpoint, curlOptions.postData().str());
        auto signature = B64Encode(ssl::Sha512Hex(strData, apiKey.privateKey()));

        SetHttpHeaders(curlOptions, apiKey, signature, nonce);
      });
}

template <class T, class CurlPostDataT = CurlPostData>
T PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view endpoint,
               CurlPostDataT&& curlPostData = CurlPostData()) {
  CurlPostData postData(std::forward<CurlPostDataT>(curlPostData));
  postData.emplace_front("endpoint", endpoint);

  CurlOptions opts(HttpRequestType::kPost, postData.urlEncodeExceptDelimiters());

  return PrivateQueryProcessWithRetries<T>(curlHandle, apiKey, endpoint, std::move(opts));
}

File GetBithumbCurrencyInfoMapCache(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "bithumbcurrencyinfocache.json", File::IfError::kNoThrow};
}

}  // namespace

BithumbPrivate::BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey)
    : ExchangePrivate(config, bithumbPublic, apiKey),
      _curlHandle(BithumbPublic::kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  config.getRunMode()),
      _currencyOrderInfoRefreshTime(exchangeConfig().query.updateFrequency.at(QueryType::currencyInfo).duration),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::depositWallet).duration,
                              _cachedResultVault),
          _curlHandle, _apiKey, bithumbPublic) {
  if (config.getRunMode() != settings::RunMode::kQueryResponseOverriden) {
    ReadExactJsonOrThrow(GetBithumbCurrencyInfoMapCache(_coincenterInfo.dataDir()).readAll(), _currencyOrderInfoMap);
  }
}

bool BithumbPrivate::validateApiKey() {
  const auto data = PrivateQuery<schema::bithumb::InfoBalance>(_curlHandle, _apiKey, "/info/balance", CurlPostData());
  if (data.status.empty()) {
    log::error("Unexpected Bithumb reply from balance");
    return false;
  }
  const auto statusCode = StringToIntegral<int64_t>(data.status);
  log::info("{} status code: {}", exchangeName(), statusCode);
  return statusCode == BithumbPublic::kStatusOK;
}

BalancePortfolio BithumbPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  const auto result =
      PrivateQuery<schema::bithumb::InfoBalance>(_curlHandle, _apiKey, "/info/balance", {{"currency", "all"}});

  BalancePortfolio balancePortfolio;

  static constexpr std::array<std::string_view, 2> kKnownPrefixes = {"available_", "in_use_"};
  auto endPrefixIt = kKnownPrefixes.end();
  if (balanceOptions.amountIncludePolicy() != BalanceOptions::AmountIncludePolicy::kWithBalanceInUse) {
    --endPrefixIt;
  }

  for (const auto& [key, value] : result.data) {
    for (auto prefixIt = kKnownPrefixes.begin(); prefixIt != endPrefixIt; ++prefixIt) {
      if (key.starts_with(*prefixIt)) {
        std::string_view curStr(key.begin() + prefixIt->size(), key.end());
        MonetaryAmount amount(value, _coincenterInfo.standardizeCurrencyCode(curStr));

        balancePortfolio += amount;
        break;
      }
    }
  }
  return balancePortfolio;
}

Wallet BithumbPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  const auto ret = PrivateQuery<schema::bithumb::InfoWalletAddress>(_curlHandle, _apiKey, kWalletAddressEndpointStr,
                                                                    {{"currency", currencyCode.str()}});
  std::string_view addressAndTag = ret.data.wallet_address;
  if (addressAndTag.empty()) {
    throw exception(
        "Bithumb wallet is not created for {}, it should be done with the UI first (no way to do it via API)",
        currencyCode);
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
  bool doCheckWallet =
      coincenterInfo.exchangeConfig(_exchangePublic.exchangeNameEnum()).withdraw.validateDepositAddressesInFile;
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet wallet(ExchangeName(_exchangePublic.exchangeNameEnum(), _apiKey.name()), currencyCode, string(address), tag,
                walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {

TimePoint RetrieveTimePointFromTrxJson(const std::variant<string, int64_t>& date) {
  // In the official documentation, transfer_date field is an integer.
  // But in fact (as of 2022) it's a string representation of the integer timestamp.
  // Let's support both types to be safe.
  // Bithumb returns a UTC based timestamp.

  int64_t microsecondsSinceEpoch;
  std::visit(
      [&microsecondsSinceEpoch](auto&& field) {
        using T = std::decay_t<decltype(field)>;
        if constexpr (std::is_same_v<T, string>) {
          microsecondsSinceEpoch = StringToIntegral<int64_t>(field);
        } else if constexpr (std::is_same_v<T, int64_t>) {
          microsecondsSinceEpoch = field;
        } else {
          throw exception("Cannot understand '{}' parameter type", field);
        }
      },
      date);

  return TimePoint{microseconds(microsecondsSinceEpoch)};
}

auto FillOrderCurrencies(const OrdersConstraints& ordersConstraints, ExchangePublic& exchangePublic,
                         CurlHandle& curlHandle, const APIKey& apiKey, std::string_view prefixKeyBalance,
                         CurlPostData& params) {
  SmallVector<CurrencyCode, 1> orderCurrencies;

  if (ordersConstraints.isCurDefined()) {
    MarketSet markets;
    Market filterMarket =
        exchangePublic.determineMarketFromFilterCurrencies(markets, ordersConstraints.cur1(), ordersConstraints.cur2());

    if (!filterMarket.base().isNeutral()) {
      orderCurrencies.push_back(filterMarket.base());
      if (!filterMarket.quote().isNeutral()) {
        params.emplace_back(kPaymentCurParamStr, filterMarket.quote().str());
      }
    }
  } else {
    // Trick: let's use balance query to guess where we can search for orders,
    // by looking at "is_use" amounts to retrieve opened orders or "available" amounts to retrieve closed orders.
    // The only drawback is that we need to make one query for each currency, but it's better than nothing.
    const auto balance =
        PrivateQuery<schema::bithumb::InfoBalance>(curlHandle, apiKey, "/info/balance", {{"currency", "all"}});
    for (const auto& [key, value] : balance.data) {
      if (key.starts_with(prefixKeyBalance)) {
        CurrencyCode cur(std::string_view(key.begin() + prefixKeyBalance.size(), key.end()));
        if (cur != "KRW" && value != 0) {
          orderCurrencies.push_back(cur);
        }
      }
    }
  }
  return orderCurrencies;
}

template <class OrderVectorType>
OrderVectorType QueryOrders(const OrdersConstraints& ordersConstraints, ExchangePublic& exchangePublic,
                            CurlHandle& curlHandle, const APIKey& apiKey) {
  static constexpr int kNbOrdersMaxPerQuery = 1000;

  CurlPostData params{{"count", kNbOrdersMaxPerQuery}};

  using OrderType = std::remove_cvref_t<decltype(*std::declval<OrderVectorType>().begin())>;

  static constexpr std::string_view kPrefixKey = std::is_same_v<OrderType, ClosedOrder> ? "available_" : "in_use_";

  const auto orderCurrencies =
      FillOrderCurrencies(ordersConstraints, exchangePublic, curlHandle, apiKey, kPrefixKey, params);

  OrderVectorType orders;
  if (ordersConstraints.isPlacedTimeAfterDefined()) {
    params.emplace_back("after", TimestampToMillisecondsSinceEpoch(ordersConstraints.placedAfter()));
  }
  if (orderCurrencies.size() > 1) {
    if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
      // for closed orders without market in input, we will use the other endpoint 'transactions'
      // as these orders are stored for short term time only.
      return orders;
    }
    log::info("Will make {} order requests", orderCurrencies.size());
  }
  for (CurrencyCode volumeCur : orderCurrencies) {
    params.set(kOrderCurrencyParamStr, volumeCur.str());

    const auto ordersReply = PrivateQuery<schema::bithumb::InfoOrders>(curlHandle, apiKey, "/info/orders", params);

    for (auto& orderDetails : ordersReply.data) {
      TimePoint placedTime = RetrieveTimePointFromTrxJson(orderDetails.order_date);
      if (!ordersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      string id = std::move(orderDetails.order_id);
      if (!ordersConstraints.validateId(id)) {
        continue;
      }

      CurrencyCode priceCur = orderDetails.payment_currency;
      MonetaryAmount originalVolume(orderDetails.units, volumeCur);
      MonetaryAmount remainingVolume(orderDetails.units_remaining, volumeCur);
      MonetaryAmount matchedVolume = originalVolume - remainingVolume;
      MonetaryAmount price(orderDetails.price, priceCur);
      TradeSide side =
          orderDetails.type == schema::bithumb::TransactionTypeEnum::bid ? TradeSide::kBuy : TradeSide::kSell;

      if constexpr (std::is_same_v<OrderType, OpenedOrder>) {
        if (remainingVolume == 0) {
          // then it's not an opened order
          continue;
        }

        orders.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
      } else if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
        if (remainingVolume != 0) {
          // then it's not a closed order
          continue;
        }

        orders.emplace_back(std::move(id), matchedVolume, price, placedTime, placedTime, side);
      } else {
        // Note: below ugly template lambda can be replaced with 'static_assert(false);' in C++23
        []<bool flag = false>() { static_assert(flag, "no match"); }();
      }
    }
  }
  std::ranges::sort(orders);
  log::info("Retrieved {} orders from {}", orders.size(), exchangePublic.name());
  return orders;
}

constexpr int kSearchGbAll = 0;
constexpr int kSearchGbBuy = 1;
constexpr int kSearchGbSell = 2;
constexpr int kSearchGbOnGoingWithdrawals = 3;
constexpr int kSearchGbDeposit = 4;
constexpr int kSearchGbProcessedWithdrawals = 5;
constexpr int kSearchGbKRWDeposits = 9;

string GenerateDepositIdFromTrx(TimePoint timestamp, const schema::bithumb::UserTransactions::UserTransaction& trx) {
  // Bithumb does not provide any transaction id, let's generate it from currency and timestamp...
  string id;
  trx.order_currency.appendStrTo(id);
  id.push_back('-');
  AppendIntegralToString(id, TimestampToMillisecondsSinceEpoch(timestamp));
  return id;
}

string GenerateWithdrawIdFromTrx(MonetaryAmount netEmittedAmount,
                                 const schema::bithumb::UserTransactions::UserTransaction& trx) {
  // We cannot use the timestamp for the withdraw ID because it changes where is switches from the state 'withdrawing'
  // to the state 'withdraw done' (searchGb 3->5)
  // There are two fields that does not seem to change over time, and we are going to use them to generate our id:
  //  - order_balance
  //  - payment_balance
  string withdrawId = netEmittedAmount.str();
  withdrawId.push_back(';');
  trx.order_balance.appendStrTo(withdrawId);
  withdrawId.push_back(';');
  trx.payment_balance.appendStrTo(withdrawId);
  return B64Encode(withdrawId);
}

MonetaryAmount RetrieveAmountFromTrxJson(const schema::bithumb::UserTransactions::UserTransaction& trx) {
  // starts with "+ " for a deposit, "- " for a withdraw, return absolute
  CurrencyCode currencyCode = trx.order_currency;
  MonetaryAmount amount{trx.units, currencyCode};
  if (amount == 0) {
    // It's strange but for 'KRW' withdraws, Bithumb returns the KRW amount in the 'price' field (as negative!)
    amount = MonetaryAmount{trx.price, currencyCode};
  }
  return amount.abs();
}

MonetaryAmount RetrieveFeeFromTrxJson(const schema::bithumb::UserTransactions::UserTransaction& trx) {
  return {trx.fee, trx.fee_currency};
}

MonetaryAmount RetrieveNetEmittedAmountFromTrxJson(const schema::bithumb::UserTransactions::UserTransaction& trx) {
  MonetaryAmount grossEmittedAmount = RetrieveAmountFromTrxJson(trx);
  MonetaryAmount fee = RetrieveFeeFromTrxJson(trx);
  return grossEmittedAmount - fee;
}

Withdraw::Status RetrieveWithdrawStatusFromTrxJson(const schema::bithumb::UserTransactions::UserTransaction& trx) {
  const auto searchGb = StringToIntegral(trx.search);

  return searchGb == kSearchGbOnGoingWithdrawals ? Withdraw::Status::kProcessing : Withdraw::Status::kSuccess;
}

enum class UserTransactionEnum : int8_t {
  kClosedOrders,
  kDeposit,
  kOngoingWithdraws,
  kProcessedWithdraws,
  kAllWithdraws
};

template <class ConstraintsType>
auto QueryUserTransactions(BithumbPrivate& exchangePrivate, CurlHandle& curlHandle, const APIKey& apiKey,
                           const ConstraintsType& constraints, UserTransactionEnum userTransactionEnum) {
  SmallVector<CurrencyCode, 1> orderCurrencies;

  if (constraints.isCurDefined()) {
    orderCurrencies.push_back(constraints.currencyCode());
  } else {
    log::warn("Retrieval of recent user transactions should be done currency by currency for {:e}",
              exchangePrivate.exchangeName());
    log::warn("Heuristic: only query for currencies which are present in the balance");
    for (const auto& amountWithEquivalent : exchangePrivate.queryAccountBalance()) {
      CurrencyCode currencyCode = amountWithEquivalent.amount.currencyCode();
      orderCurrencies.push_back(currencyCode);
    }
  }

  CurlPostData options{{"count", 50}};

  if (userTransactionEnum == UserTransactionEnum::kClosedOrders) {
    if constexpr (std::is_same_v<ConstraintsType, OrdersConstraints>) {
      if (constraints.isCur2Defined()) {
        options.emplace_back(kPaymentCurParamStr, constraints.curStr2());
      } else {
        options.emplace_back(kPaymentCurParamStr, "KRW");
      }
    }
  } else {
    // It's not clear what the payment currency option is for user_transactions endpoint for deposits and withdraws.
    // For withdraws it seems to have no impact, and even worse, it returns weird output when
    // querying withdraws for a specific coin, it can return KRW withdraws to user bank account.
    options.emplace_back(kPaymentCurParamStr, "BTC");
  }

  vector<schema::bithumb::UserTransactions::UserTransaction> allResults;
  FixedCapacityVector<int, 2> searchGbsVector;
  switch (userTransactionEnum) {
    case UserTransactionEnum::kClosedOrders:
      searchGbsVector.push_back(kSearchGbAll);  // We will get all transactions, but we will filter on buys and sells
      break;
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
      throw exception("Unexpected deposit or withdraw enum value {}", static_cast<int>(userTransactionEnum));
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
    if (userTransactionEnum == UserTransactionEnum::kDeposit) {
      if (currencyCode == "KRW") {
        searchGbsVector.resize(2, kSearchGbKRWDeposits);
      } else {
        searchGbsVector.resize(1, kSearchGbDeposit);
      }
    }

    for (int searchGb : searchGbsVector) {
      options.set("searchGb", searchGb);
      auto userTransactionsReply =
          PrivateQuery<schema::bithumb::UserTransactions>(curlHandle, apiKey, "/info/user_transactions", options);

      for (auto& trx : userTransactionsReply.data) {
        if (!constraints.validateCur(trx.order_currency)) {
          continue;
        }

        const TimePoint timestamp = RetrieveTimePointFromTrxJson(trx.transfer_date);
        if (!constraints.validateTime(timestamp)) {
          continue;
        }

        if (userTransactionEnum == UserTransactionEnum::kDeposit) {
          if (!constraints.validateId(GenerateDepositIdFromTrx(timestamp, trx))) {
            continue;
          }
        } else if (userTransactionEnum != UserTransactionEnum::kClosedOrders) {
          MonetaryAmount netEmittedAmount = RetrieveNetEmittedAmountFromTrxJson(trx);
          if (!constraints.validateId(GenerateWithdrawIdFromTrx(netEmittedAmount, trx))) {
            continue;
          }
        } else if (userTransactionEnum == UserTransactionEnum::kClosedOrders) {
          const auto searchValue = StringToIntegral<int>(trx.search);
          if (searchValue != kSearchGbBuy && searchValue != kSearchGbSell) {
            continue;
          }
        }

        allResults.push_back(std::move(trx));
      }
    }
  }
  return allResults;
}

}  // namespace

ClosedOrderVector BithumbPrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  auto closedOrders = QueryOrders<ClosedOrderVector>(closedOrdersConstraints, _exchangePublic, _curlHandle, _apiKey);

  const auto orderTransactionsJson =
      QueryUserTransactions(*this, _curlHandle, _apiKey, closedOrdersConstraints, UserTransactionEnum::kClosedOrders);

  closedOrders.reserve(closedOrders.size() + orderTransactionsJson.size());
  for (const schema::bithumb::UserTransactions::UserTransaction& trx : orderTransactionsJson) {
    const TimePoint timestamp = RetrieveTimePointFromTrxJson(trx.transfer_date);

    string id = GenerateDepositIdFromTrx(timestamp, trx);

    MonetaryAmount matchedVolume(trx.units, trx.order_currency);
    MonetaryAmount price(trx.price, trx.payment_currency);

    const auto searchGb = StringToIntegral(trx.search);

    TradeSide side = searchGb == kSearchGbBuy ? TradeSide::kBuy : TradeSide::kSell;

    closedOrders.emplace_back(std::move(id), matchedVolume, price, timestamp, timestamp, side);
  }
  std::ranges::sort(closedOrders);
  return closedOrders;
}

OpenedOrderVector BithumbPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  return QueryOrders<OpenedOrderVector>(openedOrdersConstraints, _exchangePublic, _curlHandle, _apiKey);
}

int BithumbPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  // No faster way to cancel several orders at once with Bithumb, doing a simple for loop
  OpenedOrderVector orders = queryOpenedOrders(openedOrdersConstraints);
  for (const OpenedOrder& order : orders) {
    TradeContext tradeContext(order.market(), order.side());
    cancelOrderProcess(order.id(), tradeContext);
  }
  return orders.size();
}

DepositsSet BithumbPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;

  auto txrList = QueryUserTransactions(*this, _curlHandle, _apiKey, depositsConstraints, UserTransactionEnum::kDeposit);
  deposits.reserve(txrList.size());
  for (const schema::bithumb::UserTransactions::UserTransaction& trx : txrList) {
    const TimePoint timestamp = RetrieveTimePointFromTrxJson(trx.transfer_date);

    string id = GenerateDepositIdFromTrx(timestamp, trx);

    // No status information returned by Bithumb. Defaulting to Success
    deposits.emplace_back(std::move(id), timestamp, RetrieveAmountFromTrxJson(trx), Deposit::Status::kSuccess);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

WithdrawsSet BithumbPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;

  auto txrList =
      QueryUserTransactions(*this, _curlHandle, _apiKey, withdrawsConstraints, UserTransactionEnum::kAllWithdraws);
  withdraws.reserve(txrList.size());
  for (const schema::bithumb::UserTransactions::UserTransaction& trx : txrList) {
    const TimePoint timestamp = RetrieveTimePointFromTrxJson(trx.transfer_date);

    const MonetaryAmount fee = RetrieveFeeFromTrxJson(trx);
    const MonetaryAmount netEmittedAmount = RetrieveNetEmittedAmountFromTrxJson(trx);

    withdraws.emplace_back(GenerateWithdrawIdFromTrx(netEmittedAmount, trx), timestamp, netEmittedAmount,
                           RetrieveWithdrawStatusFromTrxJson(trx), fee);
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

PlaceOrderInfo BithumbPrivate::placeOrder(MonetaryAmount /*from*/, MonetaryAmount volume, MonetaryAmount price,
                                          const TradeInfo& tradeInfo) {
  const bool placeSimulatedRealOrder = _exchangePublic.exchangeConfig().query.placeSimulateRealOrder;
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  const Market mk = tradeInfo.tradeContext.market;

  // It seems Bithumb uses "standard" currency codes, no need to translate them
  CurlPostData placePostData{{kOrderCurrencyParamStr, mk.base().str()}, {kPaymentCurParamStr, mk.quote().str()}};
  const std::string_view orderType = fromCurrencyCode == mk.base() ? "ask" : "bid";

  string endpoint("/trade/");
  if (isTakerStrategy) {
    endpoint.append(fromCurrencyCode == mk.base() ? "market_sell" : "market_buy");
  } else {
    endpoint.append("place");
    placePostData.emplace_back(kTypeParamStr, orderType);
    placePostData.emplace_back("price", price.amountStr());
  }

  // Volume is gross amount if from amount is in quote currency, we should remove the fees
  if (fromCurrencyCode == mk.quote()) {
    const auto feeType = isTakerStrategy ? schema::ExchangeTradeFeesConfig::FeeType::Taker
                                         : schema::ExchangeTradeFeesConfig::FeeType::Maker;
    const auto& exchangeConfig = _coincenterInfo.exchangeConfig(_exchangePublic.exchangeNameEnum());
    volume = exchangeConfig.tradeFees.applyFee(volume, feeType);
  }

  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;

  auto currencyOrderInfoIt = _currencyOrderInfoMap.find(mk.base());
  auto nowTime = Clock::now();
  CurrencyOrderInfo currencyOrderInfo;
  if (currencyOrderInfoIt != _currencyOrderInfoMap.end()) {
    currencyOrderInfo = currencyOrderInfoIt->second;
    if (currencyOrderInfo.nbDecimals.ts.ts + _currencyOrderInfoRefreshTime > nowTime) {
      int8_t nbMaxDecimalsUnits = currencyOrderInfo.nbDecimals.val;
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
      if (currencyOrderInfo.minOrderPrice.ts.ts + _currencyOrderInfoRefreshTime > nowTime &&
          currencyOrderInfo.minOrderPrice.val.currencyCode() == price.currencyCode()) {
        minOrderPrice = currencyOrderInfo.minOrderPrice.val;
      }
      if (currencyOrderInfo.maxOrderPrice.ts.ts + _currencyOrderInfoRefreshTime > nowTime &&
          currencyOrderInfo.maxOrderPrice.val.currencyCode() == price.currencyCode()) {
        maxOrderPrice = currencyOrderInfo.maxOrderPrice.val;
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

    if (currencyOrderInfo.minOrderSize.ts.ts + _currencyOrderInfoRefreshTime > nowTime) {
      MonetaryAmount size = currencyOrderInfo.minOrderSize.val;
      CurrencyCode minOrderSizeCur = size.currencyCode();
      if (volume.currencyCode() == minOrderSizeCur) {
        size = volume;
      } else if (price.currencyCode() == minOrderSizeCur) {
        size = volume.toNeutral() * price;
      } else {
        log::error("Unexpected currency for min order size {}", size);
      }
      if (size < currencyOrderInfo.minOrderSize.val && !isSimulationWithRealOrder) {
        log::warn("No trade of {} into {} because {} is lower than min order {}", volume, toCurrencyCode, size,
                  currencyOrderInfo.minOrderSize.val);
        placeOrderInfo.setClosed();
        return placeOrderInfo;
      }
    }
  }

  placePostData.emplace_back("units", volume.amountStr());

  placeOrderInfo.setClosed();

  static constexpr int kNbMaxRetries = 3;
  bool currencyInfoUpdated = false;
  for (int nbRetries = 0; nbRetries < kNbMaxRetries; ++nbRetries) {
    auto tradeReply = PrivateQuery<schema::bithumb::Trade>(_curlHandle, _apiKey, endpoint, placePostData);
    if (!tradeReply.order_id.empty()) {
      placeOrderInfo.orderId = std::move(tradeReply.order_id);
      placeOrderInfo.orderInfo = queryOrderInfo(placeOrderInfo.orderId, tradeInfo.tradeContext);
      break;
    }

    currencyInfoUpdated = true;
    if (LoadCurrencyInfoField(tradeReply.extra_data.nbDecimals, currencyOrderInfo.nbDecimals)) {
      volume.truncate(currencyOrderInfo.nbDecimals.val);
      if (volume == 0) {
        log::warn("No trade of {} into {} because volume is 0 after truncating to {} decimals", volume, toCurrencyCode,
                  static_cast<int>(currencyOrderInfo.nbDecimals.val));
        break;
      }
      placePostData.set("units", volume.amountStr());
    } else if (LoadCurrencyInfoField(tradeReply.extra_data.minOrderSize, currencyOrderInfo.minOrderSize)) {
      if (!isSimulationWithRealOrder || currencyOrderInfo.minOrderSize.val.currencyCode() != price.currencyCode()) {
        log::warn("No trade of {} into {} because min order size is {} for this market", volume, toCurrencyCode,
                  currencyOrderInfo.minOrderSize.val);
        break;
      }
      volume = MonetaryAmount(currencyOrderInfo.minOrderSize.val / price, volume.currencyCode());
      placePostData.set("units", volume.amountStr());
    } else if (LoadCurrencyInfoField(tradeReply.extra_data.minOrderPrice, currencyOrderInfo.minOrderPrice)) {
      if (isSimulationWithRealOrder) {
        if (!isTakerStrategy) {
          price = currencyOrderInfo.minOrderPrice.val;
          placePostData.set("price", price.amountStr());
        }
      } else {
        log::warn("No trade of {} into {} because min order price is {} for this market", volume, toCurrencyCode,
                  currencyOrderInfo.minOrderPrice.val);
        break;
      }
    } else if (LoadCurrencyInfoField(tradeReply.extra_data.maxOrderPrice, currencyOrderInfo.maxOrderPrice)) {
      if (isSimulationWithRealOrder) {
        if (!isTakerStrategy) {
          price = currencyOrderInfo.maxOrderPrice.val;
          placePostData.set("price", price.amountStr());
        }
      } else {
        log::warn("No trade of {} into {} because max order price is {} for this market", volume, toCurrencyCode,
                  currencyOrderInfo.maxOrderPrice.val);
        break;
      }
    } else {
      log::error("Unexpected answer from {} place order, no data", _exchangePublic.name());
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

  ret.underlyingBufferReserve(kOrderCurrencyParamStr.size() + kPaymentCurParamStr.size() + kTypeParamStr.size() +
                              kOrderIdParamStr.size() + baseStr.size() + quoteStr.size() + orderId.size() + 10U);

  ret.emplace_back(kOrderCurrencyParamStr, baseStr);
  ret.emplace_back(kPaymentCurParamStr, quoteStr);
  ret.emplace_back(kTypeParamStr, side == TradeSide::kSell ? "ask" : "bid");
  ret.emplace_back(kOrderIdParamStr, orderId);

  return ret;
}
}  // namespace

void BithumbPrivate::cancelOrderProcess(OrderIdView orderId, const TradeContext& tradeContext) {
  PrivateQuery<schema::bithumb::TradeCancel>(_curlHandle, _apiKey, "/trade/cancel",
                                             OrderInfoPostData(tradeContext.market, tradeContext.side, orderId));
}

OrderInfo BithumbPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  const Market mk = tradeContext.market;
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();

  CurlPostData postData = OrderInfoPostData(mk, tradeContext.side, orderId);
  auto ordersReply = PrivateQuery<schema::bithumb::InfoOrders>(_curlHandle, _apiKey, "/info/orders", postData);

  const bool isClosed = ordersReply.data.empty() || ordersReply.data.front().order_id != orderId;
  OrderInfo orderInfo{TradedAmounts(fromCurrencyCode, toCurrencyCode), isClosed};
  if (!isClosed) {
    return orderInfo;
  }

  postData.erase(kTypeParamStr);
  auto infoOrderDetailReply =
      PrivateQuery<schema::bithumb::InfoOrderDetail>(_curlHandle, _apiKey, "/info/order_detail", std::move(postData));

  for (const auto& contractDetail : infoOrderDetailReply.data.contract) {
    // always in base currency
    MonetaryAmount tradedVol(contractDetail.units, mk.base());
    // always in quote currency
    MonetaryAmount price(contractDetail.price, mk.quote());
    MonetaryAmount tradedCost = tradedVol.toNeutral() * price;
    MonetaryAmount fee(contractDetail.fee, contractDetail.fee_currency);

    if (fromCurrencyCode == mk.quote()) {
      orderInfo.tradedAmounts.from += tradedCost + fee;
      orderInfo.tradedAmounts.to += tradedVol;
    } else {
      orderInfo.tradedAmounts.from += tradedVol;
      orderInfo.tradedAmounts.to += tradedCost - fee;
    }
  }

  return orderInfo;
}

namespace {
bool CompareTrxByDate(const schema::bithumb::UserTransactions::UserTransaction& lhs,
                      const schema::bithumb::UserTransactions::UserTransaction& rhs) {
  const auto lhsTs = RetrieveTimePointFromTrxJson(lhs.transfer_date);
  const auto rhsTs = RetrieveTimePointFromTrxJson(rhs.transfer_date);
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
    withdrawPostData.emplace_back("en_name", desAccountOwner.enName());
    withdrawPostData.emplace_back("ko_name", desAccountOwner.koName());
  } else {
    log::error("Bithumb withdrawal needs further information for destination account");
    log::error("it needs the English and Korean name of its owner so query will most probably fail");
  }
  if (destinationWallet.hasTag()) {
    withdrawPostData.emplace_back("destination", destinationWallet.tag());
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

  auto oldWithdraws =
      QueryUserTransactions(*this, _curlHandle, _apiKey, withdrawConstraints, UserTransactionEnum::kOngoingWithdraws);
  std::ranges::sort(oldWithdraws, CompareTrxByDate);

  // Actually launch the withdraw
  PrivateQuery<schema::bithumb::BtcWithdrawal>(_curlHandle, _apiKey, "/trade/btc_withdrawal",
                                               ComputeLaunchWithdrawCurlPostData(netEmittedAmount, destinationWallet));

  // Query the withdraws, hopefully we will be able to find our withdraw
  std::optional<schema::bithumb::UserTransactions::UserTransaction> newWithdrawTrx;
  seconds sleepingTime(1);
  static constexpr int kNbRetriesCatchWindow = 15;
  for (int retryPos = 0; retryPos < kNbRetriesCatchWindow && !newWithdrawTrx.has_value(); ++retryPos) {
    if (retryPos != 0) {
      log::warn("Cannot retrieve just launched withdraw, retry {}/{} in {}...", retryPos, kNbRetriesCatchWindow,
                DurationToString(sleepingTime));
      std::this_thread::sleep_for(sleepingTime);
      sleepingTime = (3 * sleepingTime) / 2;
    }
    auto currentWithdraws =
        QueryUserTransactions(*this, _curlHandle, _apiKey, withdrawConstraints, UserTransactionEnum::kOngoingWithdraws);
    std::ranges::sort(currentWithdraws, CompareTrxByDate);

    // Isolate the new withdraws since the launch of our new withdraw
    decltype(currentWithdraws) newWithdraws;
    std::set_difference(currentWithdraws.begin(), currentWithdraws.end(), oldWithdraws.begin(), oldWithdraws.end(),
                        std::back_inserter(newWithdraws), CompareTrxByDate);

    log::debug("Isolated {} new withdraws, one of them is probably the one just launched", newWithdraws.size());

    for (schema::bithumb::UserTransactions::UserTransaction& withdrawTrx : newWithdraws) {
      MonetaryAmount withdrawNetEmittedAmount = RetrieveNetEmittedAmountFromTrxJson(withdrawTrx);
      if (withdrawNetEmittedAmount.isCloseTo(netEmittedAmount, 0.001)) {
        log::debug("Found new withdraw of {} {}", withdrawTrx.units, withdrawTrx.order_currency);
        newWithdrawTrx = std::move(withdrawTrx);
        break;
      }
      log::debug("Withdraw of {} {} is too different from our amount {}", withdrawTrx.units, withdrawTrx.order_currency,
                 netEmittedAmount);
    }
  }

  if (!newWithdrawTrx.has_value()) {
    throw exception(
        "Unable to retrieve just launch withdraw from Bithumb - it may be pending verification, please check your "
        "emails");
  }

  return {std::move(destinationWallet), GenerateWithdrawIdFromTrx(netEmittedAmount, *newWithdrawTrx), grossAmount};
}

void BithumbPrivate::updateCacheFile() const {
  GetBithumbCurrencyInfoMapCache(_coincenterInfo.dataDir()).write(WriteMiniJsonOrThrow(_currencyOrderInfoMap));
}

}  // namespace cct::api
