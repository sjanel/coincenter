#pragma once

#include <charconv>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "cct_format.hpp"
#include "cct_hash.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "mathhelpers.hpp"

namespace cct {

/// Represents a fixed-precision decimal amount with a CurrencyCode (fiat or coin).
/// It is designed to be
///  - fast
///  - small (16 bytes only). Thus can be passed by copy instead of reference
///  - precise (amount is stored in a int64_t)
///  - optimized, predictive and exact for additions and subtractions (if no overflow during the operation)
///
/// It is easy and straightforward to use with string_view constructor and partial constexpr support.
///
/// A MonetaryAmount is only 16 bytes:
/// - One integral amount stored on 64 bits
/// - A CurrencyCode holding up to 10 chars + the number of decimals
///
/// It can support up to 17 decimals for currency codes whose length is less than 9,
/// and up to 15 decimals for currencies whose length is 9 or 10.
///
/// Examples: $50, -2.045 BTC.
/// The integral value stored in the MonetaryAmount is multiplied by 10^'_nbDecimals'
/// Its number of decimals is automatically adjusted and simplified.
class MonetaryAmount {
 public:
  using AmountType = int64_t;

  enum class RoundType : int8_t { kDown, kUp, kNearest };

  /// Constructs a MonetaryAmount with a value of 0 of neutral currency.
  constexpr MonetaryAmount() noexcept : _amount(0) {}

  /// Constructs a MonetaryAmount representing the integer 'amount' with a neutral currency
  constexpr explicit MonetaryAmount(std::integral auto amount) noexcept : _amount(amount) { sanitizeIntegralPart(); }

  /// Constructs a MonetaryAmount representing the integer 'amount' with a currency
  constexpr explicit MonetaryAmount(std::integral auto amount, CurrencyCode currencyCode) noexcept
      : _amount(amount), _curWithDecimals(currencyCode) {
    sanitizeIntegralPart();
  }

  /// Construct a new MonetaryAmount from a double.
  /// Precision is calculated automatically.
  explicit MonetaryAmount(double amount, CurrencyCode currencyCode = CurrencyCode());

  /// Construct a new MonetaryAmount from a double, with provided rounding and expected precision.
  MonetaryAmount(double amount, CurrencyCode currencyCode, RoundType roundType, int8_t nbDecimals);

  /// Constructs a new MonetaryAmount from an integral representation which is already multiplied by given
  /// number of decimals
  constexpr MonetaryAmount(AmountType amount, CurrencyCode currencyCode, int8_t nbDecimals) noexcept
      : _amount(amount), _curWithDecimals(currencyCode) {
    sanitizeDecimals(nbDecimals, maxNbDecimals());
    sanitizeIntegralPart();
  }

  /// Constructs a new MonetaryAmount from a string, containing an optional CurrencyCode.
  /// - If a currency is not present, assume default CurrencyCode
  /// - If the currency is too long to fit in a CurrencyCode, exception will be raised
  /// - If only a currency is given, invalid_argument exception will be raised
  /// - If given string is empty, it is equivalent to a default constructor
  ///
  /// A space can be present or not between the amount and the currency code.
  /// Beware however that if there is no space and the currency starts with a digit, parsing will consider
  /// the digit as part of the amount, which results in a wrong MonetaryAmount. Use a space to avoid
  /// ambiguity in this case.
  /// Examples: "10.5EUR" -> 10.5 units of currency EUR
  ///           "45 KRW" -> 45 units of currency KRW
  ///           "-345.8909" -> -345.8909 units of no currency
  ///           "36.61INCH" -> 36.63 units of currency INCH
  ///           "36.6 1INCH" -> 36.6 units of currency 1INCH
  explicit MonetaryAmount(std::string_view amountCurrencyStr);

  /// Constructs a new MonetaryAmount from a string representing the amount only and a currency code.
  /// Precision is calculated automatically.
  MonetaryAmount(std::string_view amountStr, CurrencyCode currencyCode);

  /// Constructs a new MonetaryAmount from another MonetaryAmount and a new CurrencyCode.
  /// Use this constructor to change currency of an existing MonetaryAmount.
  constexpr MonetaryAmount(MonetaryAmount monetaryAmount, CurrencyCode newCurrencyCode)
      : _amount(monetaryAmount._amount), _curWithDecimals(newCurrencyCode) {
    setNbDecimals(monetaryAmount.nbDecimals());
  }

  /// Get an integral representation of this MonetaryAmount multiplied by given number of decimals.
  /// If an overflow would occur for the resulting amount, return std::nullopt
  /// Example : "5.6235" with 6 decimals will return 5623500
  [[nodiscard]] std::optional<AmountType> amount(int8_t nbDecimals) const;

  /// Get the integer part of the amount of this MonetaryAmount.
  [[nodiscard]] constexpr AmountType integerPart() const {
    return _amount / ipow10(static_cast<uint8_t>(nbDecimals()));
  }

  /// Get the decimal part of the amount of this MonetaryAmount.
  /// Warning: starting zeros will not be part of the returned value. Use nbDecimals to retrieve the number of decimals
  /// of this MonetaryAmount.
  /// Example: "45.046" decimalPart() = 46
  [[nodiscard]] constexpr AmountType decimalPart() const;

  /// Get the amount of this MonetaryAmount in double format.
  [[nodiscard]] constexpr double toDouble() const {
    return static_cast<double>(_amount) / ipow10(static_cast<uint8_t>(nbDecimals()));
  }

  /// Check if given amount is close to this amount.
  /// Currency is not checked here, only amount
  [[nodiscard]] bool isCloseTo(MonetaryAmount otherAmount, double relativeDifference) const;

  [[nodiscard]] constexpr CurrencyCode currencyCode() const {
    // We do not want to expose private nb decimals bits to outside world
    return _curWithDecimals.withNoDecimalsPart();
  }

  [[nodiscard]] constexpr int8_t nbDecimals() const { return _curWithDecimals.nbDecimals(); }

  [[nodiscard]] constexpr int8_t maxNbDecimals() const {
    return _curWithDecimals.isLongCurrencyCode()
               ? CurrencyCodeBase::kMaxNbDecimalsLongCurrencyCode
               : std::numeric_limits<AmountType>::digits10 - 1;  // -1 as minimal nb digits of integral part
  }

  /// Returns the maximum number of decimals that this amount could hold, given its integral part.
  /// Examples:
  ///  0.00426622338114037 EUR -> 17
  ///  45.546675 EUR           -> 16
  [[nodiscard]] constexpr int8_t currentMaxNbDecimals() const {
    return static_cast<int8_t>(maxNbDecimals() - ndigits(integerPart()) + 1);
  }

  /// Converts current amount at given price.
  /// Example: ETH/EUR
  ///            2 ETH convertTo("1600 EUR")       = 3200 EUR
  ///            1500 EUR convertTo("0.0005 ETH")  = 0.75 ETH
  /// @return a monetary amount in the currency of given price
  [[nodiscard]] MonetaryAmount convertTo(MonetaryAmount price) const { return price * toNeutral(); }

  /// Rounds current monetary amount according to given step amount.
  /// CurrencyCode of 'step' is unused.
  /// Example: 123.45 with 0.1 as step will return 123.4.
  /// Assumption: 'step' should be strictly positive amount
  void round(MonetaryAmount step, RoundType roundType);

  /// Rounds current monetary amount according to given precision (number of decimals)
  void round(int8_t nbDecimals, RoundType roundType);

  [[nodiscard]] std::strong_ordering operator<=>(const MonetaryAmount &other) const;

  [[nodiscard]] constexpr bool operator==(const MonetaryAmount &other) const = default;

  [[nodiscard]] constexpr bool operator==(AmountType amount) const { return _amount == amount && nbDecimals() == 0; }
  friend constexpr bool operator==(AmountType amount, MonetaryAmount rhs) { return rhs == amount; }

  [[nodiscard]] constexpr auto operator<=>(AmountType amount) const {
    return _amount <=> amount * ipow10(static_cast<uint8_t>(nbDecimals()));
  }

  [[nodiscard]] friend constexpr auto operator<=>(AmountType amount, MonetaryAmount other) {
    return amount * ipow10(static_cast<uint8_t>(other.nbDecimals())) <=> other._amount;
  }

  [[nodiscard]] constexpr MonetaryAmount abs() const noexcept {
    return {true, _amount < 0 ? -_amount : _amount, _curWithDecimals};
  }

  [[nodiscard]] constexpr MonetaryAmount operator-() const noexcept { return {true, -_amount, _curWithDecimals}; }

  /// @brief  Addition of two MonetaryAmounts.
  ///         They should have same currency for addition to be possible.
  ///         Exception: default MonetaryAmount (0 with neutral currency) is a neutral element for addition and
  ///         subtraction
  [[nodiscard]] MonetaryAmount operator+(MonetaryAmount other) const;

  [[nodiscard]] MonetaryAmount operator-(MonetaryAmount other) const { return *this + (-other); }

  MonetaryAmount &operator+=(MonetaryAmount other) { return *this = *this + other; }
  MonetaryAmount &operator-=(MonetaryAmount other) { return *this = *this + (-other); }

  [[nodiscard]] MonetaryAmount operator*(AmountType mult) const;

  [[nodiscard]] friend MonetaryAmount operator*(AmountType mult, MonetaryAmount rhs) { return rhs * mult; }

  /// Multiplication involving 2 MonetaryAmounts *must* have at least one 'Neutral' currency.
  /// This is to remove ambiguity on the resulting currency:
  ///  - Neutral * Neutral -> Neutral
  ///  - XXXXXXX * Neutral -> XXXXXXX
  ///  - Neutral * YYYYYYY -> YYYYYYY
  ///  - XXXXXXX * YYYYYYY -> ??????? (exception will be thrown in this case)
  [[nodiscard]] MonetaryAmount operator*(MonetaryAmount mult) const;

  MonetaryAmount &operator*=(AmountType mult) { return *this = *this * mult; }
  MonetaryAmount &operator*=(MonetaryAmount mult) { return *this = *this * mult; }

  [[nodiscard]] MonetaryAmount operator/(AmountType div) const { return *this / MonetaryAmount(div); }

  [[nodiscard]] MonetaryAmount operator/(MonetaryAmount div) const;

  MonetaryAmount &operator/=(AmountType div) { return *this = *this / div; }
  MonetaryAmount &operator/=(MonetaryAmount div) { return *this = *this / div; }

  [[nodiscard]] constexpr MonetaryAmount toNeutral() const noexcept {
    return {true, _amount, _curWithDecimals.toNeutral()};
  }

  [[nodiscard]] constexpr bool isDefault() const noexcept { return _amount == 0 && hasNeutralCurrency(); }

  [[nodiscard]] constexpr bool hasNeutralCurrency() const noexcept { return _curWithDecimals.isNeutral(); }

  [[nodiscard]] constexpr bool isAmountInteger() const noexcept { return nbDecimals() == 0; }

  /// Truncate the MonetaryAmount such that it will contain at most maxNbDecimals.
  /// Does nothing if maxNbDecimals is larger than current number of decimals
  constexpr void truncate(int8_t maxNbDecimals) noexcept { sanitizeDecimals(nbDecimals(), maxNbDecimals); }

  /// Get a string on the currency of this amount
  [[nodiscard]] string currencyStr() const { return _curWithDecimals.str(); }

  /// @brief Appends a string representation of the amount to given output iterator
  /// @param it output iterator should have at least a capacity of
  ///           std::numeric_limits<AmountType>::digits10 + 3
  ///             (+1 for the sign, +1 for the '.', +1 for the first 0 if nbDecimals >= nbDigits)
  template <class OutputIt>
  OutputIt appendAmount(OutputIt it) const {
    if (_amount < 0) {
      *it = '-';
      ++it;
    }
    const auto nbDigits = ndigits(_amount);
    const auto nbDecs = nbDecimals();
    int remNbZerosToPrint = std::max(0, nbDecs + 1 - nbDigits);

    // no terminating null char, +1 is for the biggest decimal exponent part that is not fully covered by 64 bits
    // for instance, 9223372036854775808 is 19 chars (std::numeric_limits<AmountType>::digits10 + 1)
    char amountBuf[std::numeric_limits<AmountType>::digits10 + 1];
    std::to_chars(std::begin(amountBuf), std::end(amountBuf), std::abs(_amount));

    int amountCharPos;
    if (remNbZerosToPrint > 0) {
      amountCharPos = 0;
      *it = '0';
      ++it;
      --remNbZerosToPrint;
    } else {
      amountCharPos = nbDigits - nbDecs;
      it = std::copy(std::begin(amountBuf), std::begin(amountBuf) + amountCharPos, it);
    }

    if (nbDecs > 0) {
      *it = '.';
      ++it;
    }
    it = std::fill_n(it, remNbZerosToPrint, '0');
    return std::copy_n(std::begin(amountBuf) + amountCharPos, nbDigits - amountCharPos, it);
  }

  /// @brief Appends a string representation of the amount plus its currency to given output iterator
  /// @param it output iterator should have at least a capacity of
  ///           kMaxNbCharsAmount for the amount (explanation above)
  ///            + CurrencyCodeBase::kMaxLen + 1 for the currency and the space separator
  template <class OutputIt>
  OutputIt append(OutputIt it) const {
    it = appendAmount(it);
    if (!_curWithDecimals.isNeutral()) {
      *it = ' ';
      it = _curWithDecimals.append(++it);
    }
    return it;
  }

  /// Get a string representation of the amount hold by this MonetaryAmount (without currency).
  [[nodiscard]] string amountStr() const {
    string ret(kMaxNbCharsAmount, '\0');
    ret.erase(appendAmount(ret.begin()), ret.end());
    return ret;
  }

  void appendAmountStr(string &str) const {
    str.append(kMaxNbCharsAmount, '\0');
    auto endIt = str.end();
    str.erase(appendAmount(endIt - kMaxNbCharsAmount), endIt);
  }

  /// Get a string of this MonetaryAmount
  [[nodiscard]] string str() const {
    string ret = amountStr();
    appendCurrencyStr(ret);
    return ret;
  }

  void appendStrTo(string &str) const {
    appendAmountStr(str);
    appendCurrencyStr(str);
  }

  [[nodiscard]] uint64_t code() const noexcept {
    return HashCombine(static_cast<std::size_t>(_amount), static_cast<std::size_t>(_curWithDecimals.code()));
  }

  friend std::ostream &operator<<(std::ostream &os, const MonetaryAmount &ma);

 private:
  using UnsignedAmountType = uint64_t;

  static constexpr AmountType kMaxAmountFullNDigits = ipow10(std::numeric_limits<AmountType>::digits10);
  static constexpr std::size_t kMaxNbCharsAmount = std::numeric_limits<AmountType>::digits10 + 3;

  void appendCurrencyStr(string &str) const {
    if (!_curWithDecimals.isNeutral()) {
      _curWithDecimals.appendStrWithSpaceTo(str);
    }
  }

  /// Private constructor to set fields directly without checks.
  /// We add a dummy bool parameter to differentiate it from the public constructor
  constexpr MonetaryAmount(bool, AmountType amount, CurrencyCode curWithDecimals) noexcept
      : _amount(amount), _curWithDecimals(curWithDecimals) {}

  constexpr void sanitizeDecimals(int8_t nowNbDecimals, int8_t maxNbDecimals) noexcept {
    const int8_t nbDecimalsToTruncate = nowNbDecimals - maxNbDecimals;
    if (nbDecimalsToTruncate > 0) {
      _amount /= ipow10(static_cast<uint8_t>(nbDecimalsToTruncate));
      nowNbDecimals -= nbDecimalsToTruncate;
    }
    simplifyDecimals(nowNbDecimals);
  }

  constexpr void simplifyDecimals(int8_t nbDecs) noexcept {
    if (_amount == 0) {
      nbDecs = 0;
    } else {
      for (; nbDecs > 0 && _amount % 10 == 0; --nbDecs) {
        _amount /= 10;
      }
    }
    setNbDecimals(nbDecs);
  }

  constexpr void sanitizeIntegralPart() {
    if (_amount >= kMaxAmountFullNDigits) {
      if (!std::is_constant_evaluated()) {
        log::warn("Truncating last digit of integral part {} which is too big", _amount);
      }
      _amount /= 10;
    }
  }

  constexpr void setNbDecimals(int8_t nbDecs) { _curWithDecimals.setNbDecimals(nbDecs); }

  AmountType _amount;
  CurrencyCode _curWithDecimals;
};

static_assert(sizeof(MonetaryAmount) <= 16, "MonetaryAmount size should stay small");
static_assert(std::is_trivially_copyable_v<MonetaryAmount>, "MonetaryAmount should be trivially copyable");

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::MonetaryAmount> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::MonetaryAmount &ma, FormatContext &ctx) const -> decltype(ctx.out()) {
    return ma.append(ctx.out());
  }
};
#endif

namespace std {
template <>
struct hash<cct::MonetaryAmount> {
  auto operator()(const cct::MonetaryAmount &monetaryAmount) const { return monetaryAmount.code(); }
};
}  // namespace std
