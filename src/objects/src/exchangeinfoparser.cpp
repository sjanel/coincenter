#include "exchangeinfoparser.hpp"

#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_log.hpp"
#include "exchangeinfodefault.hpp"

namespace cct {

json LoadExchangeConfigData(std::string_view dataDir, std::span<const std::string_view> allOptionNames) {
  static constexpr std::string_view kExchangeConfigFileName = "exchangeconfig.json";
  File exchangeConfigFile(dataDir, File::Type::kStatic, kExchangeConfigFileName, File::IfNotFound::kNoThrow);
  json jsonData = exchangeConfigFile.readJson();
  if (jsonData.empty()) {
    // Create a file with default values. User can then update them as he wishes.
    log::warn("No {} file found. Creating a default one which can be updated freely at your convenience",
              kExchangeConfigFileName);
    jsonData = kDefaultConfig;
    exchangeConfigFile.write(jsonData);
  } else {
    for (std::string_view optName : allOptionNames) {
      if (!jsonData.contains(optName)) {  // basic backward compatibility
        log::error("Invalid {} file (or wrong format), using default configuration instead", kExchangeConfigFileName);
        log::error("Follow same pattern as default one in 'kDefaultConfig'");
        jsonData = kDefaultConfig;
        break;
      }
    }
  }

  return jsonData;
}

TopLevelOption::TopLevelOption(const json& jsonData, std::string_view optionName) {
  auto optIt = jsonData.find(optionName);
  if (optIt == jsonData.end()) {
    throw exception("Unexpected exchange config file content");
  }
  _defaultPart = optIt->find("default");
  _hasDefaultPart = _defaultPart != optIt->end();
  _exchangePart = optIt->find("exchange");
  _hasExchangePart = _exchangePart != optIt->end();
}

string TopLevelOption::getCSVUnion(std::string_view exchangeName, std::string_view subOptionName) const {
  string ret;
  const auto appendFunc = [&](JsonIt it) {
    JsonIt optValIt = it->find(subOptionName);
    if (optValIt != it->end()) {
      if (!ret.empty()) {
        ret.push_back(',');
      }
      ret.append(optValIt->get<std::string_view>());
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

}  // namespace cct