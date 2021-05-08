#include "coincenterinfo.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "jsonhelpers.hpp"

namespace cct {

namespace {
CoincenterInfo::CurrencyEquivalentAcronymMap ComputeCurrencyEquivalentAcronymMap() {
  json jsonData = OpenJsonFile("currencyacronymtranslator", FileNotFoundMode::kThrow);
  CoincenterInfo::CurrencyEquivalentAcronymMap map;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Currency " + std::string(key) + " <=> " + std::string(value));
    map.insert_or_assign(key, value);
  }
  return map;
}

CoincenterInfo::ExchangeInfoMap ComputeExchangeInfoMap() {
  constexpr char kExchangeConfigFileName[] = ".exchangeconfig";
  json jsonData = OpenJsonFile(kExchangeConfigFileName, FileNotFoundMode::kNoThrow);
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
    WriteJsonFile(kExchangeConfigFileName, jsonData);
  } else {
    bool updateFileNeeded = false;
    for (const auto& [exchangeName, v] : kDefaultConfig.items()) {
      if (!jsonData.contains(exchangeName)) {
        jsonData[exchangeName] = v;
        updateFileNeeded = true;
      }
    }
    if (updateFileNeeded) {
      WriteJsonFile(kExchangeConfigFileName, jsonData);
    }
  }
  CoincenterInfo::ExchangeInfoMap map;
  for (const auto& [exchangeName, value] : jsonData.items()) {
    log::info("Storing exchange {} info", exchangeName);

    map.insert_or_assign(exchangeName, ExchangeInfo(exchangeName, value));
  }
  return map;
}
}  // namespace

CoincenterInfo::CoincenterInfo(settings::RunMode runMode)
    : _currencyEquiAcronymMap(ComputeCurrencyEquivalentAcronymMap()),
      // TODO: make below values configurable, with default value in a json file
      _apiCallUpdateFrequencyMap{{api::QueryTypeEnum::kCurrencies, std::chrono::hours(1)},
                                 {api::QueryTypeEnum::kMarkets, std::chrono::hours(1)},
                                 {api::QueryTypeEnum::kWithdrawalFees, std::chrono::hours(1)},
                                 {api::QueryTypeEnum::kAllOrderBooks, std::chrono::seconds(8)},
                                 {api::QueryTypeEnum::kOrderBook, std::chrono::seconds(1)},
                                 {api::QueryTypeEnum::kAccountBalance, std::chrono::seconds(5)},
                                 {api::QueryTypeEnum::kDepositWallet, std::chrono::hours(12)},
                                 {api::QueryTypeEnum::kNbDecimalsUnitsBithumb, std::chrono::hours(96)}},
      _exchangeInfoMap(ComputeExchangeInfoMap()),
      _runMode(runMode),
      _useMonitoring(_runMode == settings::kProd) {}

std::string_view CoincenterInfo::standardizeCurrencyCode(std::string_view currencyCode) const {
  auto it = _currencyEquiAcronymMap.find(std::string(currencyCode));
  if (it != _currencyEquiAcronymMap.end()) {
    // returning a const ref is OK as _currencyEquiAcronymMap is const (permanent validity of pointers)
    return it->second;
  }
  return currencyCode;
}

}  // namespace cct