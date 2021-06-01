#pragma once

#include <string.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <string_view>
#include <type_traits>

#include "cct_config.hpp"
#include "cct_hash.hpp"
#include "cct_log.hpp"
#include "cct_toupperlower.hpp"

namespace cct {

/// Lightweight object representing a currency code with its acronym. Can be used as a key.
/// Can be used to represent a fiat currency or a coin (for the latter, acronym is expected to be 7 chars long maximum)
class CurrencyCode {
 public:
  static constexpr int kAcronymMaxLen = 7;

  using AcronymType = std::array<char, kAcronymMaxLen>;  // warning: not null terminated

  static const CurrencyCode kNeutral;

  constexpr CurrencyCode() noexcept { std::fill(_data.begin(), _data.end(), '\0'); }

  template <unsigned N, std::enable_if_t<N <= sizeof(AcronymType), bool> = true>
  constexpr CurrencyCode(const char (&acronym)[N]) noexcept {
    // Fill extra chars to 0 is important as we always read them for code generation
    std::fill(
        std::transform(std::begin(acronym), std::end(acronym), _data.begin(), [](char c) { return cct::toupper(c); }),
        _data.end(), '\0');
  }

  constexpr CurrencyCode(std::string_view acronym) {
    if (CCT_UNLIKELY(_data.size() < acronym.size())) {
      if (!std::is_constant_evaluated()) {
        log::debug("Acronym {} is too long, truncating to {}", acronym, acronym.substr(0, _data.size()));
      }
      acronym.remove_suffix(acronym.size() - _data.size());
    }
    std::fill(std::transform(acronym.begin(), acronym.end(), _data.begin(), [](char c) { return cct::toupper(c); }),
              _data.end(), '\0');  // Fill extra chars to 0 is important as we always read them for code generation
  }

  constexpr std::string_view str() const {
    return std::string_view(_data.begin(), std::find(_data.begin(), _data.end(), '\0'));
  }

  std::string toString() const { return std::string(str()); }

  /// Returns a 64 bits code
  constexpr uint64_t code() const {
    uint64_t ret = _data[0];
    ret |= static_cast<uint64_t>(_data[1]) << 8;
    ret |= static_cast<uint64_t>(_data[2]) << 16;
    ret |= static_cast<uint64_t>(_data[3]) << 24;
    ret |= static_cast<uint64_t>(_data[4]) << 32;
    ret |= static_cast<uint64_t>(_data[5]) << 40;
    ret |= static_cast<uint64_t>(_data[6]) << 48;
    return ret;
  }

  constexpr bool isNeutral() const { return _data.front() == '\0'; }

  void print(std::ostream &os) const { os << str(); }

  constexpr bool operator<(CurrencyCode o) const {
    return std::lexicographical_compare(_data.begin(), _data.end(), o._data.begin(), o._data.end());
  }
  constexpr bool operator<=(CurrencyCode o) const { return !(o < *this); }
  constexpr bool operator>(CurrencyCode o) const { return o < *this; }
  constexpr bool operator>=(CurrencyCode o) const { return !(*this < o); }
  constexpr bool operator==(CurrencyCode o) const { return std::equal(_data.begin(), _data.end(), o._data.begin()); }
  constexpr bool operator!=(CurrencyCode o) const { return !(*this == o); }

 private:
  AcronymType _data;
};

std::ostream &operator<<(std::ostream &os, const CurrencyCode &c);
}  // namespace cct

/// Specialize std::hash<CurrencyCode> for easy usage of CurrencyCode as unordered_map key
namespace std {
template <>
struct hash<cct::CurrencyCode> {
  std::size_t operator()(const cct::CurrencyCode &c) const { return cct::HashValue64(c.code()); }
};
}  // namespace std
