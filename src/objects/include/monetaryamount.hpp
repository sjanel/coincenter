#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "cct_mathhelpers.hpp"
#include "currencycode.hpp"

namespace cct {

/// Represents a decimal amount with a currency (fiat or coin). Examples: $50, -2.045 BTC.
/// The integral value stored in the MonetaryAmount is multiplied by 10^'_nbDecimals'
/// Its number of decimals is automatically adjusted and simplified.
class MonetaryAmount {
 public:
  using AmountType = int64_t;

  /// Constructs a MonetaryAmount with a value of 0 of neutral currency.
  constexpr MonetaryAmount() noexcept : _amount(0), _currencyCode(), _nbDecimals(0) {}

  /// Constructs a new MonetaryAmount from an integral representation which is already multiplied by given
  /// number of decimals
  constexpr MonetaryAmount(AmountType amount, CurrencyCode currencyCode, int8_t nbDecimals) noexcept
      : _amount(amount), _currencyCode(currencyCode), _nbDecimals(nbDecimals) {
    simplify();
  }

  /// Constructs a new MonetaryAmount from a string, containing an optional CurrencyCode.
  /// If it's not present, assume default CurrencyCode.
  /// Examples: "10.5EUR" -> 10.5 units of currency EUR
  ///           "45 KRW" -> 45 units of currency KRW
  ///           "-345.8909" -> -345.8909 units of no currency
  explicit MonetaryAmount(std::string_view amountCurrencyStr);

  /// Constructs a new MonetaryAmount from a string and a currency code.
  /// Precision is calculated automatically.
  MonetaryAmount(std::string_view amountStr, CurrencyCode currencyCode);

  /// Constructs a new MonetaryAmount from another MonetaryAmount and a new CurrencyCode.
  /// Use this constructor to change currency of an existing MonetaryAmount.
  constexpr MonetaryAmount(MonetaryAmount o, CurrencyCode newCurrencyCode)
      : _amount(o._amount), _currencyCode(newCurrencyCode), _nbDecimals(o._nbDecimals) {}

  /// Construct a new MonetaryAmount from a double.
  /// Precision is calculated automatically.
  MonetaryAmount(double amount, CurrencyCode currencyCode = CurrencyCode());

  /// Get an integral representation of this MonetaryAmount multiplied by given number of decimals.
  /// If an overflow would occur for the resulting amount, return std::nullopt
  /// Example : "5.6235" with 6 decimals will return 5623500
  std::optional<AmountType> amount(int8_t nbDecimals) const;

  /// Get the integer part of the amount of this MonetaryAmount.
  constexpr AmountType integerPart() const { return _amount / ipow(10, _nbDecimals); }

  /// Get the decimal part of the amount of this MonetaryAmount.
  /// Warning: starting zeros will not be part of the returned value. Use nbDecimals to retrieve the number of decimals
  /// of this MonetaryAmount.
  /// Example: "45.046" decimalPart() = 46
  constexpr AmountType decimalPart() const { return _amount - integerPart() * ipow(10, _nbDecimals); }

  /// Get the amount of this MonetaryAmount in double format.
  constexpr double toDouble() const { return static_cast<double>(_amount) / ipow(10, _nbDecimals); }

  constexpr CurrencyCode currencyCode() const { return _currencyCode; }

  constexpr int8_t nbDecimals() const { return _nbDecimals; }

  /// Returns the maximum number of decimals that this amount could hold, given its integral part.
  constexpr int8_t maxNbDecimals() const {
    return _nbDecimals + static_cast<int8_t>(std::numeric_limits<AmountType>::digits10 - ndigits(_amount));
  }

  /// Converts current amount at given price.
  /// Example: ETH/EUR
  ///            2 ETH convertTo("1600 EUR")       = 3200 EUR
  ///            1500 EUR convertTo("0.0005 ETH")  = 0.75 ETH
  /// @return a monetary amount in the currency of given price
  MonetaryAmount convertTo(MonetaryAmount p) const { return p * toNeutral(); }

  enum class RoundType { kDown, kUp };

  /// Rounds current monetary amount according to given step amount and return the result.
  /// Example: 123.45 with 0.1 as step will return 123.4.
  /// Assumption: 'step' should be strictly positive amount
  MonetaryAmount round(MonetaryAmount step, RoundType roundType) const;

  bool operator<(MonetaryAmount o) const;
  bool operator<=(MonetaryAmount o) const { return !(o < *this); }
  bool operator>(MonetaryAmount o) const { return o < *this; }
  bool operator>=(MonetaryAmount o) const { return !(*this < o); }

  bool operator==(MonetaryAmount o) const;
  bool operator!=(MonetaryAmount o) const { return !(*this == o); }

  /// True if amount is 0
  bool isZero() const { return _amount == 0; }

  MonetaryAmount operator-() const { return MonetaryAmount(-_amount, _currencyCode, _nbDecimals); }

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

  MonetaryAmount operator/(AmountType div) const { return *this / MonetaryAmount(div, CurrencyCode::kNeutral, 0); }

  MonetaryAmount operator/(MonetaryAmount div) const;

  MonetaryAmount &operator/=(AmountType div) {
    *this = *this / div;
    return *this;
  }
  MonetaryAmount &operator/=(MonetaryAmount div) {
    *this = *this / div;
    return *this;
  }

  constexpr MonetaryAmount toNeutral() const { return MonetaryAmount(_amount, CurrencyCode(), _nbDecimals); }

  /// Truncate the MonetaryAmount such that it will contain at most maxNbDecimals
  constexpr void truncate(int8_t maxNbDecimals) { sanitizeNbDecimals(maxNbDecimals); }

  /// Get a string representation of the amount hold by this MonetaryAmount (without currency).
  std::string amountStr() const;

  std::string str() const;

  friend std::ostream &operator<<(std::ostream &os, const MonetaryAmount &m);

 private:
  using UnsignedAmountType = uint64_t;

  constexpr void simplify() {
    if (_amount == 0) {
      _nbDecimals = 0;
    } else {
      for (; _nbDecimals > 0 && _amount % 10 == 0; --_nbDecimals) {
        _amount /= 10;
      }
    }
  }

  constexpr void sanitizeNbDecimals(int8_t maxNbDecimals = std::numeric_limits<AmountType>::digits10 - 1) {
    const int8_t nbDecimalsToTruncate = _nbDecimals - maxNbDecimals;
    if (nbDecimalsToTruncate > 0) {
      _amount /= ipow(10, static_cast<uint8_t>(nbDecimalsToTruncate));
      _nbDecimals -= nbDecimalsToTruncate;
    }
  }

  constexpr bool isSane() const {
    return _nbDecimals < std::numeric_limits<AmountType>::digits10 &&
           ndigits(_amount) <= std::numeric_limits<AmountType>::digits10;
  }

  AmountType _amount;
  CurrencyCode _currencyCode;
  int8_t _nbDecimals;
};

inline MonetaryAmount operator*(MonetaryAmount::AmountType mult, MonetaryAmount rhs) { return rhs * mult; }

}  // namespace cct
