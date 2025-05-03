#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cct_config.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_json.hpp"
#include "static_string_view_helpers.hpp"
#include "string-equal-ignore-case.hpp"

namespace cct {

/**
 * Get the string representation of an enum value, provided that enum values are contiguous and start at 0, and that
 * they are specialized with glz::meta.
 */
constexpr std::string_view EnumToString(auto enumValue) {
  using T = std::remove_cvref_t<decltype(enumValue)>;
  static_assert(std::is_enum_v<T>, "EnumToString can only be used with enum types");
  return json::reflect<T>::keys[static_cast<std::underlying_type_t<T>>(enumValue)];
}

/**
 * Attempts to convert a string to an enum value, provided that enum values are contiguous and start at 0, and that
 * they are specialized with glz::meta.
 */
template <class EnumT, bool CaseInsensitive = false>
  requires(std::is_enum_v<EnumT>)
constexpr EnumT EnumFromString(std::string_view str) {
  struct KeyIdx {
    std::string_view key;
    std::size_t idx;
  };

  CCT_STATIC_CONSTEXPR_IN_CONSTEXPR_FUNC auto kSortedKeys = []() {
    auto keys = json::reflect<EnumT>::keys;

    std::array<KeyIdx, std::size(keys)> sortedKeys;
    std::ranges::transform(keys, sortedKeys.begin(), [i = 0U](auto key) mutable { return KeyIdx{key, i++}; });

    std::ranges::sort(sortedKeys, [](const auto &lhs, const auto &rhs) { return lhs.key < rhs.key; });
    return sortedKeys;
  }();

  auto pos =
      std::ranges::lower_bound(kSortedKeys, KeyIdx{str, 0},
                               [](const auto &lhs, const auto &rhs) {
                                 return CaseInsensitive ? CaseInsensitiveLess(lhs.key, rhs.key) : lhs.key < rhs.key;
                               }) -
      std::begin(kSortedKeys);
  if (std::cmp_equal(pos, std::size(kSortedKeys)) ||
      (CaseInsensitive ? !CaseInsensitiveEqual(kSortedKeys[pos].key, str) : kSortedKeys[pos].key != str)) {
    static constexpr std::string_view kSep = "|";
    CCT_STATIC_CONSTEXPR_IN_CONSTEXPR_FUNC std::string_view kConcatenatedKeys =
        make_joined_string_view<kSep, json::reflect<EnumT>::keys>::value;

    throw invalid_argument("Bad enum value {} among {}", str, kConcatenatedKeys);
  }
  return static_cast<EnumT>(kSortedKeys[pos].idx);
}

template <class EnumT>
  requires(std::is_enum_v<EnumT>)
constexpr EnumT EnumFromStringCaseInsensitive(std::string_view str) {
  return EnumFromString<EnumT, true>(str);
}

}  // namespace cct