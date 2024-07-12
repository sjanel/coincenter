#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <variant>

#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "duration-schema.hpp"
#include "exchange-name-enum.hpp"
#include "generic-object-json.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct::schema {

#define CCT_AUTO_TRADE_STOP_CRITERIA_TYPES duration, protectLoss, secureProfit

enum class AutoTradeStopCriteriaType : int8_t { CCT_AUTO_TRADE_STOP_CRITERIA_TYPES };

class AutoTradeStopCriteriaValue {
 public:
  AutoTradeStopCriteriaValue() = default;

  explicit AutoTradeStopCriteriaValue(std::string_view valueStr);

  ::cct::Duration duration() const { return std::get<::cct::Duration>(_value); }

  int maxEvolutionPercentage() const { return std::get<int>(_value); }

  std::size_t strLen() const;

  char *appendTo(char *buf) const;

  auto operator<=>(const AutoTradeStopCriteriaValue &) const = default;

 private:
  using Value = std::variant<int, ::cct::Duration>;

  Value _value;
};

struct AutoTradeStopCriterion {
  auto operator<=>(const AutoTradeStopCriterion &) const = default;

  AutoTradeStopCriteriaType type;
  AutoTradeStopCriteriaValue value;
};

struct AutoTradeMarketConfig {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  SmallVector<string, 2> accounts;
  string algorithmName;
  Duration repeatTime{std::chrono::seconds(5)};
  MonetaryAmount baseStartAmount;
  MonetaryAmount quoteStartAmount;

  vector<AutoTradeStopCriterion> stopCriteria;
};

using AutoTradeExchangeConfig = std::unordered_map<Market, AutoTradeMarketConfig>;

using AutoTradeConfig =
    FixedCapacityVector<std::pair<ExchangeNameEnum, AutoTradeExchangeConfig>, kNbSupportedExchanges>;

}  // namespace cct::schema

template <>
struct glz::meta<::cct::schema::AutoTradeStopCriteriaType> {
  using enum ::cct::schema::AutoTradeStopCriteriaType;

  static constexpr auto value = enumerate(CCT_AUTO_TRADE_STOP_CRITERIA_TYPES);
};

#undef CCT_AUTO_TRADE_STOP_CRITERIA_TYPES

namespace glz {
template <>
struct from<JSON, ::cct::schema::AutoTradeStopCriteriaValue> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) noexcept {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value = ::cct::schema::AutoTradeStopCriteriaValue(std::string_view(it, endIt));
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::schema::AutoTradeStopCriteriaValue> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    ::cct::details::ToStrLikeJson<Opts>(value, b, ix);
  }
};
}  // namespace glz
