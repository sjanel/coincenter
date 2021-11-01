#pragma once

#include <span>
#include <string_view>

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

json LoadExchangeConfigData(std::string_view dataDir, std::span<const std::string_view> allOptionNames);

/// Represents a top level option in the exchange config file
class TopLevelOption {
 public:
  // Top level option names
  static constexpr std::string_view kAssetsOptionStr = "asset";
  static constexpr std::string_view kQueryOptionStr = "query";
  static constexpr std::string_view kTradeFeesOptionStr = "tradefees";
  static constexpr std::string_view kWithdrawOptionStr = "withdraw";

  using JsonIt = json::const_iterator;

  TopLevelOption(const json& jsonData, std::string_view optionName);

  /// Get the first defined value of given sub option name, traversing the config options from bottom to up.
  /// @param def returned value will have this default value if no value can be found at all
  template <class T>
  T getBottomUp(std::string_view exchangeName, std::string_view subOptionName, T def = T()) const {
    if (_hasExchangePart) {
      JsonIt it = _exchangePart->find(exchangeName);
      if (it != _exchangePart->end()) {
        JsonIt optValIt = it->find(subOptionName);
        if (optValIt != it->end()) {
          // Exchange defined the option, it has priority, return it
          return optValIt->get<T>();
        }
      }
    }
    if (_hasDefaultPart) {
      JsonIt optValIt = _defaultPart->find(subOptionName);
      if (optValIt != _defaultPart->end()) {
        // Default defined the option
        return optValIt->get<T>();
      }
    }
    return def;
  }

  /// Create a string which is an aggregate of comma separated values of all option levels
  string getCSVUnion(std::string_view exchangeName, std::string_view subOptionName) const;

 private:
  JsonIt _defaultPart;
  JsonIt _exchangePart;
  bool _hasDefaultPart;
  bool _hasExchangePart;
};
}  // namespace cct