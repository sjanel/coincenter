#include "exchangeinfoparser.hpp"

#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_log.hpp"
#include "exchangeinfodefault.hpp"
#include "loadconfiguration.hpp"
#include "unreachable.hpp"

namespace cct {

json LoadExchangeConfigData(const LoadConfiguration& loadConfiguration) {
  switch (loadConfiguration.exchangeConfigFileType()) {
    case LoadConfiguration::ExchangeConfigFileType::kProd: {
      std::string_view filename = loadConfiguration.exchangeConfigFile();
      File exchangeConfigFile(loadConfiguration.dataDir(), File::Type::kStatic, filename, File::IfError::kNoThrow);
      json jsonData = ExchangeInfoDefault::Prod();
      json exchangeConfigJsonData = exchangeConfigFile.readJson();
      if (exchangeConfigJsonData.empty()) {
        // Create a file with default values. User can then update them as he wishes.
        log::warn("No {} file found. Creating a default one which can be updated freely at your convenience", filename);
        exchangeConfigFile.write(jsonData);
      } else {
        for (std::string_view optName : {TopLevelOption::kAssetsOptionStr, TopLevelOption::kQueryOptionStr,
                                         TopLevelOption::kTradeFeesOptionStr, TopLevelOption::kWithdrawOptionStr}) {
          if (!exchangeConfigJsonData.contains(optName)) {  // basic backward compatibility
            log::error("Invalid {} file (or wrong format), using default configuration instead", filename);
            log::error("Follow same pattern as default one in 'kDefaultConfig'");
            return jsonData;
          }
        }
        jsonData.update(exchangeConfigJsonData);
      }
      return jsonData;
    }
    case LoadConfiguration::ExchangeConfigFileType::kTest:
      return ExchangeInfoDefault::Test();
    default:
      unreachable();
  }
}

TopLevelOption::TopLevelOption(const json& jsonData, std::string_view optionName) {
  auto optIt = jsonData.find(optionName);
  if (optIt == jsonData.end()) {
    throw exception("Unable to find '{}' top level option in config file content", optionName);
  }
  _defaultPart = optIt->find("default");
  _hasDefaultPart = _defaultPart != optIt->end();
  _exchangePart = optIt->find("exchange");
  _hasExchangePart = _exchangePart != optIt->end();
}

TopLevelOption::JsonIt TopLevelOption::get(std::string_view exchangeName, std::string_view subOptionName1,
                                           std::string_view subOptionName2) const {
  if (_hasExchangePart) {
    JsonIt it = _exchangePart->find(exchangeName);
    if (it != _exchangePart->end()) {
      JsonIt optValIt = it->find(subOptionName1);
      if (optValIt != it->end()) {
        if (subOptionName2.empty()) {
          // Exchange defined the option, it has priority, return it
          return optValIt;
        }
        JsonIt optValIt2 = optValIt->find(subOptionName2);
        if (optValIt2 != optValIt->end()) {
          return optValIt2;
        }
      }
    }
  }
  if (_hasDefaultPart) {
    JsonIt optValIt = _defaultPart->find(subOptionName1);
    if (optValIt != _defaultPart->end()) {
      if (subOptionName2.empty()) {
        // Exchange defined the option, it has priority, return it
        return optValIt;
      }
      JsonIt optValIt2 = optValIt->find(subOptionName2);
      if (optValIt2 != optValIt->end()) {
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

TopLevelOption::CurrencyVector TopLevelOption::getUnorderedCurrencyUnion(std::string_view exchangeName,
                                                                         std::string_view subOptionName) const {
  CurrencyVector ret;
  const auto appendFunc = [&](JsonIt it) {
    JsonIt optValIt = it->find(subOptionName);
    if (optValIt != it->end()) {
      assert(optValIt->is_array());
      for (const auto& val : *optValIt) {
        ret.emplace_back(val.get<std::string_view>());
      }
    }
  };
  if (_hasExchangePart) {
    JsonIt it = _exchangePart->find(exchangeName);
    if (it != _exchangePart->end()) {
      appendFunc(it);
    }
  }
  if (_hasDefaultPart) {
    appendFunc(_defaultPart);
  }
  return ret;
}

TopLevelOption::CurrencyVector TopLevelOption::getCurrenciesArray(std::string_view exchangeName,
                                                                  std::string_view subOptionName1,
                                                                  std::string_view subOptionName2) const {
  JsonIt optValIt = get(exchangeName, subOptionName1, subOptionName2);

  CurrencyVector ret;
  ret.reserve(optValIt->size());
  assert(optValIt->is_array());
  for (const auto& val : *optValIt) {
    ret.emplace_back(val.get<std::string_view>());
  }
  return ret;
}

}  // namespace cct