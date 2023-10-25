#include "exchangeinfoparser.hpp"

#include <memory>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycodevector.hpp"
#include "exchangeinfodefault.hpp"
#include "file.hpp"
#include "loadconfiguration.hpp"
#include "unreachable.hpp"

namespace cct {

namespace {
constexpr std::string_view kDefaultPart = "default";
constexpr std::string_view kExchangePart = "exchange";
}  // namespace

json LoadExchangeConfigData(const LoadConfiguration& loadConfiguration) {
  switch (loadConfiguration.exchangeConfigFileType()) {
    case LoadConfiguration::ExchangeConfigFileType::kProd: {
      std::string_view filename = loadConfiguration.exchangeConfigFileName();
      File exchangeConfigFile(loadConfiguration.dataDir(), File::Type::kStatic, filename, File::IfError::kNoThrow);
      json jsonData = ExchangeInfoDefault::Prod();
      json exchangeConfigJsonData = exchangeConfigFile.readAllJson();
      if (exchangeConfigJsonData.empty()) {
        // Create a file with default values. User can then update them as he wishes.
        log::warn("No {} file found. Creating a default one which can be updated freely at your convenience", filename);
        exchangeConfigFile.write(jsonData);
        return jsonData;
      }
      for (std::string_view optName : {TopLevelOption::kAssetsOptionStr, TopLevelOption::kQueryOptionStr,
                                       TopLevelOption::kTradeFeesOptionStr, TopLevelOption::kWithdrawOptionStr}) {
        if (!exchangeConfigJsonData.contains(optName)) {  // basic backward compatibility
          log::error("Invalid {} file (or wrong format), using default configuration instead", filename);
          log::error("Follow same pattern as default one in 'kDefaultConfig'");
          return jsonData;
        }
      }
      return exchangeConfigJsonData;
    }
    case LoadConfiguration::ExchangeConfigFileType::kTest:
      return ExchangeInfoDefault::Test();
    default:
      unreachable();
  }
}

TopLevelOption::TopLevelOption(std::string_view optionName, const json& defaultJsonData, const json& personalJsonData) {
  for (const json* jsonData : {std::addressof(personalJsonData), std::addressof(defaultJsonData)}) {
    JsonIt optIt = jsonData->find(optionName);
    if (optIt != jsonData->end()) {
      bool isPersonal = jsonData == std::addressof(personalJsonData);
      for (std::string_view partName : {kExchangePart, kDefaultPart}) {
        JsonIt it = optIt->find(partName);
        if (it != optIt->end()) {
          bool isExchange = partName == kExchangePart;
          _orderedDataSource.emplace_back(it, isPersonal, isExchange);
        }
      }
    }
  }

  if (_orderedDataSource.empty()) {
    throw exception("Unable to find '{}' top level option in config file content", optionName);
  }
}

TopLevelOption::JsonIt TopLevelOption::get(std::string_view exchangeName, std::string_view subOptionName1,
                                           std::string_view subOptionName2) {
  for (const DataSource& dataSource : _orderedDataSource) {
    JsonIt exchangeIt = dataSource.exchangeIt(exchangeName);
    JsonIt optValIt = exchangeIt->find(subOptionName1);
    if (optValIt != exchangeIt->end()) {
      if (subOptionName2.empty()) {
        setReadValue(dataSource, exchangeName, subOptionName1, subOptionName2, optValIt);
        return optValIt;
      }
      JsonIt optValIt2 = optValIt->find(subOptionName2);
      if (optValIt2 != optValIt->end()) {
        setReadValue(dataSource, exchangeName, subOptionName1, subOptionName2, optValIt2);
        return optValIt2;
      }
    }
  }

  string err("Unable to find option '");
  err.append(subOptionName1);
  if (!subOptionName2.empty()) {
    err.push_back('.');
    err.append(subOptionName2);
  }
  err.append("' for '").append(exchangeName);
  err.append("'. Make sure that your ").append(LoadConfiguration::kProdDefaultExchangeConfigFile);
  err.append(" file follows same schema as possible options.");
  throw exception(std::move(err));
}

void TopLevelOption::setReadValue(const DataSource& dataSource, std::string_view exchangeName,
                                  std::string_view subOptionName1, std::string_view subOptionName2, JsonIt valueIt) {
  json emptyJson;
  bool isExchange = dataSource.isExchange;
  auto readValuesExchangePartIt = _readValues.emplace(isExchange ? kExchangePart : kDefaultPart, emptyJson).first;
  if (isExchange) {
    readValuesExchangePartIt = readValuesExchangePartIt->emplace(exchangeName, emptyJson).first;
  }
  auto readValuesSubOptionNameIt =
      readValuesExchangePartIt->emplace(subOptionName1, subOptionName2.empty() ? *valueIt : emptyJson).first;
  if (!subOptionName2.empty()) {
    readValuesSubOptionNameIt->emplace(subOptionName2, *valueIt);
  }
}

CurrencyCodeVector TopLevelOption::getUnorderedCurrencyUnion(std::string_view exchangeName,
                                                             std::string_view subOptionName) {
  CurrencyCodeVector ret;
  bool personalSourceFilled = false;
  for (const DataSource& dataSource : _orderedDataSource) {
    JsonIt exchangeIt = dataSource.exchangeIt(exchangeName);
    JsonIt optValIt = exchangeIt->find(subOptionName);
    if (optValIt != exchangeIt->end()) {
      if (dataSource.isPersonal) {
        personalSourceFilled = true;
      } else if (personalSourceFilled) {
        // Personal source has priority for this type of data.
        break;
      }
      setReadValue(dataSource, exchangeName, subOptionName, "", optValIt);
      for (const auto& val : *optValIt) {
        ret.emplace_back(val.get<std::string_view>());
      }
    }
  }
  return ret;
}

}  // namespace cct