#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "cct_hash.hpp"
#include "cct_log.hpp"
#include "toupperlower.hpp"

namespace cct {

/// Lightweight object representing a currency code with its acronym. Can be used as a key.
/// Can be used to represent a fiat currency or a coin (for the latter, acronym is expected to be 7 chars long maximum)
class CurrencyCode {
 public:
  static constexpr int kAcronymMaxLen = 7;

  using AcronymType = std::array<char, kAcronymMaxLen>;  // warning: not null terminated

  /// Constructs a neutral currency code.
  constexpr CurrencyCode() noexcept : _data() {}

  /// Constructs a currency code from a static char array.
  /// Note: spaces are not skipped. If any, there will be captured as part of the code, which is probably unexpected.
  template <unsigned N, std::enable_if_t<N <= kAcronymMaxLen + 1, bool> = true>
  constexpr CurrencyCode(const char (&acronym)[N]) noexcept {
    set(acronym);
  }

  /// Constructs a currency code from given string.
  /// Note: spaces are not skipped. If any, there will be captured as part of the code, which is probably unexpected.
  constexpr CurrencyCode(std::string_view acronym) {
    if (_data.size() < acronym.size()) {
      if (!std::is_constant_evaluated()) {
        log::debug("Acronym {} is too long, truncating to {} chars", acronym, kAcronymMaxLen);
      }
      acronym.remove_suffix(acronym.size() - _data.size());
    }
    set(acronym);
  }

  constexpr uint64_t size() const { return std::ranges::find(_data, '\0') - _data.begin(); }

  /// Get a string view of this CurrencyCode, trimmed.
  constexpr std::string_view str() const { return std::string_view(_data.begin(), std::ranges::find(_data, '\0')); }

  /// Returns a 64 bits code
  constexpr uint64_t code() const noexcept {
    uint64_t ret = _data[6];
    ret |= static_cast<uint64_t>(_data[5]) << 8;
    ret |= static_cast<uint64_t>(_data[4]) << 16;
    ret |= static_cast<uint64_t>(_data[3]) << 24;
    ret |= static_cast<uint64_t>(_data[2]) << 32;
    ret |= static_cast<uint64_t>(_data[1]) << 40;
    ret |= static_cast<uint64_t>(_data[0]) << 48;
    return ret;
  }

  constexpr bool isNeutral() const noexcept { return _data.front() == '\0'; }

  constexpr auto operator<=>(const CurrencyCode &) const = default;

  constexpr bool operator==(const CurrencyCode &) const = default;

  friend std::ostream &operator<<(std::ostream &os, const CurrencyCode &c) {
    os << c.str();
    return os;
  }

 private:
  AcronymType _data;

  constexpr inline void set(std::string_view acronym) {
    // Fill extra chars to 0 is important as we always read them for code generation
    std::fill(std::transform(acronym.begin(), acronym.end(), _data.begin(), toupper), _data.end(), '\0');
  }
};

}  // namespace cct

// Specialize std::hash<CurrencyCode> for easy usage of CurrencyCode as unordered_map key
namespace std {
template <>
struct hash<cct::CurrencyCode> {
  auto operator()(const cct::CurrencyCode &c) const { return cct::HashValue64(c.code()); }
};
}  // namespace std
