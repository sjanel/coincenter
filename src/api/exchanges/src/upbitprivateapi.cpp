#include "upbitprivateapi.hpp"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
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
#include "durationstring.hpp"
#include "exchangeconfig.hpp"
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
#include "ssl_sha.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "tradeinfo.hpp"
#include "tradeside.hpp"
#include "upbitpublicapi.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

namespace {

enum class IfError : int8_t { kThrow, kNoThrow };

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, HttpRequestType requestType, std::string_view endpoint,
                  CurlPostDataT&& curlPostData = CurlPostData(), IfError ifError = IfError::kThrow) {
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData));

  auto jsonWebToken = jwt::create()
                          .set_type("JWT")
                          .set_payload_claim("access_key", jwt::claim(std::string(apiKey.key())))
                          .set_payload_claim("nonce", jwt::claim(std::string(Nonce_TimeSinceEpochInMs())));

  if (!opts.postData().empty()) {
    const auto queryHash = ssl::Sha512Digest(opts.postData().str());

    jsonWebToken.set_payload_claim("query_hash", jwt::claim(std::string(queryHash.data(), queryHash.size())))
        .set_payload_claim("query_hash_alg", jwt::claim(std::string("SHA512")));
  }

  // hs256 does not accept std::string_view, we need a copy...
  auto token = jsonWebToken.sign(jwt::algorithm::hs256{std::string(apiKey.privateKey())});
  string authStr("Bearer ");
  authStr.append(token.begin(), token.end());

  opts.mutableHttpHeaders().emplace_back("Authorization", authStr);

  json ret = json::parse(curlHandle.query(endpoint, opts));
  if (ifError == IfError::kThrow) {
    auto errorIt = ret.find("error");
    if (errorIt != ret.end()) {
      log::error("Full Upbit json error: '{}'", ret.dump());
      if (errorIt->contains("name")) {
        throw exception(std::move((*errorIt)["name"].get_ref<string&>()));
      }
      if (errorIt->contains("message")) {
        throw exception(std::move((*errorIt)["message"].get_ref<string&>()));
      }
      throw exception("Unknown Upbit API error message");
    }
  }
  return ret;
}
}  // namespace

UpbitPrivate::UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(config, upbitPublic, apiKey),
      _curlHandle(UpbitPublic::kUrlBase, config.metricGatewayPtr(),
                  exchangeConfig().curlOptionsBuilderBase(ExchangeConfig::Api::kPrivate).build(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _apiKey, exchangeConfig(), upbitPublic._commonApi),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic) {}

bool UpbitPrivate::validateApiKey() {
  json ret =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/api_keys", CurlPostData(), IfError::kNoThrow);
  return !ret.empty() && ret.find("error") == ret.end();
}

CurrencyExchangeFlatSet UpbitPrivate::TradableCurrenciesFunc::operator()() {
  const CurrencyCodeSet& excludedCurrencies = _exchangeConfig.excludedCurrenciesAll();
  CurrencyExchangeVector currencies;
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/status/wallet");
  for (const json& curDetails : result) {
    CurrencyCode cur(curDetails["currency"].get<std::string_view>());
    CurrencyCode networkName(curDetails["net_type"].get<std::string_view>());
    if (cur != networkName) {
      log::debug("Forgive about {}-{} as net type is not the main one", cur, networkName);
      continue;
    }
    if (UpbitPublic::CheckCurrencyCode(cur, excludedCurrencies)) {
      std::string_view walletState = curDetails["wallet_state"].get<std::string_view>();
      CurrencyExchange::Withdraw withdrawStatus = CurrencyExchange::Withdraw::kUnavailable;
      CurrencyExchange::Deposit depositStatus = CurrencyExchange::Deposit::kUnavailable;
      if (walletState == "working") {
        withdrawStatus = CurrencyExchange::Withdraw::kAvailable;
        depositStatus = CurrencyExchange::Deposit::kAvailable;
      } else if (walletState == "withdraw_only") {
        withdrawStatus = CurrencyExchange::Withdraw::kAvailable;
      } else if (walletState == "deposit_only") {
        depositStatus = CurrencyExchange::Deposit::kAvailable;
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

  json ret = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/accounts");

  balancePortfolio.reserve(static_cast<BalancePortfolio::size_type>(ret.size()));

  for (const json& accountDetail : ret) {
    const CurrencyCode currencyCode(accountDetail["currency"].get<std::string_view>());
    MonetaryAmount availableAmount(accountDetail["balance"].get<std::string_view>(), currencyCode);

    if (withBalanceInUse) {
      availableAmount += MonetaryAmount(accountDetail["locked"].get<std::string_view>(), currencyCode);
    }

    balancePortfolio += availableAmount;
  }
  return balancePortfolio;
}

Wallet UpbitPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurlPostData postData{{"currency", currencyCode.str()}, {"net_type", currencyCode.str()}};
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits/coin_address", postData,
                             IfError::kNoThrow);
  bool generateDepositAddressNeeded = false;
  auto errorIt = result.find("error");
  if (errorIt != result.end()) {
    std::string_view name = (*errorIt)["name"].get<std::string_view>();
    std::string_view msg = (*errorIt)["message"].get<std::string_view>();
    if (name == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one", currencyCode);
      generateDepositAddressNeeded = true;
    } else {
      throw exception("Upbit error: {}, msg: {}", name, msg);
    }
  }
  if (generateDepositAddressNeeded) {
    json genCoinAddressResult =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/deposits/generate_coin_address", postData);
    if (genCoinAddressResult.contains("success")) {
      log::info("Successfully generated address");
    }
    seconds sleepingTime(1);
    static constexpr int kNbMaxRetries = 15;
    int nbRetries = 0;
    do {
      if (nbRetries > 0) {
        log::info("Waiting {} for address to be generated...", DurationToString(sleepingTime));
      }
      std::this_thread::sleep_for(sleepingTime);
      result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits/coin_address", postData);
      sleepingTime += seconds(1);
      ++nbRetries;
    } while (nbRetries < kNbMaxRetries && result["deposit_address"].is_null());
  }
  auto addressIt = result.find("deposit_address");
  if (addressIt == result.end() || addressIt->is_null()) {
    throw exception("Deposit address for {} is undefined", currencyCode);
  }
  std::string_view tag;
  auto secondaryAddressIt = result.find("secondary_address");
  if (secondaryAddressIt != result.end() && !secondaryAddressIt->is_null()) {
    tag = secondaryAddressIt->get<std::string_view>();
  }

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeConfig(_exchangePublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  Wallet wallet(ExchangeName(_exchangePublic.name(), _apiKey.name()), currencyCode,
                std::move(addressIt->get_ref<string&>()), tag, walletCheck, _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
template <class OrderVectorType>
void FillOrders(const OrdersConstraints& ordersConstraints, CurlHandle& curlHandle, const APIKey& apiKey,
                ExchangePublic& exchangePublic, OrderVectorType& orderVector) {
  using OrderType = std::remove_cvref_t<decltype(*std::declval<OrderVectorType>().begin())>;

  int page = 0;

  CurlPostData params{{"page", page}, {"state", std::is_same_v<OrderType, OpenedOrder> ? "wait" : "done"}};

  if (ordersConstraints.isCurDefined()) {
    MarketSet markets;
    Market filterMarket =
        exchangePublic.determineMarketFromFilterCurrencies(markets, ordersConstraints.cur1(), ordersConstraints.cur2());

    if (filterMarket.isDefined()) {
      params.emplace_back("market", UpbitPublic::ReverseMarketStr(filterMarket));
    }
  }

  static constexpr auto kMaxNbOrdersPerPage = 100;
  static constexpr auto kNbMaxPagesToRetrieve = 10;

  for (auto nbOrdersRetrieved = kMaxNbOrdersPerPage;
       nbOrdersRetrieved == kMaxNbOrdersPerPage && page < kNbMaxPagesToRetrieve;) {
    params.set("page", ++page);

    json data = PrivateQuery(curlHandle, apiKey, HttpRequestType::kGet, "/v1/orders", params);

    nbOrdersRetrieved = static_cast<decltype(nbOrdersRetrieved)>(data.size());

    for (json& orderDetails : data) {
      std::string_view marketStr = orderDetails["market"].get<std::string_view>();
      auto dashPos = marketStr.find('-');
      if (dashPos == std::string_view::npos) {
        throw exception("Expected a dash in {} for {} orders query", marketStr, exchangePublic.name());
      }
      CurrencyCode priceCur(std::string_view(marketStr.data(), dashPos));
      CurrencyCode volumeCur(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()));

      if (!ordersConstraints.validateCur(volumeCur, priceCur)) {
        continue;
      }

      // 'created_at' string is in this format: "2019-01-04T13:48:09+09:00"
      TimePoint placedTime =
          FromString(orderDetails["created_at"].get_ref<const string&>().c_str(), kTimeYearToSecondTSeparatedFormat);
      if (!ordersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      string id = std::move(orderDetails["uuid"].get_ref<string&>());
      if (!ordersConstraints.validateId(id)) {
        continue;
      }

      auto priceIt = orderDetails.find("price");
      if (priceIt == orderDetails.end()) {
        // Some old orders may have no price field set. In this case, just return what we have as the older orders will
        // probably not be filled as well.
        break;
      }

      MonetaryAmount matchedVolume(orderDetails["executed_volume"].get<std::string_view>(), volumeCur);
      MonetaryAmount price(priceIt->get<std::string_view>(), priceCur);
      TradeSide side = orderDetails["side"].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

      if constexpr (std::is_same_v<OrderType, OpenedOrder>) {
        MonetaryAmount remainingVolume(orderDetails["remaining_volume"].get<std::string_view>(), volumeCur);

        orderVector.emplace_back(std::move(id), matchedVolume, remainingVolume, price, placedTime, side);
      } else if constexpr (std::is_same_v<OrderType, ClosedOrder>) {
        const TimePoint matchedTime = placedTime;

        orderVector.emplace_back(std::move(id), matchedVolume, price, placedTime, matchedTime, side);
      } else {
        // Note: below ugly template lambda can be replaced with 'static_assert(false);' in C++23
        []<bool flag = false>() { static_assert(flag, "no match"); }
        ();
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
Deposit::Status DepositStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "PROCESSING" || statusStr == "REFUNDING") {
    return Deposit::Status::kProcessing;
  }
  if (statusStr == "ACCEPTED") {
    return Deposit::Status::kSuccess;
  }
  if (statusStr == "CANCELLED" || statusStr == "REJECTED" || statusStr == "TRAVEL_RULE_SUSPECTED" ||
      statusStr == "REFUNDED") {
    return Deposit::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized deposit status '{}' from Upbit", statusStr);
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
    json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/deposits", options);
    if (deposits.empty()) {
      deposits.reserve(static_cast<Deposits::size_type>(result.size()));
    }
    nbResults = static_cast<int>(result.size());
    for (json& trx : result) {
      CurrencyCode currencyCode(trx["currency"].get<std::string_view>());
      MonetaryAmount amount(trx["amount"].get<std::string_view>(), currencyCode);
      // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
      auto timeIt = trx.find("done_at");
      if (timeIt == trx.end() || timeIt->is_null()) {
        // It can be the case for deposits that failed - take the start time instead of the end time in this case
        timeIt = trx.find("created_at");
      }

      TimePoint timestamp = FromString(timeIt->get<std::string_view>(), kTimeYearToSecondTSeparatedFormat);
      if (!depositsConstraints.validateTime(timestamp)) {
        continue;
      }
      string& id = trx["txid"].get_ref<string&>();

      std::string_view statusStr = trx["state"].get<std::string_view>();
      Deposit::Status status = DepositStatusFromStatusStr(statusStr);

      deposits.emplace_back(std::move(id), timestamp, amount, status);
    }
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "WAITING") {
    return Withdraw::Status::kInitial;
  }
  if (statusStr == "PROCESSING") {
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "DONE") {
    return Withdraw::Status::kSuccess;
  }
  // In earlier versions of Upbit API, 'CANCELED' was written with this typo.
  // Let's support both spellings to avoid issues.
  if (statusStr == "FAILED" || statusStr == "CANCELLED" || statusStr == "CANCELED" || statusStr == "REJECTED") {
    return Withdraw::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized withdraw status '{}' from Upbit", statusStr);
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
    json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws", options);
    if (withdraws.empty()) {
      withdraws.reserve(static_cast<Withdraws::size_type>(result.size()));
    }
    nbResults = static_cast<int>(result.size());
    for (json& trx : result) {
      CurrencyCode currencyCode(trx["currency"].get<std::string_view>());
      MonetaryAmount netEmittedAmount(trx["amount"].get<std::string_view>(), currencyCode);
      MonetaryAmount withdrawFee(trx["fee"].get<std::string_view>(), currencyCode);
      // 'done_at' string is in this format: "2019-01-04T13:48:09+09:00"
      auto timeIt = trx.find("done_at");
      if (timeIt == trx.end() || timeIt->is_null()) {
        // It can be the case for withdraws that failed - take the start time instead of the end time in this case
        timeIt = trx.find("created_at");
      }
      TimePoint timestamp = FromString(timeIt->get<std::string_view>(), kTimeYearToSecondTSeparatedFormat);
      if (!withdrawsConstraints.validateTime(timestamp)) {
        continue;
      }
      string& id = trx["txid"].get_ref<string&>();

      std::string_view statusStr = trx["state"].get<std::string_view>();
      Withdraw::Status status = WithdrawStatusFromStatusStr(statusStr);

      withdraws.emplace_back(std::move(id), timestamp, netEmittedAmount, status, withdrawFee);
    }
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

namespace {
bool IsOrderClosed(const json& orderJson) {
  std::string_view state = orderJson["state"].get<std::string_view>();
  if (state == "done" || state == "cancel") {
    return true;
  }
  if (state == "wait" || state == "watch") {
    return false;
  }
  log::error("Unknown state {} to be handled for Upbit", state);
  return true;
}

OrderInfo ParseOrderJson(const json& orderJson, CurrencyCode fromCurrencyCode, Market mk) {
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, fromCurrencyCode == mk.base() ? mk.quote() : mk.base()),
                      IsOrderClosed(orderJson));

  if (orderJson.contains("trades")) {
    // TODO: to be confirmed (this is true at least for markets involving KRW)
    CurrencyCode feeCurrencyCode(mk.quote());
    MonetaryAmount fee(orderJson["paid_fee"].get<std::string_view>(), feeCurrencyCode);

    for (const json& orderDetails : orderJson["trades"]) {
      MonetaryAmount tradedVol(orderDetails["volume"].get<std::string_view>(), mk.base());  // always in base currency
      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), mk.quote());      // always in quote currency
      MonetaryAmount tradedCost(orderDetails["funds"].get<std::string_view>(), mk.quote());

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
    ExchangeConfig::FeeType feeType =
        isTakerStrategy ? ExchangeConfig::FeeType::kTaker : ExchangeConfig::FeeType::kMaker;
    const ExchangeConfig& exchangeConfig = _coincenterInfo.exchangeConfig(_exchangePublic.name());
    if (isTakerStrategy) {
      from = exchangeConfig.applyFee(from, feeType);
    } else {
      volume = exchangeConfig.applyFee(volume, feeType);
    }
  }
}

PlaceOrderInfo UpbitPrivate::placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  const bool placeSimulatedRealOrder = _exchangePublic.exchangeConfig().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const Market mk = tradeInfo.tradeContext.mk;

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

  json placeOrderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/orders", placePostData);

  placeOrderInfo.orderInfo = ParseOrderJson(placeOrderRes, fromCurrencyCode, mk);
  placeOrderInfo.orderId = std::move(placeOrderRes["uuid"].get_ref<string&>());

  // Upbit takes some time to match the market order - We should wait that it has been matched
  bool takerOrderNotClosed = isTakerStrategy && !placeOrderInfo.orderInfo.isClosed;
  while (takerOrderNotClosed) {
    json orderRes =
        PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", {{"uuid", placeOrderInfo.orderId}});

    placeOrderInfo.orderInfo = ParseOrderJson(orderRes, fromCurrencyCode, mk);

    takerOrderNotClosed = !placeOrderInfo.orderInfo.isClosed;
  }
  return placeOrderInfo;
}

OrderInfo UpbitPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  CurlPostData postData{{"uuid", orderId}};
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kDelete, "/v1/order", postData);
  bool cancelledOrderClosed = IsOrderClosed(orderRes);
  while (!cancelledOrderClosed) {
    orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", postData);
    cancelledOrderClosed = IsOrderClosed(orderRes);
  }
  return ParseOrderJson(orderRes, tradeContext.fromCur(), tradeContext.mk);
}

OrderInfo UpbitPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) {
  json orderRes = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/order", {{"uuid", orderId}});
  const CurrencyCode fromCurrencyCode(tradeContext.fromCur());
  return ParseOrderJson(orderRes, fromCurrencyCode, tradeContext.mk);
}

std::optional<MonetaryAmount> UpbitPrivate::WithdrawFeesFunc::operator()(CurrencyCode currencyCode) {
  auto curStr = currencyCode.str();
  json result = PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kGet, "/v1/withdraws/chance",
                             {{"currency", std::string_view{curStr}}, {"net_type", std::string_view{curStr}}});
  std::string_view amountStr = result["currency"]["withdraw_fee"].get<std::string_view>();
  return MonetaryAmount(amountStr, currencyCode);
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

  json result =
      PrivateQuery(_curlHandle, _apiKey, HttpRequestType::kPost, "/v1/withdraws/coin", std::move(withdrawPostData));
  return {std::move(destinationWallet), std::move(result["uuid"].get_ref<string&>()), grossAmount};
}

}  // namespace cct::api
