#include "huobiprivateapi.hpp"

#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "cct_nonce.hpp"
#include "cct_toupperlower.hpp"
#include "huobipublicapi.hpp"
#include "ssl_sha.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {

namespace {

json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, CurlOptions::RequestType requestType,
                  std::string_view method, const CurlPostData& postdata = CurlPostData()) {
  std::string url = HuobiPublic::kUrlBase;
  url.append(method);

  Nonce nonce = Nonce_LiteralDate();
  std::string encodedNonce = curlHandle.urlEncode(nonce);

  CurlOptions opts(requestType);
  opts.userAgent = HuobiPublic::kUserAgent;

  opts.httpHeaders.push_back("Content-Type: application/json");
  // Remove 'https://' (which is 8 chars) from URL base
  std::string paramsStr(opts.requestTypeStr());
  paramsStr.push_back('\n');
  paramsStr.append(HuobiPublic::kUrlBase + 8);
  paramsStr.push_back('\n');
  paramsStr.append(method);
  paramsStr.push_back('\n');

  CurlPostData signaturePostdata;

  signaturePostdata.append("AccessKeyId", apiKey.key());
  signaturePostdata.append("SignatureMethod", "HmacSHA256");
  signaturePostdata.append("SignatureVersion", "2");
  signaturePostdata.append("Timestamp", encodedNonce);
  if (!postdata.empty()) {
    if (requestType == CurlOptions::RequestType::kGet) {
      signaturePostdata.append(postdata);
    } else {
      opts.postdataInJsonFormat = true;
      opts.postdata = postdata;
    }
  }

  paramsStr.append(signaturePostdata.toStringView());

  std::string sig = curlHandle.urlEncode(B64Encode(ssl::ShaBin(ssl::ShaType::kSha256, paramsStr, apiKey.privateKey())));

  signaturePostdata.append("Signature", sig);
  url.push_back('?');
  url.append(signaturePostdata.toStringView());

  return json::parse(curlHandle.query(url, opts));
}

}  // namespace

HuobiPrivate::HuobiPrivate(const CoincenterInfo& config, HuobiPublic& huobiPublic, const APIKey& apiKey)
    : ExchangePrivate(huobiPublic, config, apiKey),
      _curlHandle(config.exchangeInfo(huobiPublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _accountIdCache(CachedResultOptions(std::chrono::hours(96), _cachedResultVault), _curlHandle, apiKey),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, huobiPublic) {}

BalancePortfolio HuobiPrivate::queryAccountBalance(CurrencyCode equiCurrency) {
  std::string method = "/v1/account/accounts/";
  method += std::to_string(_accountIdCache.get());
  method.append("/balance");
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, method);
  BalancePortfolio ret;
  for (const json& balanceDetail : result["data"]["list"]) {
    std::string_view typeStr = balanceDetail["type"].get<std::string_view>();
    CurrencyCode currencyCode(balanceDetail["currency"].get<std::string_view>());
    MonetaryAmount amount(balanceDetail["balance"].get<std::string_view>(), currencyCode);
    if (typeStr == "trade") {
      this->addBalance(ret, amount, equiCurrency);
    } else {
      log::warn("Do not consider {} as it is {} on {}", amount.str(), typeStr, _exchangePublic.name());
    }
  }
  return ret;
}

Wallet HuobiPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  std::string lowerCaseCur = cct::tolower(currencyCode.str());
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "/v2/account/deposit/address",
                             {{"currency", lowerCaseCur}});
  std::string_view address, tag;
  PrivateExchangeName privateExchangeName(_huobiPublic.name(), _apiKey.name());
  for (const json& depositDetail : result["data"]) {
    address = depositDetail["address"].get<std::string_view>();
    tag = depositDetail["addressTag"].get<std::string_view>();

    if (Wallet::IsAddressPresentInDepositFile(privateExchangeName, currencyCode, address, tag)) {
      break;
    }
    log::warn("{} & tag {} are not validated in the deposit addresses file", address, tag);
    address = std::string_view();
    tag = std::string_view();
  }

  Wallet w(privateExchangeName, currencyCode, address, tag);
  log::info("Retrieved {}", w.str());
  return w;
}

PlaceOrderInfo HuobiPrivate::placeOrder(MonetaryAmount, MonetaryAmount volume, MonetaryAmount price,
                                        const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  const CurrencyCode toCurrencyCode(tradeInfo.toCurrencyCode);

  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(fromCurrencyCode, toCurrencyCode)));
  if (tradeInfo.options.isSimulation()) {
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  const Market m = tradeInfo.m;
  std::string lowerCaseMarket = cct::tolower(m.assetsPairStr());

  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy();
  std::string_view type;
  if (isTakerStrategy) {
    type = fromCurrencyCode == m.base() ? "sell-market" : "buy-market";
  } else {
    type = fromCurrencyCode == m.base() ? "sell-limit" : "buy-limit";
  }

  HuobiPublic& huobiPublic = dynamic_cast<HuobiPublic&>(_exchangePublic);

  price = huobiPublic.sanitizePrice(m, price);

  MonetaryAmount sanitizedVol = huobiPublic.sanitizeVolume(m, fromCurrencyCode, volume, price, isTakerStrategy);
  if (volume < sanitizedVol) {
    log::warn("No trade of {} into {} because min vol order is {} for this market", volume.str(), toCurrencyCode.str(),
              sanitizedVol.str());
    placeOrderInfo.setClosed();
    return placeOrderInfo;
  }

  volume = sanitizedVol;

  CurlPostData placePostData{{"account-id", _accountIdCache.get()}, {"amount", volume.amountStr()}};
  if (!isTakerStrategy) {
    placePostData.append("price", price.amountStr());
  }
  // placePostData.append("source", "api");
  placePostData.append("symbol", lowerCaseMarket);
  placePostData.append("type", type);
  json result =
      PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "/v1/order/orders/place", placePostData);
  placeOrderInfo.orderId = result["data"].get<std::string_view>();
  return placeOrderInfo;
}

OrderInfo HuobiPrivate::cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) {
  std::string endpoint = "/v1/order/orders/";
  endpoint.append(orderId);
  endpoint.append("/submitcancel");
  PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, endpoint);
  return queryOrderInfo(orderId, tradeInfo);
}

OrderInfo HuobiPrivate::queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) {
  const CurrencyCode fromCurrencyCode(tradeInfo.fromCurrencyCode);
  const CurrencyCode toCurrencyCode(tradeInfo.toCurrencyCode);
  const Market m = tradeInfo.m;
  std::string endpoint = "/v1/order/orders/";
  endpoint.append(orderId);

  json res = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, endpoint);
  const json& data = res["data"];
  // Warning: I think Huobi's API has a typo with the 'filled' transformed into 'field' (even documentation is ambiguous
  // on this point). Let's handle both just to be sure.
  std::string_view filledAmount, filledCashAmount, filledFees;
  if (data.contains("field-amount")) {
    filledAmount = data["field-amount"].get<std::string_view>();
    filledCashAmount = data["field-cash-amount"].get<std::string_view>();
    filledFees = data["field-fees"].get<std::string_view>();
  } else {
    filledAmount = data["filled-amount"].get<std::string_view>();
    filledCashAmount = data["filled-cash-amount"].get<std::string_view>();
    filledFees = data["filled-fees"].get<std::string_view>();
  }

  MonetaryAmount baseMatchedAmount(filledAmount, m.base());
  MonetaryAmount quoteMatchedAmount(filledCashAmount, m.quote());
  MonetaryAmount fromAmount = fromCurrencyCode == m.base() ? baseMatchedAmount : quoteMatchedAmount;
  MonetaryAmount toAmount = fromCurrencyCode == m.base() ? quoteMatchedAmount : baseMatchedAmount;
  // Fee is always in destination currency (according to Huobi documentation)
  MonetaryAmount fee(filledFees, toCurrencyCode);
  toAmount -= fee;
  std::string_view state = data["state"].get<std::string_view>();
  bool isClosed = state == "filled" || state == "partial-canceled" || state == "canceled";
  return OrderInfo(TradedAmounts(fromAmount, toAmount), isClosed);
}

InitiatedWithdrawInfo HuobiPrivate::launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  std::string lowerCaseCur = cct::tolower(currencyCode.str());

  json queryWithdrawAddressJson = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet,
                                               "/v2/account/withdraw/address", {{"currency", lowerCaseCur}});
  std::string_view huobiWithdrawAddressName;
  for (const json& withdrawAddress : queryWithdrawAddressJson["data"]) {
    std::string_view address(withdrawAddress["address"].get<std::string_view>());
    std::string_view addressTag(withdrawAddress["addressTag"].get<std::string_view>());
    if (address == wallet.address() && addressTag == wallet.destinationTag()) {
      huobiWithdrawAddressName = withdrawAddress["note"].get<std::string_view>();
      break;
    }
  }
  if (huobiWithdrawAddressName.empty()) {
    throw exception("Address should be stored in your Huobi account manually in order to withdraw from API");
  }
  log::info("Found stored {} withdraw address '{}'", _exchangePublic.name(), huobiWithdrawAddressName);

  CurlPostData withdrawPostData{{"address", wallet.address()}};
  if (wallet.hasDestinationTag()) {
    withdrawPostData.append("addr-tag", wallet.destinationTag());
  }

  MonetaryAmount fee(_exchangePublic.queryWithdrawalFee(grossAmount.currencyCode()));
  MonetaryAmount netEmittedAmount = grossAmount - fee;

  withdrawPostData.append("amount", netEmittedAmount.amountStr());
  withdrawPostData.append("currency", lowerCaseCur);
  // Strange to have the fee as input parameter of a withdraw...
  withdrawPostData.append("fee", fee.amountStr());

  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "/v1/dw/withdraw/api/create",
                             withdrawPostData);
  int64_t withdrawId(result["data"].get<int64_t>());
  return InitiatedWithdrawInfo(std::move(wallet), std::to_string(withdrawId), grossAmount);
}

SentWithdrawInfo HuobiPrivate::isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  std::string lowerCaseCur = cct::tolower(currencyCode.str());
  std::string_view withdrawId = initiatedWithdrawInfo.withdrawId();
  json withdrawJson = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "/v1/query/deposit-withdraw",
                                   {{"currency", lowerCaseCur}, {"from", withdrawId}, {"type", "withdraw"}});
  MonetaryAmount netEmittedAmount;
  bool isWithdrawSent = false;
  for (const json& withdrawDetail : withdrawJson["data"]) {
    int64_t withdrawDetailId(withdrawDetail["id"].get<int64_t>());
    if (std::to_string(withdrawDetailId) == withdrawId) {
      std::string_view withdrawStatus = withdrawDetail["state"].get<std::string_view>();
      if (withdrawStatus == "verifying") {
        log::debug("Awaiting verification");
      } else if (withdrawStatus == "failed") {
        log::error("Verification failed");
      } else if (withdrawStatus == "submitted") {
        log::debug("Withdraw request submitted successfully");
      } else if (withdrawStatus == "reexamine") {
        log::warn("Under examination for withdraw validation");
      } else if (withdrawStatus == "canceled") {
        log::error("Withdraw canceled");
      } else if (withdrawStatus == "pass") {
        log::debug("Withdraw validation passed");
      } else if (withdrawStatus == "reject") {
        log::error("Withdraw validation rejected");
      } else if (withdrawStatus == "pre-transfer") {
        log::debug("Withdraw is about to be released");
      } else if (withdrawStatus == "wallet-transfer") {
        log::debug("On-chain transfer initiated");
      } else if (withdrawStatus == "wallet-reject") {
        log::error("Transfer rejected by chain");
      } else if (withdrawStatus == "confirmed") {
        isWithdrawSent = true;
        log::debug("On-chain transfer completed with one confirmation");
      } else if (withdrawStatus == "confirm-error") {
        log::error("On-chain transfer failed to get confirmation");
      } else if (withdrawStatus == "repealed") {
        log::error("Withdraw terminated by system");
      } else {
        log::error("unknown status value '{}'", withdrawStatus);
      }
      netEmittedAmount = MonetaryAmount(withdrawDetail["amount"].get<double>(), currencyCode);
      MonetaryAmount fee(withdrawDetail["fee"].get<double>(), currencyCode);
      if (netEmittedAmount + fee != initiatedWithdrawInfo.grossEmittedAmount()) {
        log::error("{} + {} != {}, maybe a change in API", netEmittedAmount.amountStr(), fee.amountStr(),
                   initiatedWithdrawInfo.grossEmittedAmount().amountStr());
      }
      break;
    }
  }
  return SentWithdrawInfo(netEmittedAmount, isWithdrawSent);
}

bool HuobiPrivate::isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                      const SentWithdrawInfo& sentWithdrawInfo) {
  const CurrencyCode currencyCode = initiatedWithdrawInfo.grossEmittedAmount().currencyCode();
  std::string lowerCaseCur = cct::tolower(currencyCode.str());

  json depositJson = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "/v1/query/deposit-withdraw",
                                  {{"currency", lowerCaseCur}, {"type", "deposit"}});
  MonetaryAmount netEmittedAmount = sentWithdrawInfo.netEmittedAmount();
  for (const json& depositDetail : depositJson["data"]) {
    MonetaryAmount amount(depositDetail["amount"].get<double>(), currencyCode);
    if (amount == netEmittedAmount) {
      std::string_view depositStatus = depositDetail["state"].get<std::string_view>();
      if (depositStatus == "unknown") {
        log::debug("On-chain transfer has not been received");
      } else if (depositStatus == "confirming") {
        log::debug("On-chain transfer waits for first confirmation");
      } else if (depositStatus == "confirmed") {
        log::debug("On-chain transfer confirmed for at least one block, user is able to transfer and trade");
        return true;
      } else if (depositStatus == "safe") {
        log::debug("Multiple on-chain confirmed, user is able to withdraw");
        return true;
      } else if (depositStatus == "orphan") {
        log::error("On-chain transfer confirmed but currently in an orphan branch");
      } else {
        log::error("unknown status value '{}'", depositStatus);
      }
      break;
    }
  }
  return false;
}

int HuobiPrivate::AccountIdFunc::operator()() {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "/v1/account/accounts");
  for (const json& accDetails : result["data"]) {
    std::string_view state = accDetails["state"].get<std::string_view>();
    if (state == "working") {
      return accDetails["id"].get<int>();
    }
  }
  throw exception("Unable to find a working Huobi account");
}
}  // namespace api
}  // namespace cct