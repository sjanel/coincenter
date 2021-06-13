#include "coincenterinfo.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "jsonhelpers.hpp"

namespace cct {

namespace {
CoincenterInfo::CurrencyEquivalentAcronymMap ComputeCurrencyEquivalentAcronymMap() {
  json jsonData = OpenJsonFile("currencyacronymtranslator.json", FileNotFoundMode::kThrow, FileType::kData);
  CoincenterInfo::CurrencyEquivalentAcronymMap map;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Currency {} <=> {}", key, value.get<std::string_view>());
    map.insert_or_assign(CurrencyCode(key), value.get<std::string_view>());
  }
  return map;
}

CoincenterInfo::StableCoinsMap ComputeStableCoinsMap() {
  json jsonData = OpenJsonFile("stablecoins.json", FileNotFoundMode::kThrow, FileType::kData);
  CoincenterInfo::StableCoinsMap ret;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Stable Crypto {} <=> {}", key, value.get<std::string_view>());
    ret.insert_or_assign(CurrencyCode(key), value.get<std::string_view>());
  }
  return ret;
}

CoincenterInfo::ExchangeInfoMap ComputeExchangeInfoMap() {
  constexpr char kExchangeConfigFileName[] = ".exchangeconfig.json";
  json jsonData = OpenJsonFile(kExchangeConfigFileName, FileNotFoundMode::kNoThrow, FileType::kConfig);
  // clang-format off
  const json kDefaultConfig = R"(
  {
    "binance": {
      "asset": {
        "allexclude": "USD,BQX",
        "withdrawexclude": "BTC,EUR"
      },
      "tradefees": {
        "maker": "0.1",
        "taker": "0.1"
      },
      "query": {
        "minpublicquerydelayms": 55,
        "minprivatequerydelayms": 150
      }
    },
    "bithumb": {
      "asset": {
        "allexclude": "",
        "withdrawexclude": "BTC,KRW"
      },
      "tradefees": {
        "maker": "0.25",
        "taker": "0.25"
      },
      "query": {
        "minpublicquerydelayms": 8,
        "minprivatequerydelayms": 8
      }
    },
    "huobi": {
      "asset": {
        "allexclude": "",
        "withdrawexclude": "BTC,EUR"
      },
      "tradefees": {
        "maker": "0.2",
        "taker": "0.2"
      },
      "query": {
        "minpublicquerydelayms": 50,
        "minprivatequerydelayms": 100
      }
    },
    "kraken": {
      "asset": {
        "allexclude": "AUD,CAD,GBP,JPY,USD,KFEE,CHF",
        "withdrawexclude": "BTC,EUR"
      },
      "tradefees": {
        "maker": "0.16",
        "taker": "0.26"
      },
      "query": {
        "minpublicquerydelayms": 500,
        "minprivatequerydelayms": 2000
      }
    },
    "upbit": {
      "asset": {
        "allexclude": "",
        "withdrawexclude": "BTC,KRW"
      },
      "tradefees": {
        "maker": "0.25",
        "taker": "0.25"
      },
      "query": {
        "minpublicquerydelayms": 100,
        "minprivatequerydelayms": 350
      }
    }
  }
  )"_json;
  // clang-format on
  if (jsonData.empty()) {
    // Create a file with default values. User can then update them as he wishes.
    log::warn("No file {}.json found. Creating a default one which can be updated freely at your convenience.",
              kExchangeConfigFileName);
    jsonData = kDefaultConfig;
    WriteJsonFile(kExchangeConfigFileName, jsonData, FileType::kConfig);
  } else {
    bool updateFileNeeded = false;
    for (const auto& [exchangeName, v] : kDefaultConfig.items()) {
      if (!jsonData.contains(exchangeName)) {
        jsonData[exchangeName] = v;
        updateFileNeeded = true;
      }
    }
    if (updateFileNeeded) {
      WriteJsonFile(kExchangeConfigFileName, jsonData, FileType::kConfig);
    }
  }
  CoincenterInfo::ExchangeInfoMap map;
  for (const auto& [exchangeName, value] : jsonData.items()) {
    log::trace("Storing exchange {} info", exchangeName);
    map.insert_or_assign(exchangeName, ExchangeInfo(exchangeName, value));
  }
  return map;
}
}  // namespace

CoincenterInfo::CoincenterInfo(settings::RunMode runMode)
    : _currencyEquiAcronymMap(ComputeCurrencyEquivalentAcronymMap()),
      _stableCoinsMap(ComputeStableCoinsMap()),
      // TODO: make below values configurable, with default value in a json file
      _apiCallUpdateFrequencyMap{{api::QueryTypeEnum::kCurrencies, std::chrono::hours(4)},
                                 {api::QueryTypeEnum::kMarkets, std::chrono::hours(4)},
                                 {api::QueryTypeEnum::kWithdrawalFees, std::chrono::hours(96)},
                                 {api::QueryTypeEnum::kAllOrderBooks, std::chrono::seconds(8)},
                                 {api::QueryTypeEnum::kOrderBook, std::chrono::seconds(1)},
                                 {api::QueryTypeEnum::kTradedVolume, std::chrono::hours(1)},
                                 {api::QueryTypeEnum::kDepositWallet, std::chrono::hours(12)},
                                 {api::QueryTypeEnum::kNbDecimalsUnitsBithumb, std::chrono::hours(96)}},
      _exchangeInfoMap(ComputeExchangeInfoMap()),
      _runMode(runMode),
      _useMonitoring(_runMode == settings::RunMode::kProd) {}

CurrencyCode CoincenterInfo::standardizeCurrencyCode(CurrencyCode currencyCode) const {
  auto it = _currencyEquiAcronymMap.find(currencyCode);
  if (it != _currencyEquiAcronymMap.end()) {
    return it->second;
  }
  return currencyCode;
}

std::optional<CurrencyCode> CoincenterInfo::fiatCurrencyIfStableCoin(CurrencyCode stableCoinCandidate) const {
  auto it = _stableCoinsMap.find(stableCoinCandidate);
  if (it != _stableCoinsMap.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace cct