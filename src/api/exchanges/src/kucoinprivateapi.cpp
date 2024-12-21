#include "kucoinprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "base64.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "closed-order.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "kucoin-schema.hpp"
#include "kucoinpublicapi.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "request-retry.hpp"
#include "ssl_sha.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

auto ComputeBaseStrToSign(HttpRequestType requestType, std::string_view method, string::size_type additionalSize,
                          std::string_view nonceStr, string& strToSign) {
  const auto httpRequestTypeStr = HttpRequestTypeToString(requestType);

  strToSign.resize(nonceStr.size() + httpRequestTypeStr.size() + method.size() + additionalSize, '?');

  auto it = std::ranges::copy(nonceStr, strToSign.data()).out;
  it = std::ranges::copy(httpRequestTypeStr, it).out;
  return std::ranges::copy(method, it).out;
}

CurlOptions CreateCurlOptions(const APIKey& apiKey, HttpRequestType requestType, std::string_view method,
                              string strToSign, std::string_view nonceTimeStr,
                              CurlPostData&& postData = CurlPostData()) {
  CurlOptions::PostDataFormat postDataFormat = CurlOptions::PostDataFormat::kString;
  if (!postData.empty()) {
    if (requestType == HttpRequestType::kGet || requestType == HttpRequestType::kDelete) {
      std::string_view postDataStr = postData.str();
      auto it = ComputeBaseStrToSign(requestType, method, postDataStr.size() + 1UL, nonceTimeStr, strToSign);
      std::ranges::copy(postDataStr, ++it);
    } else {
      auto postDataJsonStr = postData.toJsonStr();
      auto it = ComputeBaseStrToSign(requestType, method, postDataJsonStr.size(), nonceTimeStr, strToSign);
      std::ranges::copy(postDataJsonStr, it);
      postDataFormat = CurlOptions::PostDataFormat::json;
    }
  }

  CurlOptions opts(requestType, std::move(postData), postDataFormat);

  auto& httpHeaders = opts.mutableHttpHeaders();

  httpHeaders.emplace_back("KC-API-KEY", apiKey.key());
  httpHeaders.emplace_back("KC-API-SIGN", B64Encode(ssl::Sha256Bin(strToSign, apiKey.privateKey())));
  httpHeaders.emplace_back("KC-API-TIMESTAMP", nonceTimeStr);
  httpHeaders.emplace_back("KC-API-PASSPHRASE", B64Encode(ssl::Sha256Bin(apiKey.passphrase(), apiKey.privateKey())));
  httpHeaders.emplace_back("KC-API-KEY-VERSION", 2);

  return opts;
}

template <class T>
T PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view method,
               CurlPostData&& postData = CurlPostData()) {
  auto nonceTimeStr = Nonce_TimeSinceEpochInMs();
  string strToSign;
  RequestRetry requestRetry(
      curlHandle, CreateCurlOptions(apiKey, requestType, method, strToSign, nonceTimeStr, std::move(postData)),
      QueryRetryPolicy{.initialRetryDelay = seconds{1}, .nbMaxRetries = 3});

  return requestRetry.query<T>(
      method,
      [requestType](const T& response) {
        if constexpr (amc::is_detected<schema::kucoin::has_code_t, T>::value) {
          if (response.code != KucoinPublic::kStatusCodeOK) {
            log::warn("Kucoin error code: {}", response.code);
            if constexpr (amc::is_detected<schema::kucoin::has_msg_t, T>::value) {
              if (!response.msg.empty()) {
                log::warn("Kucoin msg: {}", response.msg);
              }
            }
            if (requestType == HttpRequestType::kDelete) {
              log::warn("Kucoin error {}: bypassed, object probably disappeared correctly", response.code);
              return RequestRetry::Status::kResponseOK;
            }
            return RequestRetry::Status::kResponseError;
          }
        }
        return RequestRetry::Status::kResponseOK;
      },
      [&strToSign, &apiKey, &nonceTimeStr](CurlOptions& opts) {
        auto newNonceTimeStr = Nonce_TimeSinceEpochInMs();

        strToSign.replace(0UL, nonceTimeStr.size(), newNonceTimeStr);

        auto& httpHeaders = opts.mutableHttpHeaders();
        httpHeaders.set("KC-API-SIGN", B64Encode(ssl::Sha256Bin(strToSign, apiKey.privateKey())));
        httpHeaders.set("KC-API-TIMESTAMP", newNonceTimeStr);

        nonceTimeStr = std::move(newNonceTimeStr);
      });
}

void InnerTransfer(CurlHandle& curlHandle, const APIKey& apiKey, MonetaryAmount amount, std::string_view fromStr,
                   std::string_view toStr) {
  log::info("Perform inner transfer of {} to {} account", amount, toStr);

  PrivateQuery<schema::kucoin::V1AccountsInnerTransfer>(
      curlHandle, apiKey, HttpRequestType::kPost, "/api/v2/accounts/inner-transfer",
      {{"clientOid", Nonce_TimeSinceEpochInMs()},  // Seems useless, but it's mandatory apparently
       {"currency", amount.currencyStr()},
       {"amount", amount.amountStr()},
       {"from", fromStr},
       {"to", toStr}});
}

bool EnsureEnoughAmountIn(CurlHandle& curlHandle, const APIKey& apiKey, MonetaryAmount expectedAmount,
                          std::string_view accountName) {
  // Check if enough balance in the 'accountName' account of Kucoin
  CurrencyCode cur = expectedAmount.currencyCode();
  auto res = PrivateQuery<schema::kucoin::V1Accounts>(curlHandle, apiKey, HttpRequestType::kGet, "/api/v1/accounts",
                                                      {{"currency", cur.str()}})
                 .data;
  MonetaryAmount totalAvailableAmount(0, cur);
  MonetaryAmount amountInTargetAccount = totalAvailableAmount;
  for (const auto& balanceDetail : res) {
    std::string_view typeStr = balanceDetail.type;
    MonetaryAmount av(balanceDetail.available, cur);
    totalAvailableAmount += av;
    if (typeStr == accountName) {
      amountInTargetAccount = av;
    }
  }
  if (totalAvailableAmount < expectedAmount) {
    log::error("Insufficient funds to place in '{}' ({} < {})", accountName, totalAvailableAmount, expectedAmount);
    return false;
  }
  if (amountInTargetAccount < expectedAmount) {
    for (const auto& balanceDetail : res) {
      std::string_view typeStr = balanceDetail.type;
      MonetaryAmount av(balanceDetail.available, cur);
      if (typeStr != accountName && av != 0) {
        MonetaryAmount remainingAmountToInnerTransfer = expectedAmount - amountInTargetAccount;
        if (av < remainingAmountToInnerTransfer) {
          InnerTransfer(curlHandle, apiKey, av, typeStr, accountName);
          amountInTargetAccount += av;
        } else {
          InnerTransfer(curlHandle, apiKey, remainingAmountToInnerTransfer, typeStr, accountName);
          break;
        }
      }
    }
  }
  return true;
}

}  // namespace

KucoinPrivate::KucoinPrivate(const CoincenterInfo& coincenterInfo, KucoinPublic& kucoinPublic, const APIKey& apiKey)
    : ExchangePrivate(coincenterInfo, kucoinPublic, apiKey),
      _curlHandle(KucoinPublic::kUrlBase, coincenterInfo.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  coincenterInfo.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::depositWallet).duration,
                              _cachedResultVault),
          _curlHandle, _apiKey, kucoinPublic) {}

bool KucoinPrivate::validateApiKey() {
  auto ret = PrivateQuery<schema::kucoin::V1Accounts>(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/accounts");
  return ret.code == KucoinPublic::kStatusCodeOK;
}

BalancePortfolio KucoinPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  auto result =
      PrivateQuery<schema::kucoin::V1Accounts>(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/accounts").data;
  BalancePortfolio balancePortfolio;
  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;

  balancePortfolio.reserve(static_cast<BalancePortfolio::size_type>(result.size()));

  for (const auto& balanceDetail : result) {
    if (balanceDetail.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the balance", balanceDetail.currency,
                exchangeName());
      continue;
    }
    std::string_view typeStr = balanceDetail.type;
    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(balanceDetail.currency));
    MonetaryAmount amount(withBalanceInUse ? balanceDetail.balance : balanceDetail.available, currencyCode);
    log::debug("{} in account '{}' on {}", amount, typeStr, exchangeName());

    balancePortfolio += amount;
  }
  return balancePortfolio;
}

Wallet KucoinPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  auto depositAddresses =
      PrivateQuery<schema::kucoin::V3DepositAddresses>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                       "/api/v3/deposit-addresses", {{"currency", currencyCode.str()}})
          .data;
  ExchangeName exchangeName(_kucoinPublic.exchangeNameEnum(), _apiKey.name());
  schema::kucoin::V3DepositAddress depositAddress;
  if (depositAddresses.empty()) {
    log::info("No deposit address for {} in {}, creating one", currencyCode, exchangeName);
    depositAddress = PrivateQuery<schema::kucoin::V3DepositAddressCreate>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                                          "/api/v3/deposit-address/create",
                                                                          {{"currency", currencyCode.str()}})
                         .data;
  } else {
    depositAddress = std::move(depositAddresses.front());
  }

  const CoincenterInfo& coincenterInfo = _kucoinPublic.coincenterInfo();
  bool doCheckWallet =
      coincenterInfo.exchangeConfig(_kucoinPublic.exchangeNameEnum()).withdraw.validateDepositAddressesInFile;
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  Wallet wallet(std::move(exchangeName), currencyCode, std::move(depositAddress.address),
                std::move(depositAddress.memo), walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
template <class OrderVectorType>
void FillOrders(const OrdersConstraints& ordersConstraints, CurlHandle& curlHandle, const APIKey& apiKey,
                ExchangePublic& exchangePublic, OrderVectorType& orderVector) {
  using OrderType = std::remove_cvref_t<decltype(*std::declval<OrderVectorType>().begin())>;

  CurlPostData params{{"status", std::is_same_v<OrderType, OpenedOrder> ? "active" : "done"}, {"tradeType", "TRADE"}};

  if (ordersConstraints.isCurDefined()) {
    MarketSet markets;
    Market filterMarket =
        exchangePublic.determineMarketFromFilterCurrencies(markets, ordersConstraints.cur1(), ordersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.emplace_back("symbol", filterMarket.assetsPairStrUpper('-'));
    }
  }
  if (ordersConstraints.isPlacedTimeAfterDefined()) {
    params.emplace_back("startAt", TimestampToMillisecondsSinceEpoch(ordersConstraints.placedAfter()));
  }
  if (ordersConstraints.isPlacedTimeBeforeDefined()) {
    params.emplace_back("endAt", TimestampToMillisecondsSinceEpoch(ordersConstraints.placedBefore()));
  }
  auto data = PrivateQuery<schema::kucoin::V1Orders>(curlHandle, apiKey, HttpRequestType::kGet, "/api/v1/orders",
                                                     std::move(params))
                  .data;

  for (auto& orderDetails : data.items) {
    std::string_view marketStr = orderDetails.symbol;
    const auto dashPos = marketStr.find('-');

    if (dashPos == std::string_view::npos) {
      throw exception("Expected a dash in {} for {} orders query", marketStr, exchangePublic.name());
    }

    std::string_view priceCurStr = marketStr.substr(0U, dashPos);
    if (priceCurStr.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the orders", priceCurStr,
                exchangePublic.name());
      continue;
    }

    std::string_view volumeCurStr = marketStr.substr(dashPos + 1U);
    if (volumeCurStr.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the orders", volumeCurStr,
                exchangePublic.name());
      continue;
    }

    const CurrencyCode priceCur = priceCurStr;
    const CurrencyCode volumeCur = volumeCurStr;

    if (!ordersConstraints.validateCur(volumeCur, priceCur)) {
      continue;
    }

    const TimePoint placedTime{milliseconds(orderDetails.createdAt)};

    if (!ordersConstraints.validateId(orderDetails.id)) {
      continue;
    }

    const MonetaryAmount matchedVolume(orderDetails.dealSize, volumeCur);
    const MonetaryAmount price(orderDetails.price, priceCur);
    const TradeSide side = orderDetails.side == "buy" ? TradeSide::buy : TradeSide::sell;

    if constexpr (std::is_same_v<OrderType, OpenedOrder>) {
      const MonetaryAmount originalVolume(orderDetails.size, volumeCur);
      const MonetaryAmount remainingVolume = originalVolume - matchedVolume;

      orderVector.emplace_back(std::move(orderDetails.id), matchedVolume, remainingVolume, price, placedTime, side);
    } else if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
      const TimePoint matchedTime = placedTime;

      orderVector.emplace_back(std::move(orderDetails.id), matchedVolume, price, placedTime, matchedTime, side);
    } else {
      // Note: below ugly template lambda can be replaced with 'static_assert(false);' in C++23
      []<bool flag = false>() { static_assert(flag, "no match"); }();
    }
  }
  std::ranges::sort(orderVector);
  orderVector.shrink_to_fit();
}
}  // namespace

ClosedOrderVector KucoinPrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  ClosedOrderVector closedOrders;
  FillOrders(closedOrdersConstraints, _curlHandle, _apiKey, _exchangePublic, closedOrders);
  log::info("Retrieved {} closed orders from {}", closedOrders.size(), _exchangePublic.name());
  return closedOrders;
}

OpenedOrderVector KucoinPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  OpenedOrderVector openedOrders;
  FillOrders(openedOrdersConstraints, _curlHandle, _apiKey, _exchangePublic, openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

int KucoinPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.isMarketOnlyDependent() || openedOrdersConstraints.noConstraints()) {
    CurlPostData params;
    if (openedOrdersConstraints.isMarketDefined()) {
      params.emplace_back("symbol", openedOrdersConstraints.market().assetsPairStrUpper('-'));
    }
    auto res = PrivateQuery<schema::kucoin::V1DeleteOrders>(_curlHandle, _apiKey, HttpRequestType::kDelete,
                                                            "/api/v1/orders", std::move(params));
    return res.data.cancelledOrderIds.size();
  }
  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const OpenedOrder& order : openedOrders) {
    cancelOrderProcess(order.id());
  }
  return openedOrders.size();
}

namespace {
Deposit::Status DepositStatusFromStatus(schema::kucoin::V1Deposits::Data::Item::Status depositStatus) {
  if (depositStatus == schema::kucoin::V1Deposits::Data::Item::Status::SUCCESS) {
    return Deposit::Status::success;
  }
  if (depositStatus == schema::kucoin::V1Deposits::Data::Item::Status::PROCESSING) {
    return Deposit::Status::processing;
  }
  if (depositStatus == schema::kucoin::V1Deposits::Data::Item::Status::FAILURE) {
    return Deposit::Status::failed;
  }
  throw exception("Unrecognized deposit status '{}' from Kucoin", static_cast<int>(depositStatus));
}
}  // namespace

DepositsSet KucoinPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.emplace_back("currency", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isTimeAfterDefined()) {
    options.emplace_back("startAt", TimestampToMillisecondsSinceEpoch(depositsConstraints.timeAfter()));
  }
  if (depositsConstraints.isTimeBeforeDefined()) {
    options.emplace_back("endAt", TimestampToMillisecondsSinceEpoch(depositsConstraints.timeBefore()));
  }
  if (depositsConstraints.isIdDefined()) {
    if (depositsConstraints.idSet().size() == 1) {
      options.emplace_back("txId", depositsConstraints.idSet().front());
    }
  }
  auto depositJson = PrivateQuery<schema::kucoin::V1Deposits>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                              "/api/v1/deposits", std::move(options))
                         .data;

  Deposits deposits;

  deposits.reserve(static_cast<Deposits::size_type>(depositJson.items.size()));
  for (const auto& depositDetail : depositJson.items) {
    if (depositDetail.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for {}, do not consider it in the deposits", depositDetail.currency,
                exchangeName());
      continue;
    }
    CurrencyCode currencyCode(depositDetail.currency);
    MonetaryAmount amount(depositDetail.amount, currencyCode);
    int64_t millisecondsSinceEpoch = depositDetail.updatedAt;

    Deposit::Status status = DepositStatusFromStatus(depositDetail.status);

    TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};

    // Kucoin does not provide any transaction id, let's generate it from currency and timestamp...
    string id = currencyCode.str();
    id.push_back('-');
    id.append(std::string_view(IntegralToCharVector(millisecondsSinceEpoch)));

    deposits.emplace_back(std::move(id), timestamp, amount, status);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatus(schema::kucoin::V1Withdrawals::Data::Item::Status status, bool logStatus) {
  if (status == schema::kucoin::V1Withdrawals::Data::Item::Status::PROCESSING) {
    if (logStatus) {
      log::debug("Processing");
    }
    return Withdraw::Status::processing;
  }
  if (status == schema::kucoin::V1Withdrawals::Data::Item::Status::WALLET_PROCESSING) {
    if (logStatus) {
      log::debug("Wallet processing");
    }
    return Withdraw::Status::processing;
  }
  if (status == schema::kucoin::V1Withdrawals::Data::Item::Status::SUCCESS) {
    if (logStatus) {
      log::debug("Success");
    }
    return Withdraw::Status::success;
  }
  if (status == schema::kucoin::V1Withdrawals::Data::Item::Status::FAILURE) {
    if (logStatus) {
      log::warn("Failure");
    }
    return Withdraw::Status::failed;
  }
  throw exception("unknown status value '{}' returned by Kucoin", static_cast<int>(status));
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options;
  if (withdrawsConstraints.isCurDefined()) {
    options.emplace_back("currency", withdrawsConstraints.currencyCode().str());
  }
  if (withdrawsConstraints.isTimeAfterDefined()) {
    options.emplace_back("startAt", TimestampToMillisecondsSinceEpoch(withdrawsConstraints.timeAfter()));
  }
  if (withdrawsConstraints.isTimeBeforeDefined()) {
    options.emplace_back("endAt", TimestampToMillisecondsSinceEpoch(withdrawsConstraints.timeBefore()));
  }
  return options;
}

}  // namespace

WithdrawsSet KucoinPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  auto withdrawJson =
      PrivateQuery<schema::kucoin::V1Withdrawals>(_curlHandle, _apiKey, HttpRequestType::kGet, "/api/v1/withdrawals",
                                                  CreateOptionsFromWithdrawConstraints(withdrawsConstraints))
          .data;

  Withdraws withdraws;

  withdraws.reserve(static_cast<Withdraws::size_type>(withdrawJson.items.size()));
  for (auto& withdrawDetail : withdrawJson.items) {
    CurrencyCode currencyCode(withdrawDetail.currency);
    MonetaryAmount netEmittedAmount(withdrawDetail.amount, currencyCode);
    MonetaryAmount fee(withdrawDetail.fee, currencyCode);
    int64_t millisecondsSinceEpoch = withdrawDetail.updatedAt;

    Withdraw::Status status = WithdrawStatusFromStatus(withdrawDetail.status, withdrawsConstraints.isIdDependent());

    TimePoint timestamp{milliseconds(millisecondsSinceEpoch)};

    if (!withdrawsConstraints.validateId(withdrawDetail.id)) {
      continue;
    }

    withdraws.emplace_back(std::move(withdrawDetail.id), timestamp, netEmittedAmount, status, fee);
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdrawals for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

PlaceOrderInfo KucoinPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                         const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  if (!EnsureEnoughAmountIn(_curlHandle, _apiKey, from, "trade")) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  const Market mk = tradeInfo.tradeContext.market;

  bool isTakerStrategy =
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeConfig().query.placeSimulateRealOrder);

  KucoinPublic& kucoinPublic = dynamic_cast<KucoinPublic&>(_exchangePublic);

  price = kucoinPublic.sanitizePrice(mk, price);

  MonetaryAmount sanitizedVol = kucoinPublic.sanitizeVolume(mk, volume);
  if (volume < sanitizedVol) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode,
              sanitizedVol);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  std::string_view buyOrSell = fromCurrencyCode == mk.base() ? "sell" : "buy";
  std::string_view strategyType = isTakerStrategy ? "market" : "limit";

  CurlPostData params = KucoinPublic::GetSymbolPostData(mk);
  params.emplace_back("clientOid", Nonce_TimeSinceEpochInMs());
  params.emplace_back("side", buyOrSell);
  params.emplace_back("type", strategyType);
  params.emplace_back("remark", "Placed by coincenter client");
  params.emplace_back("tradeType", "TRADE");
  params.emplace_back("size", volume.amountStr());
  if (!isTakerStrategy) {
    params.emplace_back("price", price.amountStr());
  }

  // Add automatic cancelling just in case program unexpectedly stops
  params.emplace_back("timeInForce", "GTT");  // Good until cancelled or time expires
  params.emplace_back("cancelAfter", std::chrono::duration_cast<seconds>(tradeInfo.options.maxTradeTime()).count() + 1);

  auto result = PrivateQuery<schema::kucoin::V1OrdersPlace>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                            "/api/v1/orders", std::move(params))
                    .data;
  placeOrderInfo.orderId = std::move(result.orderId);
  return placeOrderInfo;
}

OrderInfo KucoinPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId);
  return queryOrderInfo(orderId, tradeContext);
}

void KucoinPrivate::cancelOrderProcess(OrderIdView orderId) {
  const auto endpoint = fmt::format("/api/v1/orders/{}", orderId);
  PrivateQuery<schema::kucoin::V1OrderCancel>(_curlHandle, _apiKey, HttpRequestType::kDelete, endpoint);
}

OrderInfo KucoinPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  const CurrencyCode fromCurrencyCode(tradeContext.fromCur());
  const Market mk = tradeContext.market;
  const auto endpoint = fmt::format("/api/v1/orders/{}", orderId);

  auto data = PrivateQuery<schema::kucoin::V1OrderInfo>(_curlHandle, _apiKey, HttpRequestType::kGet, endpoint).data;

  MonetaryAmount size(data.size, mk.base());
  MonetaryAmount matchedSize(data.dealSize, mk.base());

  // Fee is already deduced from the matched amount
  MonetaryAmount fromAmount;
  MonetaryAmount toAmount;
  MonetaryAmount dealFunds(data.dealFunds, mk.quote());
  if (fromCurrencyCode == mk.base()) {
    // sell
    fromAmount = matchedSize;
    toAmount = dealFunds;
  } else {
    // buy
    fromAmount = dealFunds;
    toAmount = matchedSize;
  }
  return OrderInfo(TradedAmounts(fromAmount, toAmount), !data.isActive);
}

InitiatedWithdrawInfo KucoinPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  if (!EnsureEnoughAmountIn(_curlHandle, _apiKey, grossAmount, "main")) {
    throw exception("Insufficient funds for withdraw");
  }
  const CurrencyCode currencyCode = grossAmount.currencyCode();

  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFeeOrZero(currencyCode);

  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;

  CurlPostData opts{{"currency", currencyCode.str()},
                    {"address", destinationWallet.address()},
                    {"amount", netEmittedAmount.amountStr()}};
  if (destinationWallet.hasTag()) {
    opts.emplace_back("memo", destinationWallet.tag());
  }

  auto result = PrivateQuery<schema::kucoin::V3ApplyWithdrawal>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                                "/api/v3/withdrawals", std::move(opts))
                    .data;
  return {std::move(destinationWallet), std::move(result.withdrawalId), grossAmount};
}

}  // namespace cct::api
