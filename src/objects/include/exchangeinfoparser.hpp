#pragma once

#include <string_view>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "durationstring.hpp"
#include "monetaryamount.hpp"

namespace cct {
class LoadConfiguration;

json LoadExchangeConfigData(const LoadConfiguration& loadConfiguration);

/// Represents a top level option in the exchange config file
class TopLevelOption {
 public:
  // Top level option names
  static constexpr std::string_view kAssetsOptionStr = "asset";
  static constexpr std::string_view kQueryOptionStr = "query";
  static constexpr std::string_view kTradeFeesOptionStr = "tradefees";
  static constexpr std::string_view kWithdrawOptionStr = "withdraw";

  using JsonIt = json::const_iterator;
  using CurrencyVector = vector<CurrencyCode>;
  using MonetaryAmountVector = vector<MonetaryAmount>;

  TopLevelOption(const json& jsonData, std::string_view optionName);

  /// Get the first defined string of given sub option name, traversing the config options from bottom to up.
  std::string_view getStr(std::string_view exchangeName, std::string_view subOptionName1,
                          std::string_view subOptionName2 = "") const {
    return get(exchangeName, subOptionName1, subOptionName2)->get<std::string_view>();
  }

  /// Get the first defined duration of given sub option name, traversing the config options from bottom to up.
  Duration getDuration(std::string_view exchangeName, std::string_view subOptionName1,
                       std::string_view subOptionName2 = "") const {
    return ParseDuration(getStr(exchangeName, subOptionName1, subOptionName2));
  }

  /// Get the first defined int of given sub option name, traversing the config options from bottom to up.
  int getInt(std::string_view exchangeName, std::string_view subOptionName1,
             std::string_view subOptionName2 = "") const {
    return get(exchangeName, subOptionName1, subOptionName2)->get<int>();
  }

  /// Get the first defined bool of given sub option name, traversing the config options from bottom to up.
  bool getBool(std::string_view exchangeName, std::string_view subOptionName1,
               std::string_view subOptionName2 = "") const {
    return get(exchangeName, subOptionName1, subOptionName2)->get<bool>();
  }

  /// Create an unordered aggregation of currencies from array string values of all option levels
  CurrencyVector getUnorderedCurrencyUnion(std::string_view exchangeName, std::string_view subOptionName) const;

  /// Get the array of currencies from array string values traversing the config options from bottom to up.
  CurrencyVector getCurrenciesArray(std::string_view exchangeName, std::string_view subOptionName1,
                                    std::string_view subOptionName2 = "") const {
    return getArray<CurrencyCode>(exchangeName, subOptionName1, subOptionName2);
  }

  /// Get the array of monetary amounts
  MonetaryAmountVector getMonetaryAmountsArray(std::string_view exchangeName, std::string_view subOptionName1,
                                               std::string_view subOptionName2 = "") const {
    return getArray<MonetaryAmount>(exchangeName, subOptionName1, subOptionName2);
  }

 private:
  JsonIt get(std::string_view exchangeName, std::string_view subOptionName1,
             std::string_view subOptionName2 = "") const;

  template <class ValueType>
  vector<ValueType> getArray(std::string_view exchangeName, std::string_view subOptionName1,
                             std::string_view subOptionName2 = "") const {
    JsonIt optValIt = get(exchangeName, subOptionName1, subOptionName2);
    if (!optValIt->is_array()) {
      string errMsg(subOptionName1);
      if (!subOptionName2.empty()) {
        errMsg.push_back('.');
        errMsg.append(subOptionName2);
      }
      errMsg.append(" should be an array for ");
      errMsg.append(exchangeName);
      throw exception(std::move(errMsg));
    }

    vector<ValueType> ret;
    ret.reserve(optValIt->size());
    for (const auto& val : *optValIt) {
      ret.emplace_back(val.get<std::string_view>());
    }
    return ret;
  }

  JsonIt _defaultPart;
  JsonIt _exchangePart;
  bool _hasDefaultPart;
  bool _hasExchangePart;
};
}  // namespace cct