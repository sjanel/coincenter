#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {
class StringOptionParser {
 public:
  enum class AmountType : int8_t { kAbsolute, kPercentage, kNotPresent };
  enum class FieldIs : int8_t { kMandatory, kOptional };

  StringOptionParser() noexcept = default;

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  /// If FieldIs is kOptional and there is no currency, default currency code will be returned.
  /// otherwise exception invalid_argument will be raised
  CurrencyCode parseCurrency(FieldIs fieldIs = FieldIs::kMandatory);

  /// If FieldIs is kOptional and there is no market, default market will be returned.
  /// otherwise exception invalid_argument will be raised
  Market parseMarket(FieldIs fieldIs = FieldIs::kMandatory);

  /// If FieldIs is kOptional and there is no amount, AmountType kNotPresent will be returned
  /// otherwise exception invalid_argument will be raised
  std::pair<MonetaryAmount, AmountType> parseNonZeroAmount(FieldIs fieldIs = FieldIs::kMandatory);

  /// Parse the remaining option string with CSV string values.
  vector<string> getCSVValues();

  /// Parse exchanges.
  /// Exception will be raised for any invalid exchange name - but an empty list of exchanges is accepted.
  ExchangeNames parseExchanges(char sep = ',');

  void checkEndParsing() const;

 private:
  std::string_view _opt;
  std::size_t _pos{};
};
}  // namespace cct