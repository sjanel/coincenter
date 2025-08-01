#include "upbitprivateapi.hpp"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
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
#include "currencycodeset.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchange-tradefees-config.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "query-retry-policy.hpp"
#include "request-retry.hpp"
#include "ssl_sha.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "upbit-schema.hpp"
#include "upbitpublicapi.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

enum class IfError : int8_t { kThrow, kNoThrow };

string ComputeAuthToken(const APIKey& apiKey, const CurlPostData& postData) {
  auto jsonWebToken = jwt::create()
                          .set_type("JWT")
                          .set_payload_claim("access_key", jwt::claim(std::string(apiKey.key())))
                          .set_payload_claim("nonce", jwt::claim(std::string(Nonce_TimeSinceEpochInMs())));

  if (!postData.empty()) {
    const auto queryHash = ssl::Sha512Digest(postData.str());

    jsonWebToken.set_payload_claim("query_hash", jwt::claim(std::string(queryHash.data(), queryHash.size())))
        .set_payload_claim("query_hash_alg", jwt::claim(std::string("SHA512")));
  }

  // hs256 does not accept std::string_view, we need a copy...
  const auto token = jsonWebToken.sign(jwt::algorithm::hs256{std::string(apiKey.privateKey())});

  static constexpr std::string_view kBearerPrefix = "Bearer";
  string authStr(kBearerPrefix.size() + 1U + token.size(), ' ');
  auto it = std::ranges::copy(kBearerPrefix, authStr.begin()).out;
  std::ranges::copy(token, it + 1);
  return authStr;
}

template <class T, class CurlPostDataT = CurlPostData>
std::pair<T, schema::upbit::Error> PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey,
                                                HttpRequestType requestType, std::string_view endpoint,
                                                CurlPostDataT&& curlPostData = CurlPostData(),
                                                int16_t nbMaxRetries = 3) {
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData));

  opts.mutableHttpHeaders().emplace_back("Authorization", ComputeAuthToken(apiKey, opts.postData()));

  RequestRetry requestRetry(
      curlHandle, std::move(opts),
      QueryRetryPolicy{.initialRetryDelay = seconds{1}, .exponentialBackoff = 1.5, .nbMaxRetries = nbMaxRetries});

  return schema::upbit::GetOrValueInitialized<T>(requestRetry, endpoint, [&apiKey](CurlOptions& curlOptions) {
    curlOptions.mutableHttpHeaders().set_back("Authorization", ComputeAuthToken(apiKey, curlOptions.postData()));
  });
}
}  // namespace

UpbitPrivate::UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(config, upbitPublic, apiKey),
      _curlHandle(UpbitPublic::kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::currencies), _cachedResultVault),
          _curlHandle, _apiKey, exchangeConfig().asset, upbitPublic._commonApi),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::depositWallet), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::withdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic) {}

bool UpbitPrivate::validateApiKey() {
  auto ret = PrivateQuery<schema::upbit::V1ApiKeys>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/api_keys").first;
  return !ret.empty();
}

CurrencyExchangeFlatSet UpbitPrivate::TradableCurrenciesFunc::operator()() {
  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;
  CurrencyExchangeVector currencies;
  auto result =
      PrivateQuery<schema::upbit::V1StatusWallets>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/status/wallet")
          .first;
  for (const auto& curDetails : result) {
    if (curDetails.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long, do not consider it in the currencies", curDetails.currency);
      continue;
    }
    CurrencyCode cur(curDetails.currency);
    CurrencyCode networkName(curDetails.net_type);
    if (cur != networkName) {
      log::debug("Forgive about {}-{} as net type is not the main one", cur, networkName);
      continue;
    }
    if (UpbitPublic::CheckCurrencyCode(cur, excludedCurrencies)) {
      CurrencyExchange::Withdraw withdrawStatus = CurrencyExchange::Withdraw::kUnavailable;
      CurrencyExchange::Deposit depositStatus = CurrencyExchange::Deposit::kUnavailable;

      switch (curDetails.wallet_state) {
        case schema::upbit::V1StatusWallet::WalletState::working:
          withdrawStatus = CurrencyExchange::Withdraw::kAvailable;
          depositStatus = CurrencyExchange::Deposit::kAvailable;
          break;
        case schema::upbit::V1StatusWallet::WalletState::withdraw_only:
          withdrawStatus = CurrencyExchange::Withdraw::kAvailable;
          break;
        case schema::upbit::V1StatusWallet::WalletState::deposit_only:
          depositStatus = CurrencyExchange::Deposit::kAvailable;
          break;
        default:
          break;
      }

      if (withdrawStatus == CurrencyExchange::Withdraw::kUnavailable) {
        log::debug("{} cannot be withdrawn from Upbit", cur);
      }
      if (depositStatus == CurrencyExchange::Deposit::kUnavailable) {
        log::debug("{} cannot be deposited to Upbit", cur);
      }
      currencies.emplace_back(
          cur, cur, cur, depositStatus, withdrawStatus,
          _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Upbit currencies", ret.size());
  return ret;
}

BalancePortfolio UpbitPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  const bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;

  BalancePortfolio balancePortfolio;

  auto ret = PrivateQuery<schema::upbit::V1Accounts>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/accounts").first;

  balancePortfolio.reserve(static_cast<BalancePortfolio::size_type>(ret.size()));

  for (const auto& accountDetail : ret) {
    if (accountDetail.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code '{}' is too long for Upbit, do not consider it in the balance", accountDetail.currency);
      continue;
    }
    const CurrencyCode currencyCode(accountDetail.currency);
    MonetaryAmount availableAmount(accountDetail.balance, currencyCode);

    if (withBalanceInUse) {
      availableAmount += MonetaryAmount(accountDetail.locked, currencyCode);
    }

    balancePortfolio += availableAmount;
  }
  return balancePortfolio;
}

Wallet UpbitPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurlPostData postData{{"currency", currencyCode.str()}, {"net_type", currencyCode.str()}};
  auto [result, error] = PrivateQuery<schema::upbit::V1DepositCoinAddress>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                                           "/v1/deposits/coin_address", postData, 1);
  bool generateDepositAddressNeeded = false;
  if (std::holds_alternative<string>(error.error.name)) {
    std::string_view msg = error.error.message;
    if (std::get<string>(error.error.name) == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one", currencyCode);
      generateDepositAddressNeeded = true;
    } else {
      throw exception("Upbit error: {}, msg: {}", std::get<string>(error.error.name), msg);
    }
  }
  if (generateDepositAddressNeeded) {
    auto genCoinAddressResult =
        PrivateQuery<schema::upbit::V1DepositsGenerateCoinAddress>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                                   "/v1/deposits/generate_coin_address", postData)
            .first;
    if (genCoinAddressResult.success) {
      log::info("Successfully generated address");
    } else {
      log::error("Failed to generate address (or unexpected answer), message: {}", genCoinAddressResult.message);
    }
    log::info("Waiting for address to be generated...");
    result = PrivateQuery<schema::upbit::V1DepositCoinAddress>(_curlHandle, _apiKey, HttpRequestType::kGet,
                                                               "/v1/deposits/coin_address", postData, 10)
                 .first;
  }

  std::string_view tag;
  if (result.secondary_address) {
    tag = *result.secondary_address;
  }

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet =
      coincenterInfo.exchangeConfig(_exchangePublic.exchangeNameEnum()).withdraw.validateDepositAddressesInFile;
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet wallet(ExchangeName(_exchangePublic.exchangeNameEnum(), _apiKey.name()), currencyCode,
                std::move(result.deposit_address), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
template <class OrderVectorType>
void FillOrders(const OrdersConstraints& ordersConstraints, CurlHandle& curlHandle, const APIKey& apiKey,
                ExchangePublic& exchangePublic, OrderVectorType& orderVector) {
  using OrderType = std::remove_cvref_t<decltype(*std::declval<OrderVectorType>().begin())>;

  int page = 0;

  static constexpr bool kIsOpenedOrder = std::is_same_v<OrderType, OpenedOrder>;

  CurlPostData params;

  static constexpr int kMaxNbOrdersPerPage = kIsOpenedOrder ? 100 : 1000;
  static constexpr auto kNbMaxPagesToRetrieve = kIsOpenedOrder ? 10 : 1;

  if constexpr (kIsOpenedOrder) {
    params.emplace_back("page", page);
  } else {
    params.emplace_back("state", "done");
    params.emplace_back("limit", kMaxNbOrdersPerPage);
  }

  if (ordersConstraints.isCurDefined()) {
    MarketSet markets;
    Market filterMarket =
        exchangePublic.determineMarketFromFilterCurrencies(markets, ordersConstraints.cur1(), ordersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.emplace_back("market", UpbitPublic::ReverseMarketStr(filterMarket));
    }
  }

  for (auto nbOrdersRetrieved = kMaxNbOrdersPerPage;
       nbOrdersRetrieved == kMaxNbOrdersPerPage && page < kNbMaxPagesToRetrieve;) {
    if constexpr (kIsOpenedOrder) {
      params.set("page", ++page);
    }

    static constexpr std::string_view kOpenedOrdersEndpoint = "/v1/orders/open";
    static constexpr std::string_view kClosedOrdersEndpoint = "/v1/orders/closed";

    std::string_view endpoint = kIsOpenedOrder ? kOpenedOrdersEndpoint : kClosedOrdersEndpoint;

    auto data =
        PrivateQuery<schema::upbit::V1Orders>(curlHandle, apiKey, HttpRequestType::kGet, endpoint, params).first;

    nbOrdersRetrieved = static_cast<decltype(nbOrdersRetrieved)>(data.size());

    for (auto& orderDetails : data) {
      const std::string_view marketStr = orderDetails.market;
      const auto dashPos = marketStr.find('-');

      if (dashPos == std::string_view::npos) {
        log::error("Expected a dash in {} for {} orders query", marketStr, exchangePublic.name());
        continue;
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

      // 'created_at' string is in this format: "2019-01-04T13:48:09+09:00"
      const auto placedTime = StringToTime(orderDetails.created_at, kTimeYearToSecondTSeparatedFormat);
      if (!ordersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      if (!ordersConstraints.validateId(orderDetails.uuid)) {
        continue;
      }

      if (orderDetails.price.isDefault()) {
        // Some old orders may have no price field set. In this case, just return what we have as the older orders will
        // probably not be filled as well.
        break;
      }

      const MonetaryAmount matchedVolume(orderDetails.executed_volume, volumeCur);
      const MonetaryAmount price(orderDetails.price, priceCur);
      const TradeSide side = orderDetails.side == "bid" ? TradeSide::buy : TradeSide::sell;

      if constexpr (kIsOpenedOrder) {
        const MonetaryAmount remainingVolume(orderDetails.remaining_volume, volumeCur);

        orderVector.emplace_back(std::move(orderDetails.uuid), matchedVolume, remainingVolume, price, placedTime, side);
      } else if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
        const auto matchedTime = placedTime;

        orderVector.emplace_back(std::move(orderDetails.uuid), matchedVolume, price, placedTime, matchedTime, side);
      } else {
        // Note: below ugly template lambda can be replaced with 'static_assert(false);' in C++23
        []<bool flag = false>() { static_assert(flag, "no match"); }();
      }
    }
  }

  if (page == kNbMaxPagesToRetrieve) {
    log::warn("Already queried {} order pages, stop the queries at this point", page);
    log::warn("Try to refine the orders query by specifying the market");
  }

  std::ranges::sort(orderVector);

  orderVector.shrink_to_fit();
}
}  // namespace

ClosedOrderVector UpbitPrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  ClosedOrderVector closedOrders;
  FillOrders(closedOrdersConstraints, _curlHandle, _apiKey, _exchangePublic, closedOrders);
  log::info("Retrieved {} closed orders from {}", closedOrders.size(), _exchangePublic.name());
  return closedOrders;
}

OpenedOrderVector UpbitPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  OpenedOrderVector openedOrders;
  FillOrders(openedOrdersConstraints, _curlHandle, _apiKey, _exchangePublic, openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), _exchangePublic.name());
  return openedOrders;
}

int UpbitPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  // No faster way to cancel several orders at once, doing a simple for loop
  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const OpenedOrder& order : openedOrders) {
    TradeContext tradeContext(order.market(), order.side());
    cancelOrder(order.id(), tradeContext);
  }
  return openedOrders.size();
}

namespace {
Deposit::Status DepositStatusFromStatus(schema::upbit::V1Deposit::State state) {
  switch (state) {
    case schema::upbit::V1Deposit::State::ACCEPTED:
      return Deposit::Status::success;
    case schema::upbit::V1Deposit::State::CANCELLED:
      [[fallthrough]];
    case schema::upbit::V1Deposit::State::REJECTED:
      [[fallthrough]];
    case schema::upbit::V1Deposit::State::TRAVEL_RULE_SUSPECTED:
      [[fallthrough]];
    case schema::upbit::V1Deposit::State::REFUNDED:
      return Deposit::Status::failed;
    case schema::upbit::V1Deposit::State::PROCESSING:
      [[fallthrough]];
    case schema::upbit::V1Deposit::State::REFUNDING:
      return Deposit::Status::processing;
    default:
      throw exception("Unrecognized deposit status '{}' from Upbit", static_cast<int>(state));
  }
}

constexpr int kNbResultsPerPage = 100;

}  // namespace

DepositsSet UpbitPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options{{"limit", kNbResultsPerPage}};
  if (depositsConstraints.isCurDefined()) {
    options.emplace_back("currency", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isIdDefined()) {
    for (std::string_view depositId : depositsConstraints.idSet()) {
      // Use the "PHP" method of arrays in query string parameter
      options.emplace_back("txids[]", depositId);
    }
  }

  // To make sure we retrieve all results, ask for next page when maximum results per page is returned
  for (int nbResults = kNbResultsPerPage, page = 1; nbResults == kNbResultsPerPage; ++page) {
    options.set("page", page);
    auto result =
        PrivateQuery<schema::upbit::V1Deposits>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits", options)
            .first;
    if (deposits.empty()) {
      deposits.reserve(static_cast<Deposits::size_type>(result.size()));
    }
    nbResults = static_cast<int>(result.size());
    for (auto& trx : result) {
      if (trx.currency.size() > CurrencyCode::kMaxLen) {
        log::warn("Currency code '{}' is too long for Upbit, do not consider it in the deposits", trx.currency);
        continue;
      }
      CurrencyCode currencyCode(trx.currency);
      MonetaryAmount amount(trx.amount, currencyCode);

      // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
      // It can be empty for deposits that failed - take the start time instead of the end time in this case
      std::string_view timeStr = trx.timeStr();

      TimePoint timestamp = StringToTime(timeStr, kTimeYearToSecondTSeparatedFormat);
      if (!depositsConstraints.validateTime(timestamp)) {
        continue;
      }

      deposits.emplace_back(std::move(trx.txid), timestamp, amount, DepositStatusFromStatus(trx.state));
    }
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatus(schema::upbit::V1Withdraw::State status) {
  switch (status) {
    case schema::upbit::V1Withdraw::State::WAITING:
      return Withdraw::Status::initial;
    case schema::upbit::V1Withdraw::State::PROCESSING:
      return Withdraw::Status::processing;
    case schema::upbit::V1Withdraw::State::DONE:
      return Withdraw::Status::success;
    case schema::upbit::V1Withdraw::State::FAILED:
      [[fallthrough]];
    case schema::upbit::V1Withdraw::State::CANCELLED:
      [[fallthrough]];
    case schema::upbit::V1Withdraw::State::CANCELED:
      [[fallthrough]];
    case schema::upbit::V1Withdraw::State::REJECTED:
      return Withdraw::Status::failed;
    default:
      throw exception("Unrecognized withdraw status '{}' from Upbit", static_cast<int>(status));
  }
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options{{"limit", kNbResultsPerPage}};
  if (withdrawsConstraints.isCurDefined()) {
    options.emplace_back("currency", withdrawsConstraints.currencyCode().str());
  }
  if (withdrawsConstraints.isIdDefined()) {
    for (std::string_view depositId : withdrawsConstraints.idSet()) {
      // Use the "PHP" method of arrays in query string parameter
      options.emplace_back("txids[]", depositId);
    }
  }
  return options;
}

}  // namespace

WithdrawsSet UpbitPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;
  CurlPostData options = CreateOptionsFromWithdrawConstraints(withdrawsConstraints);
  // To make sure we retrieve all results, ask for next page when maximum results per page is returned
  for (int nbResults = kNbResultsPerPage, page = 1; nbResults == kNbResultsPerPage; ++page) {
    options.set("page", page);
    auto result =
        PrivateQuery<schema::upbit::V1Withdraws>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws", options)
            .first;
    if (withdraws.empty()) {
      withdraws.reserve(static_cast<Withdraws::size_type>(result.size()));
    }
    nbResults = static_cast<int>(result.size());
    for (auto& trx : result) {
      if (trx.currency.size() > CurrencyCode::kMaxLen) {
        log::warn("Currency code '{}' is too long for Upbit, do not consider it in the withdraws", trx.currency);
        continue;
      }
      CurrencyCode currencyCode(trx.currency);
      MonetaryAmount netEmittedAmount(trx.amount, currencyCode);
      MonetaryAmount withdrawFee(trx.fee, currencyCode);
      // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
      // It can be empty for withdraws that failed - take the start time instead of the end time in this case
      std::string_view timeStr = trx.timeStr();

      TimePoint timestamp = StringToTime(timeStr, kTimeYearToSecondTSeparatedFormat);
      if (!withdrawsConstraints.validateTime(timestamp)) {
        continue;
      }

      withdraws.emplace_back(std::move(trx.txid), timestamp, netEmittedAmount, WithdrawStatusFromStatus(trx.state),
                             withdrawFee);
    }
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

namespace {
bool IsOrderClosed(std::string_view state) {
  if (state == "done" || state == "cancel") {
    return true;
  }
  if (state == "wait" || state == "watch") {
    return false;
  }
  log::error("Unknown state {} to be handled for Upbit", state);
  return true;
}

OrderInfo ParseOrderJson(const auto& orderJson, CurrencyCode fromCurrencyCode, Market mk) {
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, fromCurrencyCode == mk.base() ? mk.quote() : mk.base()),
                      IsOrderClosed(orderJson.state));

  if (orderJson.trades) {
    // TODO: to be confirmed (this is true at least for markets involving KRW)
    CurrencyCode feeCurrencyCode(mk.quote());
    MonetaryAmount fee(orderJson.paid_fee.value_or(MonetaryAmount{}), feeCurrencyCode);

    for (const auto& orderDetails : *orderJson.trades) {
      MonetaryAmount tradedVol(orderDetails.volume, mk.base());  // always in base currency
      MonetaryAmount price(orderDetails.price, mk.quote());      // always in quote currency
      MonetaryAmount tradedCost(orderDetails.funds, mk.quote());

      if (fromCurrencyCode == mk.quote()) {
        orderInfo.tradedAmounts.from += tradedCost;
        orderInfo.tradedAmounts.to += tradedVol;
      } else {
        orderInfo.tradedAmounts.from += tradedVol;
        orderInfo.tradedAmounts.to += tradedCost;
      }
    }
    if (fromCurrencyCode == mk.quote()) {
      orderInfo.tradedAmounts.from += fee;
    } else {
      orderInfo.tradedAmounts.to -= fee;
    }
  }

  return orderInfo;
}

}  // namespace

void UpbitPrivate::applyFee(Market mk, CurrencyCode fromCurrencyCode, bool isTakerStrategy, MonetaryAmount& from,
                            MonetaryAmount& volume) {
  if (fromCurrencyCode == mk.quote()) {
    // For 'buy', from amount is fee excluded
    const auto feeType = isTakerStrategy ? schema::ExchangeTradeFeesConfig::FeeType::Taker
                                         : schema::ExchangeTradeFeesConfig::FeeType::Maker;
    const auto& exchangeConfig = _coincenterInfo.exchangeConfig(_exchangePublic.exchangeNameEnum());
    if (isTakerStrategy) {
      from = exchangeConfig.tradeFees.applyFee(from, feeType);
    } else {
      volume = exchangeConfig.tradeFees.applyFee(volume, feeType);
    }
  }
}

PlaceOrderInfo UpbitPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  const bool placeSimulatedRealOrder = _exchangePublic.exchangeConfig().query.placeSimulateRealOrder;
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const Market mk = tradeInfo.tradeContext.market;

  const std::string_view askOrBid = fromCurrencyCode == mk.base() ? "ask" : "bid";
  const std::string_view marketOrPrice = fromCurrencyCode == mk.base() ? "market" : "price";
  const std::string_view orderType = isTakerStrategy ? marketOrPrice : "limit";

  CurlPostData placePostData{
      {"market", UpbitPublic::ReverseMarketStr(mk)}, {"side", askOrBid}, {"ord_type", orderType}};

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));

  applyFee(mk, fromCurrencyCode, isTakerStrategy, from, volume);

  MonetaryAmount sanitizedVol = UpbitPublic::SanitizeVolume(volume, price);
  const bool isSimulationWithRealOrder = tradeInfo.options.isSimulation() && placeSimulatedRealOrder;
  if (volume < sanitizedVol && !isSimulationWithRealOrder) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode,
              sanitizedVol);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  if (isTakerStrategy) {
    // Upbit has an exotic way to distinguish buy and sell on the same market
    if (fromCurrencyCode == mk.base()) {
      placePostData.emplace_back("volume", volume.amountStr());
    } else {
      placePostData.emplace_back("price", from.amountStr());
    }
  } else {
    placePostData.emplace_back("volume", volume.amountStr());
    placePostData.emplace_back("price", price.amountStr());
  }

  auto placeOrderRes = PrivateQuery<schema::upbit::V1SingleOrder>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                                  "/v1/orders", placePostData)
                           .first;

  placeOrderInfo.orderInfo = ParseOrderJson(placeOrderRes, fromCurrencyCode, mk);
  placeOrderInfo.orderId = std::move(placeOrderRes.uuid);

  // Upbit takes some time to match the market order - We should wait that it has been matched
  bool takerOrderNotClosed = isTakerStrategy && !placeOrderInfo.orderInfo.isClosed;
  while (takerOrderNotClosed) {
    auto orderRes = PrivateQuery<schema::upbit::V1SingleOrder>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order",
                                                               {{"uuid", placeOrderInfo.orderId}})
                        .first;

    placeOrderInfo.orderInfo = ParseOrderJson(orderRes, fromCurrencyCode, mk);

    takerOrderNotClosed = !placeOrderInfo.orderInfo.isClosed;
  }
  return placeOrderInfo;
}

OrderInfo UpbitPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  CurlPostData postData{{"uuid", orderId}};
  auto orderRes =
      PrivateQuery<schema::upbit::V1SingleOrder>(_curlHandle, _apiKey, HttpRequestType::kDelete, "/v1/order", postData)
          .first;
  bool cancelledOrderClosed = IsOrderClosed(orderRes.state);
  while (!cancelledOrderClosed) {
    orderRes =
        PrivateQuery<schema::upbit::V1SingleOrder>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", postData)
            .first;
    cancelledOrderClosed = IsOrderClosed(orderRes.state);
  }
  return ParseOrderJson(orderRes, tradeContext.fromCur(), tradeContext.market);
}

OrderInfo UpbitPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  auto orderRes = PrivateQuery<schema::upbit::V1SingleOrder>(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order",
                                                             {{"uuid", orderId}})
                      .first;
  const CurrencyCode fromCurrencyCode(tradeContext.fromCur());
  return ParseOrderJson(orderRes, fromCurrencyCode, tradeContext.market);
}

std::optional<MonetaryAmount> UpbitPrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  auto curStr = currencyCode.str();
  auto result = PrivateQuery<schema::upbit::V1WithdrawChance>(
                    _curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws/chance",
                    {{"currency", std::string_view{curStr}}, {"net_type", std::string_view{curStr}}})
                    .first;
  return MonetaryAmount(result.currency.withdraw_fee, currencyCode);
}

InitiatedWithdrawInfo UpbitPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  MonetaryAmount withdrawFee = _exchangePublic.queryWithdrawalFeeOrZero(currencyCode);
  MonetaryAmount netEmittedAmount = grossAmount - withdrawFee;
  CurlPostData withdrawPostData{{"currency", currencyCode.str()},
                                {"net_type", currencyCode.str()},
                                {"amount", netEmittedAmount.amountStr()},
                                {"address", destinationWallet.address()}};
  if (destinationWallet.hasTag()) {
    withdrawPostData.emplace_back("secondary_address", destinationWallet.tag());
  }

  auto result = PrivateQuery<schema::upbit::V1WithdrawsCoin>(_curlHandle, _apiKey, HttpRequestType::kPost,
                                                             "/v1/withdraws/coin", std::move(withdrawPostData))
                    .first;
  return {std::move(destinationWallet), std::move(result.uuid), grossAmount};
}

}  // namespace cct::api
