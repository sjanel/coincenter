#pragma once

#include <string>
#include <string_view>

#include "currencycode.hpp"

namespace cct {

/// Represents a currency for a given exchange.
class CurrencyExchange {
 public:
  enum class Deposit { kAvailable, kUnavailable };  // use scoped enums to ensure type checks and increase readability
  enum class Withdraw { kAvailable, kUnavailable };

  /// Constructs a CurrencyExchange with up to two alternative codes.
  CurrencyExchange(CurrencyCode standardCode, CurrencyCode exchangeCode, CurrencyCode altCode, Deposit deposit,
                   Withdraw withdraw);

  std::string_view standardStr() const { return _standardCode.str(); }
  std::string_view exchangeStr() const { return _exchangeCode.str(); }
  std::string_view altStr() const { return _altCode.str(); }

  std::string str() const;

  CurrencyCode standardCode() const { return _standardCode; }
  CurrencyCode exchangeCode() const { return _exchangeCode; }
  CurrencyCode altCode() const { return _altCode; }

  bool canDeposit() const { return _canDeposit; }
  bool canWithdraw() const { return _canWithdraw; }

  bool operator<(const CurrencyExchange &o) const { return _standardCode < o._standardCode; }
  bool operator==(const CurrencyExchange &o) const { return _standardCode == o._standardCode; }
  bool operator!=(const CurrencyExchange &o) const { return !(*this == o); }

 private:
  CurrencyCode _standardCode;
  CurrencyCode _exchangeCode;
  CurrencyCode _altCode;
  bool _canDeposit;
  bool _canWithdraw;
};

}  // namespace cct