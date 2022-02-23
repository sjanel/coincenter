#pragma once

#include <cstdint>
#include <string_view>

#include "cct_string.hpp"
#include "currencycode.hpp"

namespace cct {

/// Represents a currency for a given exchange.
class CurrencyExchange {
 public:
  // use scoped enums to ensure type checks and increase readability
  enum class Deposit : int8_t { kAvailable, kUnavailable };
  enum class Withdraw : int8_t { kAvailable, kUnavailable };
  enum class Type : int8_t { kFiat, kCrypto };

  CurrencyExchange() noexcept = default;

  /// Constructs a CurrencyExchange with a standard code, with unknown withdrawal / deposit statuses
  CurrencyExchange(CurrencyCode standardCode)
      : _standardCode(standardCode), _exchangeCode(standardCode), _altCode(standardCode) {}

  /// Constructs a CurrencyExchange with up to two alternative codes, with unknown withdrawal / deposit statuses
  CurrencyExchange(CurrencyCode standardCode, CurrencyCode exchangeCode, CurrencyCode altCode)
      : _standardCode(standardCode), _exchangeCode(exchangeCode), _altCode(altCode) {}

  /// Constructs a CurrencyExchange with known withdrawal / deposit statuses
  CurrencyExchange(CurrencyCode standardCode, Deposit deposit, Withdraw withdraw, Type type);

  /// Constructs a CurrencyExchange with up to two alternative codes, with known withdrawal / deposit statuses
  CurrencyExchange(CurrencyCode standardCode, CurrencyCode exchangeCode, CurrencyCode altCode, Deposit deposit,
                   Withdraw withdraw, Type type);

  std::string_view standardStr() const { return _standardCode.str(); }
  std::string_view exchangeStr() const { return _exchangeCode.str(); }
  std::string_view altStr() const { return _altCode.str(); }

  string str() const;

  CurrencyCode standardCode() const { return _standardCode; }
  CurrencyCode exchangeCode() const { return _exchangeCode; }
  CurrencyCode altCode() const { return _altCode; }

  bool canDeposit() const { return _canDeposit; }
  bool canWithdraw() const { return _canWithdraw; }

  bool isFiat() const { return _isFiat; }

  auto operator<=>(const CurrencyExchange &o) const { return _standardCode <=> o._standardCode; }

  bool operator==(const CurrencyExchange &o) const { return _standardCode == o._standardCode; }
  bool operator!=(const CurrencyExchange &o) const { return !(*this == o); }

  operator CurrencyCode() const { return _standardCode; }

 private:
  CurrencyCode _standardCode;
  CurrencyCode _exchangeCode;
  CurrencyCode _altCode;
  bool _canDeposit = false;
  bool _canWithdraw = false;
  bool _isFiat = false;
};

}  // namespace cct