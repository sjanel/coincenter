#pragma once

#include <spdlog/fmt/bundled/format.h>

#include <compare>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "cct_hash.hpp"
#include "cct_invalid_argument_exception.hpp"

namespace cct {

struct CurrencyCodeConstants {
  static constexpr int kMaxLen = 10;

  static constexpr uint64_t kNbBitsChar = 6;
  static constexpr uint64_t kNbBitsNbDecimals = 4;

  static constexpr uint64_t kNbDecimals4Mask = (1ULL << kNbBitsNbDecimals) - 1ULL;

  static constexpr uint64_t kFirstCharMask = ~((1ULL << (kNbBitsNbDecimals + (kMaxLen - 1) * kNbBitsChar)) - 1ULL);

  static constexpr uint64_t kBeforeLastCharMask = kFirstCharMask >> (kNbBitsChar * (kMaxLen - 2));

  static constexpr int64_t kMaxNbDecimalsLongCurrencyCode = 15;  // 2^4 - 1

  static constexpr char kFirstAuthorizedLetter = 33;  // '!'
  static constexpr char kLastAuthorizedLetter = 95;   // '_'
};

class CurrencyCodeIterator {
 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = char;
  using pointer = const char *;
  using reference = const char &;

  constexpr auto operator<=>(const CurrencyCodeIterator &) const noexcept = default;
  bool operator==(const CurrencyCodeIterator &) const noexcept = default;

  CurrencyCodeIterator &operator++() noexcept {  // Prefix increment
    ++_pos;
    return *this;
  }

  CurrencyCodeIterator &operator--() noexcept {  // Prefix decrement
    --_pos;
    return *this;
  }

  CurrencyCodeIterator operator++(int) noexcept {  // Postfix increment
    CurrencyCodeIterator oldSelf = *this;
    ++*this;
    return oldSelf;
  }

  CurrencyCodeIterator operator--(int) noexcept {  // Postfix decrement
    CurrencyCodeIterator oldSelf = *this;
    --*this;
    return oldSelf;
  }

  char operator*() const noexcept {
    return static_cast<char>((_data >> (CurrencyCodeConstants::kNbBitsNbDecimals +
                                        CurrencyCodeConstants::kNbBitsChar *
                                            (CurrencyCodeConstants::kMaxLen - static_cast<int>(_pos) - 1))) &
                             ((1ULL << CurrencyCodeConstants::kNbBitsChar) - 1ULL)) +
           CurrencyCodeConstants::kFirstAuthorizedLetter - 1;
  }

 private:
  friend class CurrencyCode;

  // Default constructor needed for an iterator in C++20
  CurrencyCodeIterator() noexcept : _data(0), _pos(0) {}

  explicit CurrencyCodeIterator(uint64_t data, uint64_t pos = 0) : _data(data), _pos(pos) {}

  uint64_t _data;
  uint64_t _pos;
};

/// Lightweight object representing a currency code with its acronym. Can be used as a key.
/// Can be used to represent a fiat currency or a coin (for the latter, acronym is expected to be 10 chars long maximum)
class CurrencyCode {
 public:
  using iterator = CurrencyCodeIterator;
  using const_iterator = CurrencyCodeIterator;

  static constexpr auto kMaxLen = CurrencyCodeConstants::kMaxLen;

  /// Constructs a neutral currency code.
  constexpr CurrencyCode() noexcept : _data(0) {}

  /// Constructs a currency code from a char array.
  template <unsigned N, std::enable_if_t<N <= kMaxLen + 1, bool> = true>
  constexpr CurrencyCode(const char (&acronym)[N]) : _data(ComputeData(acronym)) {}

  /// Constructs a currency code from given string.
  /// If number of chars in 'acronym' is higher than 'kMaxLen', exception will be raised.
  /// Note: spaces are not skipped. If any, they will be captured as part of the code, which is probably unexpected.
  constexpr CurrencyCode(std::string_view acronym) {
    if (acronym.length() > kMaxLen) {
      throw invalid_argument("Acronym is too long to fit in a CurrencyCode");
    }
    _data = ComputeData(acronym);
  }

  CurrencyCodeIterator begin() const { return CurrencyCodeIterator(_data); }
  CurrencyCodeIterator end() const { return CurrencyCodeIterator(_data, size()); }

  constexpr uint64_t size() const {
    uint64_t s = 0;
    while (static_cast<int>(s) < kMaxLen &&
           (_data & (CurrencyCodeConstants::kFirstCharMask >> (CurrencyCodeConstants::kNbBitsChar * s)))) {
      ++s;
    }
    return s;
  }

  constexpr uint64_t length() const { return size(); }

  /// Get a string of this CurrencyCode, trimmed.
  string str() const {
    string s;
    appendStr(s);
    return s;
  }

  /// Append currency string reprensentation to given string.
  void appendStr(string &s) const {
    for (uint32_t charPos = 0; charPos < kMaxLen; ++charPos) {
      char c = (*this)[charPos];
      if (c == CurrencyCodeConstants::kFirstAuthorizedLetter - 1) {
        break;
      }
      s.push_back(c);
    }
  }

  /// Returns a 64 bits code
  constexpr uint64_t code() const noexcept { return _data; }

  constexpr bool isNeutral() const noexcept { return !(_data & CurrencyCodeConstants::kFirstCharMask); }

  constexpr char operator[](uint32_t pos) const {
    return static_cast<char>((_data >> (CurrencyCodeConstants::kNbBitsNbDecimals +
                                        CurrencyCodeConstants::kNbBitsChar * (kMaxLen - static_cast<int>(pos) - 1))) &
                             ((1ULL << CurrencyCodeConstants::kNbBitsChar) - 1ULL)) +
           CurrencyCodeConstants::kFirstAuthorizedLetter - 1;
  }

  constexpr auto operator<=>(const CurrencyCode &) const = default;

  constexpr bool operator==(const CurrencyCode &) const = default;

  friend std::ostream &operator<<(std::ostream &os, const CurrencyCode &cur) {
    for (uint32_t charPos = 0; charPos < kMaxLen; ++charPos) {
      char c = cur[charPos];
      if (c == CurrencyCodeConstants::kFirstAuthorizedLetter - 1) {
        break;
      }
      os << c;
    }
    return os;
  }

 private:
  friend class MonetaryAmount;

  // bitmap with 10 words of 6 bits (from ascii [33, 95]) + 4 extra bits that will be used by
  // MonetaryAmount to hold number of decimals (max 15)
  uint64_t _data;

  explicit constexpr CurrencyCode(uint64_t data) : _data(data) {}

  constexpr bool isLongCurrencyCode() const { return _data & CurrencyCodeConstants::kBeforeLastCharMask; }

  constexpr void setNbDecimals(int8_t nbDecimals) {
    if (isLongCurrencyCode()) {
      if (!std::is_constant_evaluated() && nbDecimals > CurrencyCodeConstants::kMaxNbDecimalsLongCurrencyCode) {
        throw invalid_argument("Too many decimals for long currency code");
      }
      // For currency codes whose length is > 8, only 15 digits are supported
      _data = static_cast<uint64_t>(nbDecimals) + (_data & (~CurrencyCodeConstants::kNbDecimals4Mask));
    } else {
      // max 64 decimals for currency codes whose length is maximum 8 (most cases)
      _data =
          (static_cast<uint64_t>(nbDecimals) << CurrencyCodeConstants::kNbBitsNbDecimals) +
          (_data & ~((1ULL << (CurrencyCodeConstants::kNbBitsChar + CurrencyCodeConstants::kNbBitsNbDecimals)) - 1ULL));
    }
  }

  constexpr int8_t nbDecimals() const {
    if (isLongCurrencyCode()) {
      // For currency codes whose length is > 8, only 15 digits are supported
      return static_cast<int8_t>(_data & CurrencyCodeConstants::kNbDecimals4Mask);
    }
    // max 64 decimals for currency codes whose length is maximum 8 (most cases)
    return static_cast<int8_t>((_data >> CurrencyCodeConstants::kNbBitsNbDecimals) &
                               ((1ULL << CurrencyCodeConstants::kNbBitsChar) - 1ULL));
  }

  constexpr CurrencyCode toNeutral() const {
    // keep number of decimals
    return CurrencyCode(
        _data &
        (isLongCurrencyCode()
             ? CurrencyCodeConstants::kNbDecimals4Mask
             : (1ULL << (CurrencyCodeConstants::kNbBitsChar + CurrencyCodeConstants::kNbBitsNbDecimals)) - 1ULL));
  }

  constexpr CurrencyCode withNoDecimalsPart() const {
    // Keep currency, not decimals
    return CurrencyCode(
        _data &
        ~(isLongCurrencyCode()
              ? CurrencyCodeConstants::kNbDecimals4Mask
              : (1ULL << (CurrencyCodeConstants::kNbBitsChar + CurrencyCodeConstants::kNbBitsNbDecimals)) - 1ULL));
  }

  static constexpr uint64_t ComputeData(std::string_view acronym) {
    uint64_t ret = 0;
    int charPos = 0;
    for (char c : acronym) {
      if (c >= 'a') {
        if (c > 'z') {
          throw invalid_argument("Unexpected char in acronym");
        }
        c -= 'a' - 'A';
      }
      if (c < CurrencyCodeConstants::kFirstAuthorizedLetter || c > CurrencyCodeConstants::kLastAuthorizedLetter) {
        throw invalid_argument("Unexpected char in acronym");
      }

      ret |= static_cast<uint64_t>(c - CurrencyCodeConstants::kFirstAuthorizedLetter + 1)
             << (CurrencyCodeConstants::kNbBitsNbDecimals +
                 CurrencyCodeConstants::kNbBitsChar * static_cast<uint64_t>(kMaxLen - charPos - 1));

      ++charPos;
    }
    return ret;
  }
};

}  // namespace cct

template <>
struct fmt::formatter<cct::CurrencyCode> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const cct::CurrencyCode &cur, FormatContext &ctx) const -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    for (char c : cur) {
      *ctx.out() = c;
      ++ctx.out();
    }
    return ctx.out();
  }
};

// Specialize std::hash<CurrencyCode> for easy usage of CurrencyCode as unordered_map key
namespace std {
template <>
struct hash<cct::CurrencyCode> {
  auto operator()(const cct::CurrencyCode &c) const { return cct::HashValue64(c.code()); }
};
}  // namespace std
