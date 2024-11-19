#pragma once

#include <unordered_map>

#include "cct_string.hpp"
#include "currencycode.hpp"

namespace cct::schema {

struct FreeCurrencyConverterResponse {
  struct Quote {
    string id;
    double val;
    string to;
    string fr;
  };

  std::unordered_map<string, Quote> results;
};

struct FiatRatesSource2Response {
  string date;
  CurrencyCode base;
  std::unordered_map<CurrencyCode, double> rates;
};

}  // namespace cct::schema