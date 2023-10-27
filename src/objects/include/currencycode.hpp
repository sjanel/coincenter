#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "cct_format.hpp"
#include "cct_hash.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "toupperlower.hpp"

namespace cct {

struct CurrencyCodeBase {
  static constexpr int kMaxLen = 10;

  static constexpr uint64_t kNbBitsChar = 6;
  static constexpr uint64_t kNbBitsNbDecimals = 4;

  static constexpr uint64_t kNbDecimals4Mask = (1ULL << kNbBitsNbDecimals) - 1ULL;
  static constexpr uint64_t kNbDecimals6Mask = (1ULL << 6ULL) - 1ULL;

  static constexpr uint64_t kFirstCharMask = ~((1ULL << (kNbBitsNbDecimals + (kMaxLen - 1) * kNbBitsChar)) - 1ULL);

  static constexpr uint64_t kBeforeLastCharMask = kFirstCharMask >> (kNbBitsChar * (kMaxLen - 2));

  static constexpr int64_t kMaxNbDecimalsLongCurrencyCode = 15;  // 2^4 - 1

  static constexpr char kFirstAuthorizedLetter = 32;  // ' '
  static constexpr char kLastAuthorizedLetter = 95;   // '_'

  static constexpr char CharAt(uint64_t data, int pos) noexcept {
    return static_cast<char>((data >> (kNbBitsNbDecimals + kNbBitsChar * (kMaxLen - pos - 1))) &
                             ((1ULL << kNbBitsChar) - 1ULL)) +
           kFirstAuthorizedLetter;
  }

  static constexpr uint64_t DecimalsMask(bool isLongCurrencyCode) {
    return isLongCurrencyCode ? kNbDecimals4Mask : kNbDecimals6Mask;
  }

  static constexpr uint64_t StrToBmp(std::string_view acronym) {
    uint64_t ret = 0;
    uint32_t charPos = kMaxLen;
    for (char ch : acronym) {
      if (ch >= 'a') {
        if (ch > 'z') {
          throw invalid_argument("Unexpected char '{}' in acronym '{}'", ch, acronym);
        }
        ch -= 'a' - 'A';
      } else if (ch <= kFirstAuthorizedLetter || ch > kLastAuthorizedLetter) {
        throw invalid_argument("Unexpected char '{}' in acronym '{}'", ch, acronym);
      }

      ret |= static_cast<uint64_t>(ch - kFirstAuthorizedLetter) << (kNbBitsNbDecimals + kNbBitsChar * --charPos);
    }
    return ret;
  }
};

class CurrencyCodeIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = char;
  using pointer = const char *;
  using reference = const char &;

  constexpr std::strong_ordering operator<=>(const CurrencyCodeIterator &) const noexcept = default;

  constexpr bool operator==(const CurrencyCodeIterator &) const noexcept = default;

  constexpr CurrencyCodeIterator &operator++() noexcept {  // Prefix increment
    ++_pos;
    return *this;
  }

  constexpr CurrencyCodeIterator &operator--() noexcept {  // Prefix decrement
    --_pos;
    return *this;
  }

  constexpr CurrencyCodeIterator operator++(int) noexcept {  // Postfix increment
    CurrencyCodeIterator oldSelf = *this;
    ++*this;
    return oldSelf;
  }

  constexpr CurrencyCodeIterator operator--(int) noexcept {  // Postfix decrement
    CurrencyCodeIterator oldSelf = *this;
    --*this;
    return oldSelf;
  }

  constexpr char operator*() const noexcept { return CurrencyCodeBase::CharAt(_data, static_cast<int>(_pos)); }
  // operator-> cannot be implemented here - we would need a const char * but it's not possible.

 private:
  friend class CurrencyCode;

  // Default constructor needed for an iterator in C++20
  constexpr explicit CurrencyCodeIterator(uint64_t data = 0, uint64_t pos = 0) noexcept : _data(data), _pos(pos) {}

  uint64_t _data;
  uint64_t _pos;
};

/// Lightweight object representing a currency code with its acronym.
/// Can be used to represent a fiat currency or a coin (for the latter, acronym is expected to be 10 chars long maximum)
/// It supports up to 10 characters and weights only 64 bits, with characters between '!' and '_' in the ASCII code,
/// each coded on 6 bits. Space cannot be present in the currency code, they are coded as 6 bits of 0.
/// The last 4 bits are either unused, or used to store number of decimals of MonetaryAmount, internally. They are not
/// exposed publicly.
class CurrencyCode {
 public:
  using iterator = CurrencyCodeIterator;
  using const_iterator = iterator;

  static constexpr auto kMaxLen = CurrencyCodeBase::kMaxLen;

  /// Returns true if and only if a CurrencyCode can be constructed from 'curStr'.
  /// Note that an empty string is a valid representation of a CurrencyCode.
  static constexpr bool IsValid(std::string_view curStr) noexcept {
    return curStr.size() <= kMaxLen && std::ranges::all_of(curStr, [](char ch) {
             return ch > CurrencyCodeBase::kFirstAuthorizedLetter &&
                    (ch <= CurrencyCodeBase::kLastAuthorizedLetter || (ch >= 'a' && ch <= 'z'));
           });
  }

  /// Constructs a neutral currency code.
  constexpr CurrencyCode() noexcept : _data() {}

  /// Constructs a currency code from a char array.
  template <unsigned N, std::enable_if_t<N <= kMaxLen + 1, bool> = true>
  constexpr CurrencyCode(const char (&acronym)[N]) : _data(CurrencyCodeBase::StrToBmp(acronym)) {}

  /// Constructs a currency code from given string.
  /// If number of chars in 'acronym' is higher than 'kMaxLen', exception will be raised.
  /// Note: spaces are not skipped. If any, they will be captured as part of the code, which is probably unexpected.
  constexpr CurrencyCode(std::string_view acronym) {
    if (acronym.length() > kMaxLen) {
      throw invalid_argument("Acronym '{}' is too long to fit in a CurrencyCode", acronym);
    }
    _data = CurrencyCodeBase::StrToBmp(acronym);
  }

  constexpr const_iterator begin() const { return const_iterator(_data); }
  constexpr const_iterator end() const { return const_iterator(_data, size()); }

  constexpr uint64_t size() const {
    uint64_t sz = 0;
    while (static_cast<int>(sz) < kMaxLen &&
           (_data & (CurrencyCodeBase::kFirstCharMask >> (CurrencyCodeBase::kNbBitsChar * sz)))) {
      ++sz;
    }
    return sz;
  }

  constexpr uint64_t length() const { return size(); }

  /// Get a string of this CurrencyCode, trimmed.
  string str() const {
    string ret;
    appendStrTo(ret);
    return ret;
  }

  /// Return true if this currency code acronym is equal to given string.
  /// Comparison is case insensitive.
  constexpr bool iequal(std::string_view curStr) const {
    if (curStr.size() > kMaxLen) {
      return false;
    }
    for (uint32_t charPos = 0; charPos < kMaxLen; ++charPos) {
      const char ch = (*this)[charPos];
      if (ch == CurrencyCodeBase::kFirstAuthorizedLetter) {
        return curStr.size() == charPos;
      }
      if (curStr.size() == charPos || ch != toupper(curStr[charPos])) {
        return false;
      }
    }
    return true;
  }

  /// Append currency string representation to given string.
  void appendStrTo(string &str) const {
    const auto len = size();
    str.append(len, '\0');
    append(str.end() - len);
  }

  /// Append currency string representation to given output iterator
  template <class OutputIt>
  constexpr OutputIt append(OutputIt it) const {
    for (uint32_t charPos = 0; charPos < kMaxLen; ++charPos) {
      const char ch = (*this)[charPos];
      if (ch == CurrencyCodeBase::kFirstAuthorizedLetter) {
        break;
      }
      *it = ch;
      ++it;
    }
    return it;
  }

  /// Returns a 64 bits code
  constexpr uint64_t code() const noexcept { return _data; }

  constexpr bool isNeutral() const noexcept { return !(_data & CurrencyCodeBase::kFirstCharMask); }

  constexpr char operator[](uint32_t pos) const { return CurrencyCodeBase::CharAt(_data, static_cast<int>(pos)); }

  /// Note that this respects the lexicographical order - chars are encoded from the most significant bits first
  constexpr std::strong_ordering operator<=>(const CurrencyCode &) const noexcept = default;

  constexpr bool operator==(const CurrencyCode &) const noexcept = default;

  friend std::ostream &operator<<(std::ostream &os, const CurrencyCode &cur) {
    for (uint32_t charPos = 0; charPos < kMaxLen; ++charPos) {
      const char ch = cur[charPos];
      if (ch == CurrencyCodeBase::kFirstAuthorizedLetter) {
        break;
      }
      os << ch;
    }
    return os;
  }

 private:
  friend class MonetaryAmount;

  // bitmap with 10 words of 6 bits (from ascii [33, 95]) + 4 extra bits that will be used by
  // MonetaryAmount to hold number of decimals (max 15)
  // Example, with currency code "EUR":
  // 100101 110101 110010 000000 000000 000000 000000 000000 000000 000000 0000 = 10906733135072854016 (code)
  // |----| |----| |----| |----| |----| |----| |----| |----| |----| |----| |--|
  //  'E'    'U'    'R'    ' '    ' '    ' '    ' '    ' '    ' '    ' '
  uint64_t _data;

  explicit constexpr CurrencyCode(uint64_t data) : _data(data) {}

  constexpr bool isLongCurrencyCode() const { return _data & CurrencyCodeBase::kBeforeLastCharMask; }

  constexpr void setNbDecimals(int8_t nbDecimals) {
    // For currency codes whose length is > 8, only 15 digits are supported
    // max 64 decimals for currency codes whose length is maximum 8 (most cases)
    _data = static_cast<uint64_t>(nbDecimals) + (_data & (~CurrencyCodeBase::DecimalsMask(isLongCurrencyCode())));
  }

  constexpr int8_t nbDecimals() const {
    return static_cast<int8_t>(_data & CurrencyCodeBase::DecimalsMask(isLongCurrencyCode()));
  }

  constexpr CurrencyCode toNeutral() const {
    // keep number of decimals
    return CurrencyCode(_data & CurrencyCodeBase::DecimalsMask(isLongCurrencyCode()));
  }

  constexpr CurrencyCode withNoDecimalsPart() const {
    // Keep currency, not decimals
    return CurrencyCode(_data & ~CurrencyCodeBase::DecimalsMask(isLongCurrencyCode()));
  }

  /// Append currency string representation to given string, with a space before (used by MonetaryAmount)
  void appendStrWithSpaceTo(string &str) const {
    const auto len = size();
    str.append(len + 1UL, ' ');
    append(str.end() - len);
  }
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::CurrencyCode> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::CurrencyCode &cur, FormatContext &ctx) const -> decltype(ctx.out()) {
    return cur.append(ctx.out());
  }
};
#endif

// Specialize std::hash<CurrencyCode> for easy usage of CurrencyCode as unordered_map key
namespace std {
template <>
struct hash<cct::CurrencyCode> {
  auto operator()(const cct::CurrencyCode &currencyCode) const { return cct::HashValue64(currencyCode.code()); }
};
}  // namespace std
