#include "upbitprivateapi.hpp"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_nonce.hpp"
#include "coincenterinfo.hpp"
#include "jsonhelpers.hpp"
#include "monetaryamount.hpp"
#include "ssl_sha.hpp"
#include "tradeoptionsapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {
namespace api {

namespace {

template <class CurlPostDataT = CurlPostData>
json PrivateQuery(CurlHandle& curlHandle, const APIKey& apiKey, CurlOptions::RequestType requestType,
                  std::string_view method, CurlPostDataT&& curlPostData = CurlPostData()) {
  std::string method_url = UpbitPublic::kUrlBase;
  method_url.append("/v1/");
  method_url.append(method);

  Nonce nonce = Nonce_TimeSinceEpoch();
  CurlOptions opts(requestType, std::forward<CurlPostDataT>(curlPostData), UpbitPublic::kUserAgent);

  auto jsonWebToken = jwt::create()
                          .set_type("JWT")
                          .set_payload_claim("access_key", jwt::claim(apiKey.key()))
                          .set_payload_claim("nonce", jwt::claim(nonce));

  if (!opts.postdata.empty()) {
    std::string queryHash = ssl::ShaDigest(ssl::ShaType::kSha512, opts.postdata.toString());

    jsonWebToken.set_payload_claim("query_hash", jwt::claim(queryHash))
        .set_payload_claim("query_hash_alg", jwt::claim(std::string("SHA512")));
  }

  std::string token = jsonWebToken.sign(jwt::algorithm::hs256{apiKey.privateKey()});

  opts.httpHeaders.emplace_back("Authorization: Bearer ").append(token);

  json dataJson = json::parse(curlHandle.query(method_url, opts));
  return dataJson;
}
}  // namespace

UpbitPrivate::UpbitPrivate(CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey)
    : ExchangePrivate(apiKey),
      _curlHandle(config.exchangeInfo(upbitPublic.name()).minPrivateQueryDelay(), config.getRunMode()),
      _config(config),
      _upbitPublic(upbitPublic),
      _balanceCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAccountBalance), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic),
      _depositWalletsCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kDepositWallet), _cachedResultVault),
          _curlHandle, _apiKey, upbitPublic) {}

BalancePortfolio UpbitPrivate::AccountBalanceFunc::operator()(CurrencyCode equiCurrency) {
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "accounts");
  BalancePortfolio ret;
  for (const json& accountDetail : result) {
    MonetaryAmount a(accountDetail["balance"].get<std::string_view>(),
                     accountDetail["currency"].get<std::string_view>());
    if (!a.isZero()) {
      if (equiCurrency == CurrencyCode::kNeutral) {
        log::info("{} Balance {}", _upbitPublic.name(), a.str());
        ret.add(a, MonetaryAmount("0", equiCurrency));
      } else {
        MonetaryAmount equivalentInMainCurrency = _upbitPublic.computeEquivalentInMainCurrency(a, equiCurrency);
        ret.add(a, equivalentInMainCurrency);
      }
    }
  }
  return ret;
}

Wallet UpbitPrivate::DepositWalletFunc::operator()(CurrencyCode currencyCode) {
  CurlPostData postdata{{"currency", currencyCode.str()}};
  json result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "deposits/coin_address", postdata);
  bool generateDepositAddressNeeded = false;
  if (result.contains("error")) {
    std::string_view name = result["error"]["name"].get<std::string_view>();
    std::string_view msg = result["error"]["message"].get<std::string_view>();
    if (name == "coin_address_not_found") {
      log::warn("No deposit address found for {}, generating a new one...", currencyCode.str());
      generateDepositAddressNeeded = true;
    } else {
      throw exception("error: " + std::string(name) + "msg = " + std::string(msg));
    }
  }
  if (generateDepositAddressNeeded) {
    json genCoinAddressResult =
        PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kPost, "deposits/generate_coin_address", postdata);
    if (genCoinAddressResult.contains("success")) {
      log::info("Successfully generated address");
    }
    result = PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "deposits/coin_address", postdata);
  }
  if (result["deposit_address"].is_null()) {
    throw exception("Deposit address for " + currencyCode.toString() + " is undefined");
  }
  std::string_view address = result["deposit_address"].get<std::string_view>();
  std::string_view tag;
  if (result.contains("secondary_address") && !result["secondary_address"].is_null()) {
    tag = result["secondary_address"].get<std::string_view>();
  }

  Wallet w(PrivateExchangeName(_upbitPublic.name(), _apiKey.name()), currencyCode, address, tag);
  log::info("Retrieved {}", w.str());
  return w;
}

json UpbitPrivate::withdrawalInformation(CurrencyCode currencyCode) {
  return PrivateQuery(_curlHandle, _apiKey, CurlOptions::RequestType::kGet, "withdraws/chance",
                      {{"currency", currencyCode.str()}});
}

}  // namespace api
}  // namespace cct
