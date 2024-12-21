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
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "httprequesttype.hpp"
#include "kraken-schema.hpp"
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

template <class T, class CurlPostDataT = CurlPostData>
auto PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, std::string_view method,
                  CurlPostDataT&& curlPostData = CurlPostData()) {
  CurlOptions opts(HttpRequestType::kPost, std::forward<CurlPostDataT>(curlPostData));
  opts.mutableHttpHeaders().emplace_back("API-Key", apiKey.key());

  RequestRetry requestRetry(curlHandle, std::move(opts),
                            QueryRetryPolicy{.initialRetryDelay = seconds{1}, .nbMaxRetries = 3});

  KrakenErrorEnum err = KrakenErrorEnum::kNoError;

  return std::pair<T, KrakenErrorEnum>(
      requestRetry.query<T>(
          method,
          [&err](const T& response) {
            if (response.error.empty()) {
              return RequestRetry::Status::kResponseOK;
            }
            for (const auto& error : response.error) {
              if (error == "EAPI:Rate limit exceeded") {
                log::warn("kraken private API rate limit exceeded");
                return RequestRetry::Status::kResponseError;
              }
              if (error.ends_with("Unknown order")) {
                err = KrakenErrorEnum::kExpiredOrder;
                return RequestRetry::Status::kResponseOK;
              }
              if (error.ends_with("Unknown withdraw key")) {
                err = KrakenErrorEnum::kUnknownWithdrawKey;
                return RequestRetry::Status::kResponseOK;
              }
              log::error("kraken unknown error {}", error);
            }
            return RequestRetry::Status::kResponseError;
          },
          [&apiKey, method](CurlOptions& curlOptions) {
            Nonce noncePostData = Nonce_TimeSinceEpochInMs();
            curlOptions.mutablePostData().set("nonce", noncePostData);

            // concatenate nonce and postdata and compute SHA256
            noncePostData.append(curlOptions.postData().str());

            // concatenate path and nonce_postdata (path + ComputeSha256(nonce + postdata))
            auto sha256 = ssl::Sha256(noncePostData);

            string path;
            path.reserve(KrakenPublic::kVersion.size() + method.size() + sha256.size());
            path.append(KrakenPublic::kVersion).append(method).append(sha256.data(), sha256.data() + sha256.size());

            static constexpr std::string_view kSignatureKey = "API-Sign";

            // and compute HMAC
            curlOptions.mutableHttpHeaders().set_back(kSignatureKey,
                                                      B64Encode(ssl::Sha512Bin(path, B64Decode(apiKey.privateKey()))));
          }),
      err);
}
}  // namespace

KrakenPrivate::KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey)
    : ExchangePrivate(config, krakenPublic, apiKey),
      _curlHandle(KrakenPublic::kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  config.getRunMode()),
      _depositWalletsCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::depositWallet).duration,
                              _cachedResultVault),
          _curlHandle, _apiKey, krakenPublic) {}

bool KrakenPrivate::validateApiKey() {
  return PrivateQuery<schema::kraken::PrivateBalance>(_curlHandle, _apiKey, "/private/Balance").second ==
         KrakenErrorEnum::kNoError;
}

CurrencyExchangeFlatSet KrakenPrivate::queryTradableCurrencies() { return _exchangePublic.queryTradableCurrencies(); }

BalancePortfolio KrakenPrivate::queryAccountBalance(const BalanceOptions& balanceOptions) {
  BalancePortfolio balancePortfolio;
  auto [res, err] = PrivateQuery<schema::kraken::PrivateBalance>(_curlHandle, _apiKey, "/private/Balance");
  // Kraken returns an empty array in case of account with no balance at all
  MonetaryAmountVector balanceAmounts;
  balanceAmounts.reserve(static_cast<MonetaryAmountVector::size_type>(res.result.size()));
  for (const auto& [curCode, amount] : res.result) {
    if (curCode.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code {} is too long, skipping", curCode);
      continue;
    }

    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(curCode));

    balanceAmounts.emplace_back(amount, currencyCode);
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
        case TradeSide::buy: {
          MonetaryAmount price = order.price();
          auto lb = std::ranges::lower_bound(balanceAmounts, price, compByCurrency);
          if (lb != balanceAmounts.end() && lb->currencyCode() == price.currencyCode()) {
            *lb -= remVolume.toNeutral() * price;
          } else {
            log::error("Was expecting at least {} in Kraken balance", remVolume.toNeutral() * price);
          }
          break;
        }
        case TradeSide::sell: {
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
  auto [depositMethods, errDepositMethods] = PrivateQuery<schema::kraken::DepositMethods>(
      _curlHandle, _apiKey, "/private/DepositMethods", {{"asset", krakenCurrency.altStr()}});
  const CoincenterInfo& coincenterInfo = _exchangePublic.coincenterInfo();
  const bool doCheckWallet =
      coincenterInfo.exchangeConfig(_exchangePublic.exchangeNameEnum()).withdraw.validateDepositAddressesInFile;
  WalletCheck walletCheck(coincenterInfo.dataDir(), doCheckWallet);

  ExchangeName eName(_exchangePublic.exchangeNameEnum(), _apiKey.name());
  string address;
  string tag;

  for (const auto& depositMethod : depositMethods.result) {
    auto [res, err] = PrivateQuery<schema::kraken::DepositAddresses>(
        _curlHandle, _apiKey, "/private/DepositAddresses",
        {{"asset", krakenCurrency.altStr()}, {"method", depositMethod.method}});
    if (res.result.empty()) {
      // This means user has not created a wallet yet, but it's possible to do it via DepositMethods query above.
      log::warn("No deposit address found on {} for {}, creating a new one", eName, currencyCode);
      std::tie(res, err) = PrivateQuery<schema::kraken::DepositAddresses>(
          _curlHandle, _apiKey, "/private/DepositAddresses",
          {{"asset", krakenCurrency.altStr()}, {"method", depositMethod.method}, {"new", "true"}});
      if (res.result.empty()) {
        log::error("Cannot create a new deposit address on {} for {}", eName, currencyCode);
        continue;
      }
    }

    for (auto& depositDetail : res.result) {
      std::visit(
          [&tag](auto&& field) {
            using T = std::decay_t<decltype(field)>;
            if constexpr (std::is_same_v<T, string>) {
              tag = std::move(field);
            } else if constexpr (std::is_same_v<T, int64_t>) {
              tag = IntegralToString(field);
            } else {
              static_assert(std::is_same_v<T, string> || std::is_same_v<T, int64_t>, "unexpected type");
            }
          },
          depositDetail.tag);

      if (tag.empty()) {
        std::visit(
            [&tag](auto&& field) {
              using T = std::decay_t<decltype(field)>;
              if constexpr (std::is_same_v<T, string>) {
                tag = std::move(field);
              } else if constexpr (std::is_same_v<T, int64_t>) {
                tag = IntegralToString(field);
              } else {
                static_assert(std::is_same_v<T, string> || std::is_same_v<T, int64_t>, "unexpected type");
              }
            },
            depositDetail.memo);
      }

      address = std::move(depositDetail.address);

      if (Wallet::ValidateWallet(walletCheck, eName, currencyCode, address, tag)) {
        break;
      }
      log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
      address.clear();
      tag.clear();
    }
    if (!address.empty()) {
      // Limitation: do not process other methods if we have found a valid address
      break;
    }
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

    auto [data, err] =
        PrivateQuery<schema::kraken::OpenedOrClosedOrders>(_curlHandle, _apiKey, "/private/ClosedOrders", params);

    nbOrdersRetrieved = 0;

    for (const auto& [orderId, orderDetails] : data.result.closed) {
      ++nbOrdersRetrieved;
      if (!closedOrdersConstraints.validateId(orderId)) {
        continue;
      }
      const auto& descrPart = orderDetails.descr;
      std::string_view marketStr = descrPart.pair;

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

      MonetaryAmount matchedVolume(orderDetails.vol_exec, volumeCur);
      if (matchedVolume == 0) {
        continue;
      }

      MonetaryAmount price(orderDetails.price, priceCur);
      TimePoint placedTime = TimePointFromKrakenTime(orderDetails.opentm);
      if (!closedOrdersConstraints.validatePlacedTime(placedTime)) {
        continue;
      }

      TimePoint matchedTime = TimePointFromKrakenTime(orderDetails.closetm);

      TradeSide side = descrPart.type == "buy" ? TradeSide::buy : TradeSide::sell;

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
  auto [res, err] = PrivateQuery<schema::kraken::OpenedOrClosedOrders>(_curlHandle, _apiKey, "/private/OpenOrders",
                                                                       {{"trades", "true"}});
  OpenedOrderVector openedOrders;
  MarketSet markets;

  for (const auto& [id, orderDetails] : res.result.open) {
    const auto& descrPart = orderDetails.descr;
    std::string_view marketStr = descrPart.pair;

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

    MonetaryAmount originalVolume(orderDetails.vol, volumeCur);
    MonetaryAmount matchedVolume(orderDetails.vol_exec, volumeCur);
    MonetaryAmount remainingVolume = originalVolume - matchedVolume;
    MonetaryAmount price(descrPart.price, priceCur);
    TradeSide side = descrPart.type == "buy" ? TradeSide::buy : TradeSide::sell;

    TimePoint placedTime = TimePointFromKrakenTime(orderDetails.opentm);
    if (!openedOrdersConstraints.validatePlacedTime(placedTime)) {
      continue;
    }

    openedOrders.emplace_back(id, matchedVolume, remainingVolume, price, placedTime, side);
  }

  std::ranges::sort(openedOrders);
  log::info("Retrieved {} opened orders from {}", openedOrders.size(), exchangeName());
  return openedOrders;
}

int KrakenPrivate::cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints) {
  if (openedOrdersConstraints.noConstraints()) {
    auto [res, err] = PrivateQuery<schema::kraken::CancelAllOrders>(_curlHandle, _apiKey, "/private/CancelAll");
    return res.result.count;
  }
  OpenedOrderVector openedOrders = queryOpenedOrders(openedOrdersConstraints);
  for (const OpenedOrder& order : openedOrders) {
    cancelOrderProcess(order.id());
  }
  return openedOrders.size();
}

namespace {
Deposit::Status DepositStatusFromStatus(schema::kraken::DepositStatus::Deposit::Status depositStatus) {
  if (depositStatus == schema::kraken::DepositStatus::Deposit::Status::Settled) {
    return Deposit::Status::processing;
  }
  if (depositStatus == schema::kraken::DepositStatus::Deposit::Status::Success) {
    return Deposit::Status::success;
  }
  return Deposit::Status::failed;
}
}  // namespace

DepositsSet KrakenPrivate::queryRecentDeposits(const DepositsConstraints& depositsConstraints) {
  Deposits deposits;
  CurlPostData options;
  if (depositsConstraints.isCurDefined()) {
    options.emplace_back("asset", depositsConstraints.currencyCode().str());
  }
  auto [res, err] =
      PrivateQuery<schema::kraken::DepositStatus>(_curlHandle, _apiKey, "/private/DepositStatus", options);
  for (auto& trx : res.result) {
    Deposit::Status status = DepositStatusFromStatus(trx.status);

    if (trx.asset.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code {} is too long, skipping", trx.asset);
      continue;
    }

    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(trx.asset));
    MonetaryAmount amount(trx.amount, currencyCode);
    int64_t secondsSinceEpoch = trx.time;
    TimePoint timestamp{seconds(secondsSinceEpoch)};

    if (!depositsConstraints.validateTime(timestamp)) {
      continue;
    }
    if (!depositsConstraints.validateId(trx.txid)) {
      continue;
    }

    deposits.emplace_back(std::move(trx.txid), timestamp, amount, status);
  }
  DepositsSet depositsSet(std::move(deposits));
  log::info("Retrieved {} recent deposits for {}", depositsSet.size(), exchangeName());
  return depositsSet;
}

namespace {
Withdraw::Status WithdrawStatusFromStatusStr(std::string_view statusStr) {
  if (statusStr == "Initial" || statusStr == "Pending") {
    return Withdraw::Status::initial;
  }
  if (statusStr == "Settled" || statusStr == "On hold") {
    return Withdraw::Status::processing;
  }
  if (statusStr == "Success") {
    return Withdraw::Status::success;
  }
  if (statusStr == "Failure") {
    return Withdraw::Status::failed;
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
  auto [res, err] = PrivateQuery<schema::kraken::WithdrawStatus>(
      _curlHandle, _apiKey, "/private/WithdrawStatus", CreateOptionsFromWithdrawConstraints(withdrawsConstraints));
  for (auto& trx : res.result) {
    if (trx.asset.size() > CurrencyCode::kMaxLen) {
      log::warn("Currency code {} is too long, skipping", trx.asset);
      continue;
    }

    int64_t secondsSinceEpoch = trx.time;
    TimePoint timestamp{seconds(secondsSinceEpoch)};
    if (!withdrawsConstraints.validateTime(timestamp)) {
      continue;
    }

    if (!withdrawsConstraints.validateId(trx.refid)) {
      continue;
    }

    Withdraw::Status status = WithdrawStatusFromStatusStr(trx.status);

    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(trx.asset));
    MonetaryAmount netEmittedAmount(trx.amount, currencyCode);
    MonetaryAmount fee(trx.fee, currencyCode);

    withdraws.emplace_back(std::move(trx.refid), timestamp, netEmittedAmount, status, fee);
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
      tradeInfo.options.isTakerStrategy(_exchangePublic.exchangeConfig().query.placeSimulateRealOrder);
  const bool isSimulation = tradeInfo.options.isSimulation();
  const Market mk = tradeInfo.tradeContext.market;
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

  auto [placeOrderRes, err] =
      PrivateQuery<schema::kraken::AddOrder>(_curlHandle, _apiKey, "/private/AddOrder", std::move(placePostData));
  // {"error":[],"result":{"descr":{"order":"buy 24.69898116 XRPETH @ limit 0.0003239"},"txid":["OWBA44-TQZQ7-EEYSXA"]}}
  if (isSimulation) {
    // In simulation mode, there is no txid returned. If we arrived here (after CollectResults) we assume that the call
    // to api was a success.
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  if (placeOrderRes.result.txid.empty()) {
    log::error("Kraken did not return any txid for the order");
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  placeOrderInfo.orderId = std::move(placeOrderRes.result.txid.front());

  // Kraken will automatically truncate the decimals to the maximum allowed for the trade assets. Get this information
  // and adjust our amount.
  std::string_view orderDescriptionStr = placeOrderRes.result.descr.order;
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
  auto [response, err] =
      PrivateQuery<schema::kraken::CancelOrder>(_curlHandle, _apiKey, "/private/CancelOrder", {{"txid", orderId}});
  if (err == KrakenErrorEnum::kExpiredOrder) {
    log::warn("{} is unable to find order {} - it has probably expired or been matched", exchangeName(), orderId);
  }
}

OrderInfo KrakenPrivate::queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext, QueryOrder queryOrder) {
  const CurrencyCode fromCurrencyCode = tradeContext.fromCur();
  const CurrencyCode toCurrencyCode = tradeContext.toCur();
  const Market mk = tradeContext.market;

  const auto data = queryOrdersData(tradeContext.userRef, orderId, queryOrder);

  const auto openIt = data.result.open.find(orderId);
  const auto closedIt = data.result.closed.find(orderId);
  const bool orderInOpenedPart = openIt != data.result.open.end();

  OrderInfo orderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode), !orderInOpenedPart);

  if (openIt == data.result.open.end() && closedIt == data.result.closed.end()) {
    log::error("{} is unable to find order {}", exchangeName(), orderId);
    return orderInfo;
  }

  const auto& orderJson = orderInOpenedPart ? openIt->second : closedIt->second;
  MonetaryAmount vol(orderJson.vol, mk.base());             // always in base currency
  MonetaryAmount tradedVol(orderJson.vol_exec, mk.base());  // always in base currency
  // Avoid division by 0 as the price is returned as 0.
  if (tradedVol != 0) {
    MonetaryAmount tradedCost(orderJson.cost, mk.quote());  // always in quote currency
    MonetaryAmount fee(orderJson.fee, mk.quote());          // always in quote currency

    if (fromCurrencyCode == mk.quote()) {
      MonetaryAmount price(orderJson.price, mk.base());
      orderInfo.tradedAmounts.from += tradedCost;
      orderInfo.tradedAmounts.to += (tradedCost - fee).toNeutral() / price;
    } else {
      orderInfo.tradedAmounts.from += tradedVol;
      orderInfo.tradedAmounts.to += tradedCost - fee;
    }
  }

  return orderInfo;
}

schema::kraken::OpenedOrClosedOrders KrakenPrivate::queryOrdersData(int64_t userRef, OrderIdView orderId,
                                                                    QueryOrder queryOrder) {
  static constexpr int kNbMaxRetriesQueryOrders = 10;
  int nbRetries = 0;
  CurlPostData ordersPostData{{"trades", "true"}, {"userref", userRef}};
  const bool isOpenedFirst = queryOrder == QueryOrder::kOpenedThenClosed;
  const std::string_view firstQueryFullName = isOpenedFirst ? "/private/OpenOrders" : "/private/ClosedOrders";
  do {
    auto data =
        PrivateQuery<schema::kraken::OpenedOrClosedOrders>(_curlHandle, _apiKey, firstQueryFullName, ordersPostData)
            .first;
    const auto& firstOrders = isOpenedFirst ? data.result.open : data.result.closed;
    bool foundOrder = firstOrders.contains(orderId);
    if (!foundOrder) {
      const std::string_view secondQueryFullName = isOpenedFirst ? "/private/ClosedOrders" : "/private/OpenOrders";
      auto secondData =
          PrivateQuery<schema::kraken::OpenedOrClosedOrders>(_curlHandle, _apiKey, secondQueryFullName, ordersPostData)
              .first;
      if (isOpenedFirst) {
        data.result.closed = std::move(secondData.result.closed);
      } else {
        data.result.open = std::move(secondData.result.open);
      }

      const auto& secondOrders = isOpenedFirst ? data.result.closed : data.result.open;
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

  auto [withdrawData, err] = PrivateQuery<schema::kraken::Withdraw>(_curlHandle, _apiKey, "/private/Withdraw",
                                                                    {{"amount", grossAmount.amountStr()},
                                                                     {"asset", krakenCurrency.altStr()},
                                                                     {"key", krakenWalletKey},
                                                                     {"address", destinationWallet.address()}});

  if (err == KrakenErrorEnum::kUnknownWithdrawKey) {
    throw exception(
        "In order to withdraw {} to {} with the API, you need to create a wallet key from {} UI, with the name '{}'",
        grossAmount.currencyCode(), destinationWallet, exchangeName(), krakenWalletKey);
  }

  return {std::move(destinationWallet), std::move(withdrawData.result.refid), grossAmount};
}

}  // namespace cct::api
