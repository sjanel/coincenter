#include "krakenprivateapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "apikey.hpp"
#include "apiquerytypeenum.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "base64.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangeconfig.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "krakenpublicapi.hpp"
#include "market.hpp"
#include "monetary-amount-vector.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "orderid.hpp"
#include "ordersconstraints.hpp"
#include "permanentcurloptions.hpp"
#include "query-retry-policy.hpp"
#include "request-retry.hpp"
#include "ssl_sha.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "toupperlower.hpp"
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

enum class KrakenErrorEnum : int8_t { kExpiredOrder, kUnknownWithdrawKey, kUnknownError, kNoError };

template <class CurlPostDataT = CurlPostData>
auto PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view method,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  CurlOptions opts(HttpRequestType::kPost, std::forward<CurlPostDataT>(curlPostData));
  opts.mutableHttpHeaders().emplace_back("API-Key", apiKey.key());

  RequestRetry requestRetry(curlHandle, std::move(opts),
                            QueryRetryPolicy{.initialRetryDelay = seconds{1}, .nbMaxRetries = 3});

  static constexpr std::string_view kErrorKey = "error";

  KrakenErrorEnum err = KrakenErrorEnum::kNoError;

  json::container ret = requestRetry.queryJson(
      method,
      [&err](const json::container& jsonResponse) {
        const auto errorIt = jsonResponse.find(kErrorKey);
        if (errorIt != jsonResponse.end() && !errorIt->empty()) {
          std::string_view msg = errorIt->front().get<std::string_view>();
          if (msg == "EAPI:Rate limit exceeded") {
            log::warn("kraken private API rate limit exceeded");
            return RequestRetry::Status::kResponseError;
          }
          if (msg.ends_with("Unknown order")) {
            err = KrakenErrorEnum::kExpiredOrder;
            return RequestRetry::Status::kResponseOK;
          }
          if (msg.ends_with("Unknown withdraw key")) {
            err = KrakenErrorEnum::kUnknownWithdrawKey;
            return RequestRetry::Status::kResponseOK;
          }
          log::error("kraken unknown error {}", msg);
          return RequestRetry::Status::kResponseError;
        }
        return RequestRetry::Status::kResponseOK;
      },
      [&apiKey, method](CurlOptions& opts) {
        Nonce noncePostData = Nonce_TimeSinceEpochInMs();
        opts.mutablePostData().set("nonce", noncePostData);

        // concatenate nonce and postdata and compute SHA256
        noncePostData.append(opts.postData().str());

        // concatenate path and nonce_postdata (path + ComputeSha256(nonce + postdata))
        auto sha256 = ssl::Sha256(noncePostData);

        string path;
        path.reserve(KrakenPublic::kVersion.size() + method.size() + sha256.size());
        path.append(KrakenPublic::kVersion).append(method).append(sha256.data(), sha256.data() + sha256.size());

        static constexpr std::string_view kSignatureKey = "API-Sign";

        // and compute HMAC
        opts.mutableHttpHeaders().set_back(kSignatureKey,
                                           B64Encode(ssl::Sha512Bin(path, B64Decode(apiKey.privateKey()))));
      });

  auto resultIt = ret.find("result");
  std::pair<json::container, KrakenErrorEnum> retPair(json::container::object_t{}, err);
  if (resultIt != ret.end()) {
    retPair.first = std::move(*resultIt);
  }
  return retPair;
}
}  // namespace

KrakenPrivate::KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey)
    : ExchangePrivate(config, krakenPublic, apiKey),
      _curlHandle(KrakenPublic::kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  config.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, krakenPublic) {}

bool KrakenPrivate::validateApiKey() {
  return PrivateQuery(_curlHandle, _apiKey, "/private/Balance").second == KrakenErrorEnum::kNoError;
}

CurrencyExchangeFlatSet KrakenPrivate::queryTradableCurrencies() { return _exchangePublic.queryTradableCurrencies(); }

BalancePortfolio KrakenPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  BalancePortfolio balancePortfolio;
  auto [res, err] = PrivateQuery(_curlHandle, _apiKey, "/private/Balance");
  // Kraken returns an empty array in case of account with no balance at all
  MonetaryAmountVector balanceAmounts;
  balanceAmounts.reserve(static_cast<MonetaryAmountVector::size_type>(res.size()));
  for (const auto& [curCode, amountStr] : res.items()) {
    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(curCode));

    balanceAmounts.emplace_back(amountStr.get<std::string_view>(), currencyCode);
  }
  const auto compByCurrency = [](MonetaryAmount lhs, MonetaryAmount rhs) {
    return lhs.currencyCode() < rhs.currencyCode();
  };
  std::ranges::sort(balanceAmounts, compByCurrency);

  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;

  // Kraken returns total balance, including the amounts in use
  if (!withBalanceInUse) {
    // We need to query the opened orders to remove the balance in use
    for (const OpenedOrder& order : queryOpenedOrders()) {
      MonetaryAmount remVolume = order.remainingVolume();
      switch (order.side()) {
        case TradeSide::kBuy: {
          MonetaryAmount price = order.price();
          auto lb = std::ranges::lower_bound(balanceAmounts, price, compByCurrency);
          if (lb != balanceAmounts.end() && lb->currencyCode() == price.currencyCode()) {
            *lb -= remVolume.toNeutral() * price;
          } else {
            log::error("Was expecting at least {} in Kraken balance", remVolume.toNeutral() * price);
          }
          break;
        }
        case TradeSide::kSell: {
          auto lb = std::ranges::lower_bound(balanceAmounts, remVolume, compByCurrency);
          if (lb != balanceAmounts.end() && lb->currencyCode() == remVolume.currencyCode()) {
            *lb -= remVolume;
          } else {
            log::error("Was expecting at least {} in Kraken balance", remVolume);
          }
          break;
        }
        default:
          throw exception("unknown trade side");
      }
    }
  }
  std::ranges::for_each(balanceAmounts, [&balancePortfolio](const auto amt) { balancePortfolio += amt; });
  return balancePortfolio;
}

Wallet KrakenPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  auto [res, err] = PrivateQuery(_curlHandle, _apiKey, "/private/DepositMethods", {{"asset", krakenCurrency.altStr()}});
  ExchangeName eName(_exchangePublic.name(), _apiKey.name());
  if (res.empty()) {
    throw exception("No deposit method found on {} for {}", eName, currencyCode);
  }
  // Don't keep a view on 'method' value, we will override json data just below. We can just steal the string.
  string method = std::move(res.front()["method"].get_ref<string&>());
  std::tie(res, err) = PrivateQuery(_curlHandle, _apiKey, "/private/DepositAddresses",
                                    {{"asset", krakenCurrency.altStr()}, {"method", method}});
  if (res.empty()) {
    // This means user has not created a wallet yet, but it's possible to do it via DepositMethods query above.
    log::warn("No deposit address found on {} for {}, creating a new one", eName, currencyCode);
    std::tie(res, err) = PrivateQuery(_curlHandle, _apiKey, "/private/DepositAddresses",
                                      {{"asset", krakenCurrency.altStr()}, {"method", method}, {"new", "true"}});
    if (res.empty()) {
      throw exception("Cannot create a new deposit address on {} for {}", eName, currencyCode);
    }
  }

  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  bool doCheckWallet = coincenterInfo.exchangeConfig(_exchangePublic.name()).validateDepositAddressesInFile();
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);
  string address;
  string tag;
  for (const json::container& depositDetail : res) {
    for (const auto& [keyStr, valueStr] : depositDetail.items()) {
      if (keyStr == "address") {
        address = valueStr;
      } else if (keyStr == "expiretm") {
        if (valueStr.is_number_integer()) {  // WARNING: when new = true, expiretm is not a string, but a number!
          int64_t expireTmValue = valueStr.get<int64_t>();
          if (expireTmValue != 0) {
            log::warn("{} wallet has an expire time of {}", eName, expireTmValue);
          }
        } else if (valueStr.is_string()) {
          std::string_view expireTmValue = valueStr.get<std::string_view>();
          if (expireTmValue != "0") {
            log::warn("{} wallet has an expire time of {}", eName, expireTmValue);
          }
        } else {
          throw exception("Cannot retrieve 'expiretm' field of {} deposit address", eName);
        }

      } else if (keyStr == "new") {
        // Never used, it's ok, safely pass this
      } else {
        // Heuristic: this last field may change key name and is optional (tag for XRP, memo for EOS for instance)
        if (!tag.empty()) {
          throw exception("Tag already set / unknown key information for {}", currencyCode);
        }
        if (valueStr.is_number_integer()) {
          tag = IntegralToString(static_cast<long>(valueStr));
        } else {
          tag = valueStr.get<string>();
        }
      }
    }
    if (Wallet::ValidateWallet(walletCheck, eName, currencyCode, address, tag)) {
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
    address.clear();
    tag.clear();
  }

  Wallet wallet(std::move(eName), currencyCode, std::move(address), std::move(tag), walletCheck,
                _apiKey.accountOwner());
  log::info("Retrieved {}", wallet);
  return wallet;
}

namespace {
TimePoint TimePointFromKrakenTime(double seconds) {
  int64_t millisecondsSinceEpoch = static_cast<int64_t>(1000 * seconds);
  return TimePoint{milliseconds(millisecondsSinceEpoch)};
}
}  // namespace

ClosedOrderVector KrakenPrivate::queryClosedOrders(const OrdersConstraints& closedOrdersConstraints) {
  ClosedOrderVector closedOrders;

  int page = 0;

  CurlPostData params{{"ofs", page}, {"trades", "true"}};

  if (closedOrdersConstraints.isPlacedTimeAfterDefined()) {
    params.emplace_back("start", TimestampToSecondsSinceEpoch(closedOrdersConstraints.placedAfter()));
  }
  if (closedOrdersConstraints.isPlacedTimeBeforeDefined()) {
    params.emplace_back("end", TimestampToSecondsSinceEpoch(closedOrdersConstraints.placedBefore()));
  }

  static constexpr int kLimitNbOrdersPerPage = 50;
  static constexpr int kNbMaxPagesToRetrieve = 10;

  MarketSet markets;

  for (int nbOrdersRetrieved = kLimitNbOrdersPerPage;
       nbOrdersRetrieved == kLimitNbOrdersPerPage && page < kNbMaxPagesToRetrieve; ++page) {
    params.set("ofs", page);

    auto [data, err] = PrivateQuery(_curlHandle, _apiKey, "/private/ClosedOrders", params);

    const auto closedItOrders = data.find("closed");
    if (closedItOrders == data.end()) {
      throw exception("Unexpected reply from {} for closed orders query", _exchangePublic.name());
    }

    nbOrdersRetrieved = 0;

    for (const auto& [orderId, orderDetails] : closedItOrders->items()) {
      ++nbOrdersRetrieved;
      if (!closedOrdersConstraints.validateId(orderId)) {
        continue;
      }
      const json::container& descrPart = orderDetails["descr"];
      std::string_view marketStr = descrPart["pair"].get<std::string_view>();

      std::optional<Market> optMarket =
          _exchangePublic.determineMarketFromMarketStr(marketStr, markets, closedOrdersConstraints.cur1());

      if (!optMarket) {
        continue;
      }

      CurrencyCode volumeCur = optMarket->base();
      CurrencyCode priceCur = optMarket->quote();
      if (!closedOrdersConstraints.validateCur(volumeCur, priceCur)) {
        continue;
      }

      MonetaryAmount matchedVolume(orderDetails["vol_exec"].get<std::string_view>(), volumeCur);
      if (matchedVolume == 0) {
        continue;
      }

      MonetaryAmount price(orderDetails["price"].get<std::string_view>(), priceCur);
      TimePoint placedTime = TimePointFromKrakenTime(orderDetails["opentm"].get<double>());
      if (!closedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      TimePoint matchedTime = TimePointFromKrakenTime(orderDetails["closetm"].get<double>());

      TradeSide side = descrPart["type"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

      closedOrders.emplace_back(orderId, matchedVolume, price, placedTime, matchedTime, side);
    }
  }

  if (page == kNbMaxPagesToRetrieve) {
    log::warn("Already queried {} order pages, stop the queries at this point", page);
    log::warn("Try to refine the orders query by specifying the market and / or the time window");
  }

  std::ranges::sort(closedOrders);
  log::info("Retrieved {} closed orders from {}", closedOrders.size(), _exchangePublic.name());
  return closedOrders;
}

OpenedOrderVector KrakenPrivate::queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  auto [res, err] = PrivateQuery(_curlHandle, _apiKey, "/private/OpenOrders", {{"trades", "true"}});
  auto openedPartIt = res.find("open");
  OpenedOrderVector openedOrders;
  if (openedPartIt != res.end()) {
    MarketSet markets;

    for (const auto& [id, orderDetails] : openedPartIt->items()) {
      const json::container& descrPart = orderDetails["descr"];
      std::string_view marketStr = descrPart["pair"].get<std::string_view>();

      std::optional<Market> optMarket =
          _exchangePublic.determineMarketFromMarketStr(marketStr, markets, openedOrdersConstraints.cur1());

      if (!optMarket) {
        continue;
      }

      CurrencyCode volumeCur = optMarket->base();
      CurrencyCode priceCur = optMarket->quote();
      if (!openedOrdersConstraints.validateCur(volumeCur, priceCur)) {
        continue;
      }

      if (!openedOrdersConstraints.validateId(id)) {
        continue;
      }

      MonetaryAmount originalVolume(orderDetails["vol"].get<std::string_view>(), volumeCur);
      MonetaryAmount matchedVolume(orderDetails["vol_exec"].get<std::string_view>(), volumeCur);
      MonetaryAmount remainingVolume = originalVolume - matchedVolume;
      MonetaryAmount price(descrPart["price"].get<std::string_view>(), priceCur);
      TradeSide side = descrPart["type"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

      TimePoint placedTime = TimePointFromKrakenTime(orderDetails["opentm"].get<double>());
      if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      openedOrders.emplace_back(id, matchedVolume, remainingVolume, price, placedTime, side);
    }
  }
  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), exchangeName());
  return openedOrders;
}

int KrakenPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.noConstraints()) {
    auto [cancelledOrders, err] = PrivateQuery(_curlHandle, _apiKey, "/private/CancelAll");
    return cancelledOrders["count"].get<int>();
  }
  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const OpenedOrder& order : openedOrders) {
    cancelOrderProcess(order.id());
  }
  return openedOrders.size();
}

namespace {
Deposit::Status DepositStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "Settled") {
    return Deposit::Status::kProcessing;
  }
  if (statusStr == "Success") {
    return Deposit::Status::kSuccess;
  }
  if (statusStr == "Failure") {
    return Deposit::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized deposit status '{}' from Kraken", statusStr);
}
}  // namespace

DepositsSet KrakenPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.emplace_back("asset", depositsConstraints.currencyCode().str());
  }
  auto [res, err] = PrivateQuery(_curlHandle, _apiKey, "/private/DepositStatus", options);
  for (const json::container& trx : res) {
    auto additionalNoteIt = trx.find("status-prop");
    if (additionalNoteIt != trx.end()) {
      std::string_view statusNote(additionalNoteIt->get<std::string_view>());
      if (statusNote == "onhold") {
        log::debug("Additional status is {}", statusNote);
      }
    }
    std::string_view statusStr(trx["status"].get<std::string_view>());
    Deposit::Status status = DepositStatusFromStatusStr(statusStr);

    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(trx["asset"].get<std::string_view>()));
    MonetaryAmount amount(trx["amount"].get<std::string_view>(), currencyCode);
    int64_t secondsSinceEpoch = trx["time"].get<int64_t>();
    std::string_view id = trx["txid"].get<std::string_view>();
    TimePoint timestamp{seconds(secondsSinceEpoch)};

    if (!depositsConstraints.validateTime(timestamp)) {
      continue;
    }
    if (!depositsConstraints.validateId(id)) {
      continue;
    }

    deposits.emplace_back(id, timestamp, amount, status);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "Initial" || statusStr == "Pending") {
    return Withdraw::Status::kInitial;
  }
  if (statusStr == "Settled" || statusStr == "On hold") {
    return Withdraw::Status::kProcessing;
  }
  if (statusStr == "Success") {
    return Withdraw::Status::kSuccess;
  }
  if (statusStr == "Failure") {
    return Withdraw::Status::kFailureOrRejected;
  }
  throw exception("Unrecognized withdraw status '{}' from Kraken", statusStr);
}

CurlPostData CreateOptionsFromWithdrawConstraints(const WithdrawsConstraints& withdrawsConstraints) {
  CurlPostData options;
  if (withdrawsConstraints.isCurDefined()) {
    options.emplace_back("asset", withdrawsConstraints.currencyCode().str());
  }
  return options;
}
}  // namespace

WithdrawsSet KrakenPrivate::queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints) {
  Withdraws withdraws;
  auto [res, err] = PrivateQuery(_curlHandle, _apiKey, "/private/WithdrawStatus",
                                 CreateOptionsFromWithdrawConstraints(withdrawsConstraints));
  for (const json::container& trx : res) {
    int64_t secondsSinceEpoch = trx["time"].get<int64_t>();
    TimePoint timestamp{seconds(secondsSinceEpoch)};
    if (!withdrawsConstraints.validateTime(timestamp)) {
      continue;
    }

    std::string_view id = trx["refid"].get<std::string_view>();
    if (!withdrawsConstraints.validateId(id)) {
      continue;
    }

    std::string_view statusStr(trx["status"].get<std::string_view>());
    Withdraw::Status status = WithdrawStatusFromStatusStr(statusStr);

    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(trx["asset"].get<std::string_view>()));
    MonetaryAmount netEmittedAmount(trx["amount"].get<std::string_view>(), currencyCode);
    MonetaryAmount fee(trx["fee"].get<std::string_view>(), currencyCode);

    withdraws.emplace_back(id, timestamp, netEmittedAmount, status, fee);
  }
  WithdrawsSet withdrawsSet(std::move(withdraws));
  log::info("Retrieved {} recent withdraws for {}", withdrawsSet.size(), exchangeName());
  return withdrawsSet;
}

PlaceOrderInfo KrakenPrivate::placeOrder([[maybe_unused]] MonetaryAmount from, MonetaryAmount volume,
                                         MonetaryAmount price, const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.tradeContext.fromCur());
  const CurrencyCode toCurrencyCode(tradeInfo.tradeContext.toCur());
  const bool isTakerStrategy =
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeConfig().placeSimulateRealOrder());
  const bool isSimulation = tradeInfo.options.isSimulation();
  const Market mk = tradeInfo.tradeContext.mk;
  KrakenPublic& krakenPublic = dynamic_cast<KrakenPublic&>(_exchangePublic);
  const MonetaryAmount orderMin = krakenPublic.queryVolumeOrderMin(mk);
  CurrencyExchange krakenCurrencyBase = _exchangePublic.convertStdCurrencyToCurrencyExchange(mk.base());
  CurrencyExchange krakenCurrencyQuote = _exchangePublic.convertStdCurrencyToCurrencyExchange(mk.quote());
  Market krakenMarket(krakenCurrencyBase.altCode(), krakenCurrencyQuote.altCode());
  const std::string_view orderType = fromCurrencyCode == mk.base() ? "sell" : "buy";

  auto volAndPriNbDecimals = krakenPublic._marketsCache.get().second.find(mk)->second.volAndPriNbDecimals;

  price.truncate(volAndPriNbDecimals.priNbDecimals);

  // volume in quote currency (viqc) is not available (as of March 2021), receiving error 'EAPI:Feature disabled:viqc'
  // We have to compute the amount manually (always in base currency)
  volume.truncate(volAndPriNbDecimals.volNbDecimals);

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)), OrderId("UndefinedId"));
  if (volume < orderMin) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume, toCurrencyCode, orderMin);
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  // minimum expire time tested on my side was 5 seconds. I chose 10 seconds just to be sure that we will not have any
  // problem.
  const int maxTradeTimeInSeconds =
      static_cast<int>(std::chrono::duration_cast<seconds>(tradeInfo.options.maxTradeTime()).count());
  const int expireTimeInSeconds = std::max(10, maxTradeTimeInSeconds);

  const auto nbSecondsSinceEpoch = TimestampToSecondsSinceEpoch(Clock::now());

  // oflags: Ask fee in destination currency.
  // This will not work if user has enough Kraken Fee Credits (in this case, they will be used instead).
  // Warning: this does not change the currency of the returned fee from Kraken in the get Closed / Opened orders,
  // which will be always in quote currency (as per the documentation)
  CurlPostData placePostData{{"pair", krakenMarket.assetsPairStrUpper()},
                             {"type", orderType},
                             {"ordertype", isTakerStrategy ? "market" : "limit"},
                             {"price", price.amountStr()},
                             {"volume", volume.amountStr()},
                             {"oflags", fromCurrencyCode == mk.quote() ? "fcib" : "fciq"},
                             {"expiretm", nbSecondsSinceEpoch + expireTimeInSeconds},
                             {"userref", tradeInfo.tradeContext.userRef}};
  if (isSimulation) {
    placePostData.emplace_back("validate", "true");  // validate inputs only. do not submit order (optional)
  }

  auto [placeOrderRes, err] = PrivateQuery(_curlHandle, _apiKey, "/private/AddOrder", std::move(placePostData));
  // {"error":[],"result":{"descr":{"order":"buy 24.69898116 XRPETH @ limit 0.0003239"},"txid":["OWBA44-TQZQ7-EEYSXA"]}}
  if (isSimulation) {
    // In simulation mode, there is no txid returned. If we arrived here (after CollectResults) we assume that the call
    // to api was a success.
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  placeOrderInfo.orderId = std::move(placeOrderRes["txid"].front().get_ref<string&>());

  // Kraken will automatically truncate the decimals to the maximum allowed for the trade assets. Get this information
  // and adjust our amount.
  std::string_view orderDescriptionStr = placeOrderRes["descr"]["order"].get<std::string_view>();
  std::string_view krakenTruncatedAmount(
      orderDescriptionStr.begin() + orderType.size() + 1,
      orderDescriptionStr.begin() + orderDescriptionStr.find(' ', orderType.size() + 1));
  MonetaryAmount krakenVolume(krakenTruncatedAmount, mk.base());
  log::debug("Kraken adjusted volume: {}", krakenVolume);

  placeOrderInfo.orderInfo =
      queryOrderInfo(placeOrderInfo.orderId, tradeInfo.tradeContext,
                     isTakerStrategy ? QueryOrder::kClosedThenOpened : QueryOrder::kOpenedThenClosed);

  return placeOrderInfo;
}

OrderInfo KrakenPrivate::cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) {
  cancelOrderProcess(orderId);
  return queryOrderInfo(orderId, tradeContext, QueryOrder::kClosedThenOpened);
}

void KrakenPrivate::cancelOrderProcess(OrderIdView orderId) {
  auto [response, err] = PrivateQuery(_curlHandle, _apiKey, "/private/CancelOrder", {{"txid", orderId}});
  if (err == KrakenErrorEnum::kExpiredOrder) {
    log::warn("{} is unable to find order {} - it has probably expired or been matched", exchangeName(), orderId);
  }
}

OrderInfo KrakenPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext, QueryOrder queryOrder) {
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();
  const Market mk = tradeContext.mk;

  json::container ordersRes = queryOrdersData(tradeContext.userRef, orderId, queryOrder);
  auto openIt = ordersRes.find("open");
  const bool orderInOpenedPart = openIt != ordersRes.end() && openIt->contains(orderId);
  const json::container& orderJson = orderInOpenedPart ? (*openIt)[orderId] : ordersRes["closed"][orderId];
  MonetaryAmount vol(orderJson["vol"].get<std::string_view>(), mk.base());             // always in base currency
  MonetaryAmount tradedVol(orderJson["vol_exec"].get<std::string_view>(), mk.base());  // always in base currency
  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode), !orderInOpenedPart);
  // Avoid division by 0 as the price is returned as 0.
  if (tradedVol != 0) {
    MonetaryAmount tradedCost(orderJson["cost"].get<std::string_view>(), mk.quote());  // always in quote currency
    MonetaryAmount fee(orderJson["fee"].get<std::string_view>(), mk.quote());          // always in quote currency

    if (fromCurrencyCode == mk.quote()) {
      MonetaryAmount price(orderJson["price"].get<std::string_view>(), mk.base());
      orderInfo.tradedAmounts.from += tradedCost;
      orderInfo.tradedAmounts.to += (tradedCost - fee).toNeutral() / price;
    } else {
      orderInfo.tradedAmounts.from += tradedVol;
      orderInfo.tradedAmounts.to += tradedCost - fee;
    }
  }

  return orderInfo;
}

json::container KrakenPrivate::queryOrdersData(int64_t userRef, OrderIdView orderId, QueryOrder queryOrder) {
  static constexpr int kNbMaxRetriesQueryOrders = 10;
  int nbRetries = 0;
  CurlPostData ordersPostData{{"trades", "true"}, {"userref", userRef}};
  const bool isOpenedFirst = queryOrder == QueryOrder::kOpenedThenClosed;
  const std::string_view firstQueryFullName = isOpenedFirst ? "/private/OpenOrders" : "/private/ClosedOrders";
  do {
    auto [data, err] = PrivateQuery(_curlHandle, _apiKey, firstQueryFullName, ordersPostData);
    const json::container& firstOrders = data[isOpenedFirst ? "open" : "closed"];
    bool foundOrder = firstOrders.contains(orderId);
    if (!foundOrder) {
      const std::string_view secondQueryFullName = isOpenedFirst ? "/private/ClosedOrders" : "/private/OpenOrders";
      data.update(PrivateQuery(_curlHandle, _apiKey, secondQueryFullName, ordersPostData).first);
      const json::container& secondOrders = data[isOpenedFirst ? "closed" : "open"];
      foundOrder = secondOrders.contains(orderId);
    }

    if (!foundOrder) {
      if (++nbRetries < kNbMaxRetriesQueryOrders) {
        log::warn("{} is not present in opened nor closed orders, retry {}", orderId, nbRetries);
        continue;
      }
      throw exception("I lost contact with {} order {}", _exchangePublic.name(), orderId);
    }
    return data;

  } while (true);
}

namespace {
/// Compute the destination key name as defined in the Kraken UI by the user.
/// It should be done once per destination account manually.
string KrakenWalletKeyName(const Wallet& destinationWallet) {
  string krakenWalletName(destinationWallet.exchangeName().str());
  krakenWalletName.push_back('_');
  destinationWallet.currencyCode().appendStrTo(krakenWalletName);
  std::ranges::transform(krakenWalletName, krakenWalletName.begin(), tolower);
  return krakenWalletName;
}
}  // namespace

InitiatedWithdrawInfo KrakenPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  CurrencyExchange krakenCurrency = _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  string krakenWalletKey = KrakenWalletKeyName(destinationWallet);

  auto [withdrawData, err] = PrivateQuery(_curlHandle, _apiKey, "/private/Withdraw",
                                          {{"amount", grossAmount.amountStr()},
                                           {"asset", krakenCurrency.altStr()},
                                           {"key", krakenWalletKey},
                                           {"address", destinationWallet.address()}});

  if (err == KrakenErrorEnum::kUnknownWithdrawKey) {
    throw exception(
        "In order to withdraw {} to {} with the API, you need to create a wallet key from {} UI, with the name '{}'",
        grossAmount.currencyCode(), destinationWallet, exchangeName(), krakenWalletKey);
  }

  return {std::move(destinationWallet), std::move(withdrawData["refid"].get_ref<string&>()), grossAmount};
}

}  // namespace cct::api
