#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string_view>
#include <type_traits>

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
  constexpr MonetaryAmount(MonetaryAmount o, CurrencyCode newCurrencyCode)
      : _amount(o._amount), _curWithDecimals(newCurrencyCode) {
    setNbDecimals(o.nbDecimals());
  }

  /// Get an integral representation of this MonetaryAmount multiplied by given number of decimals.
  /// If an overflow would occur for the resulting amount, return std::nullopt
  /// Example : "5.6235" with 6 decimals will return 5623500
  std::optional<AmountType> amount(int8_t nbDecimals) const;

  /// Get the integer part of the amount of this MonetaryAmount.
  constexpr AmountType integerPart() const { return _amount / ipow(10, static_cast<uint8_t>(nbDecimals())); }

  /// Get the decimal part of the amount of this MonetaryAmount.
  /// Warning: starting zeros will not be part of the returned value. Use nbDecimals to retrieve the number of decimals
  /// of this MonetaryAmount.
  /// Example: "45.046" decimalPart() = 46
  constexpr AmountType decimalPart() const {
    return _amount - integerPart() * ipow(10, static_cast<uint8_t>(nbDecimals()));
  }

  /// Get the amount of this MonetaryAmount in double format.
  constexpr double toDouble() const {
    return static_cast<double>(_amount) / ipow(10, static_cast<uint8_t>(nbDecimals()));
  }

  constexpr CurrencyCode currencyCode() const {
    // We do not want to expose private nb decimals bits to outside world
    return _curWithDecimals.withNoDecimalsPart();
  }

  constexpr int8_t nbDecimals() const { return _curWithDecimals.nbDecimals(); }

  constexpr int8_t maxNbDecimals() const {
    return _curWithDecimals.isLongCurrencyCode()
               ? CurrencyCodeConstants::kMaxNbDecimalsLongCurrencyCode
               : std::numeric_limits<AmountType>::digits10 - 1;  // -1 as minimal nb digits of integral part
  }

  /// Returns the maximum number of decimals that this amount could hold, given its integral part.
  /// Examples:
  ///  0.00426622338114037 EUR -> 17
  ///  45.546675 EUR           -> 16
  constexpr int8_t currentMaxNbDecimals() const {
    return static_cast<int8_t>(maxNbDecimals() - ndigits(integerPart()) + 1);
  }

  /// Converts current amount at given price.
  /// Example: ETH/EUR
  ///            2 ETH convertTo("1600 EUR")       = 3200 EUR
  ///            1500 EUR convertTo("0.0005 ETH")  = 0.75 ETH
  /// @return a monetary amount in the currency of given price
  MonetaryAmount convertTo(MonetaryAmount p) const { return p * toNeutral(); }

  /// Rounds current monetary amount according to given step amount.
  /// CurrencyCode of 'step' is unused.
  /// Example: 123.45 with 0.1 as step will return 123.4.
  /// Assumption: 'step' should be strictly positive amount
  void round(MonetaryAmount step, RoundType roundType);

  /// Rounds current monetary amount according to given precision (number of decimals)
  void round(int8_t nbDecimals, RoundType roundType);

  std::strong_ordering operator<=>(const MonetaryAmount &o) const;

  constexpr bool operator==(const MonetaryAmount &o) const = default;

  /// True if amount is 0
  constexpr bool isZero() const noexcept { return _amount == 0; }

  constexpr bool isPositiveOrZero() const noexcept { return _amount >= 0; }
  constexpr bool isNegativeOrZero() const noexcept { return _amount <= 0; }
  constexpr bool isStrictlyPositive() const noexcept { return _amount > 0; }
  constexpr bool isStrictlyNegative() const noexcept { return _amount < 0; }

  constexpr MonetaryAmount abs() const noexcept {
    return MonetaryAmount(true, _amount < 0 ? -_amount : _amount, _curWithDecimals);
  }

  constexpr MonetaryAmount operator-() const noexcept { return MonetaryAmount(true, -_amount, _curWithDecimals); }

  MonetaryAmount operator+(MonetaryAmount o) const;

  MonetaryAmount operator-(MonetaryAmount o) const { return *this + (-o); }

  MonetaryAmount &operator+=(MonetaryAmount o) {
    *this = *this + o;
    return *this;
  }
  MonetaryAmount &operator-=(MonetaryAmount o) {
    *this = *this - o;
    return *this;
  }

  MonetaryAmount operator*(AmountType mult) const;

  /// Multiplication involving 2 MonetaryAmounts *must* have at least one 'Neutral' currency.
  /// This is to remove ambiguity on the resulting currency:
  ///  - Neutral * Neutral -> Neutral
  ///  - XXXXXXX * Neutral -> XXXXXXX
  ///  - Neutral * YYYYYYY -> YYYYYYY
  ///  - XXXXXXX * YYYYYYY -> ??????? (exception will be thrown in this case)
  MonetaryAmount operator*(MonetaryAmount mult) const;

  MonetaryAmount &operator*=(AmountType mult) {
    *this = *this * mult;
    return *this;
  }
  MonetaryAmount &operator*=(MonetaryAmount mult) {
    *this = *this * mult;
    return *this;
  }

  MonetaryAmount operator/(AmountType div) const { return *this / MonetaryAmount(div); }

  MonetaryAmount operator/(MonetaryAmount div) const;

  MonetaryAmount &operator/=(AmountType div) {
    *this = *this / div;
    return *this;
  }
  MonetaryAmount &operator/=(MonetaryAmount div) {
    *this = *this / div;
    return *this;
  }

  constexpr MonetaryAmount toNeutral() const noexcept {
    return MonetaryAmount(true, _amount, _curWithDecimals.toNeutral());
  }

  constexpr bool isDefault() const noexcept { return _amount == 0 && hasNeutralCurrency(); }

  constexpr bool hasNeutralCurrency() const noexcept { return _curWithDecimals.isNeutral(); }

  constexpr bool isAmountInteger() const noexcept { return nbDecimals() == 0; }

  /// Truncate the MonetaryAmount such that it will contain at most maxNbDecimals.
  /// Does nothing if maxNbDecimals is larger than current number of decimals
  constexpr void truncate(int8_t maxNbDecimals) noexcept { sanitizeDecimals(nbDecimals(), maxNbDecimals); }

  /// Get a string on the currency of this amount
  string currencyStr() const { return _curWithDecimals.str(); }

  /// Get a string representation of the amount hold by this MonetaryAmount (without currency).
  string amountStr() const;

  string str() const;

  friend std::ostream &operator<<(std::ostream &os, const MonetaryAmount &m);

 private:
  using UnsignedAmountType = uint64_t;

  static constexpr AmountType kMaxAmountFullNDigits = ipow(10, std::numeric_limits<AmountType>::digits10);

  /// Private constructor to set fields directly without checks.
  /// We add a dummy bool parameter to differentiate it from the public constructor
  constexpr MonetaryAmount(bool, AmountType amount, CurrencyCode curWithDecimals) noexcept
      : _amount(amount), _curWithDecimals(curWithDecimals) {}

  constexpr void sanitizeDecimals(int8_t nowNbDecimals, int8_t maxNbDecimals) noexcept {
    const int8_t nbDecimalsToTruncate = nowNbDecimals - maxNbDecimals;
    if (nbDecimalsToTruncate > 0) {
      _amount /= ipow(10, static_cast<uint8_t>(nbDecimalsToTruncate));
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

  constexpr inline void setNbDecimals(int8_t nbDecs) { _curWithDecimals.setNbDecimals(nbDecs); }

  AmountType _amount;
  CurrencyCode _curWithDecimals;
};

static_assert(sizeof(MonetaryAmount) <= 16, "MonetaryAmount size should stay small");
static_assert(std::is_trivially_copyable_v<MonetaryAmount>, "MonetaryAmount should be trivially copyable");

inline MonetaryAmount operator*(MonetaryAmount::AmountType mult, MonetaryAmount rhs) { return rhs * mult; }

}  // namespace cct
