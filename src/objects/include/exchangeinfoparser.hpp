#pragma once

#include <string_view>

#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "currencycodevector.hpp"
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

  using MonetaryAmountVector = vector<MonetaryAmount>;

  /// @brief Create a TopLevelOption from given json data.
  /// @param optionName top level option name
  /// @param defaultJsonData the json containing the personal exchange config data
  /// @param personalJsonData the json containing the personal exchange config data
  TopLevelOption(std::string_view optionName, const json& defaultJsonData, const json& personalJsonData);

  /// Get the first defined string of given sub option name, traversing the config options from bottom to up.
  std::string_view getStr(std::string_view exchangeName, std::string_view subOptionName1,
                          std::string_view subOptionName2 = "") {
    return get(exchangeName, subOptionName1, subOptionName2)->get<std::string_view>();
  }

  /// Get the first defined duration of given sub option name, traversing the config options from bottom to up.
  Duration getDuration(std::string_view exchangeName, std::string_view subOptionName1,
                       std::string_view subOptionName2 = "") {
    return ParseDuration(getStr(exchangeName, subOptionName1, subOptionName2));
  }

  /// Get the first defined int of given sub option name, traversing the config options from bottom to up.
  int getInt(std::string_view exchangeName, std::string_view subOptionName1, std::string_view subOptionName2 = "") {
    return get(exchangeName, subOptionName1, subOptionName2)->get<int>();
  }

  /// Get the first defined bool of given sub option name, traversing the config options from bottom to up.
  bool getBool(std::string_view exchangeName, std::string_view subOptionName1, std::string_view subOptionName2 = "") {
    return get(exchangeName, subOptionName1, subOptionName2)->get<bool>();
  }

  /// Create an unordered aggregation of currencies from array string values of all option levels
  CurrencyCodeVector getUnorderedCurrencyUnion(std::string_view exchangeName, std::string_view subOptionName);

  /// Get the array of currencies from array string values traversing the config options from bottom to up.
  CurrencyCodeVector getCurrenciesArray(std::string_view exchangeName, std::string_view subOptionName1,
                                        std::string_view subOptionName2 = "") {
    return getArray<CurrencyCode>(exchangeName, subOptionName1, subOptionName2);
  }

  /// Get the array of monetary amounts
  MonetaryAmountVector getMonetaryAmountsArray(std::string_view exchangeName, std::string_view subOptionName1,
                                               std::string_view subOptionName2 = "") {
    return getArray<MonetaryAmount>(exchangeName, subOptionName1, subOptionName2);
  }

  const json& getReadValues() const { return _readValues; }

 private:
  using JsonIt = json::const_iterator;

  struct DataSource {
#ifndef CCT_AGGR_INIT_CXX20
    DataSource(JsonIt it, bool isPersonal, bool isExchange) : it(it), isPersonal(isPersonal), isExchange(isExchange) {}
#endif

    JsonIt exchangeIt(std::string_view exchangeName) const {
      JsonIt exchangeIt = it->find(exchangeName);
      return exchangeIt == it->end() ? it : exchangeIt;
    }

    JsonIt it;
    bool isPersonal;
    bool isExchange;
  };

  JsonIt get(std::string_view exchangeName, std::string_view subOptionName1, std::string_view subOptionName2 = "");

  void setReadValue(const DataSource& dataSource, std::string_view exchangeName, std::string_view subOptionName1,
                    std::string_view subOptionName2, JsonIt valueIt);

  template <class ValueType>
  vector<ValueType> getArray(std::string_view exchangeName, std::string_view subOptionName1,
                             std::string_view subOptionName2 = "") {
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

  json _readValues;
  FixedCapacityVector<DataSource, 4> _orderedDataSource;
};
}  // namespace cct