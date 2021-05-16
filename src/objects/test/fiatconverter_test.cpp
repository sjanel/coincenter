
#include "fiatconverter.hpp"

#include <gtest/gtest.h>

#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "jsonhelpers.hpp"

namespace cct {

namespace {
bool AreDoubleEqual(double lhs, double rhs) {
  constexpr double kEpsilon = 0.000001;
  if (lhs < rhs) {
    return (rhs - lhs) < kEpsilon;
  }
  return (lhs - rhs) < kEpsilon;
}

constexpr double kKRW = 1341.88;
constexpr double kUSD = 1.21;
constexpr double kGBP = 0.88;
}  // namespace

CurlHandle::CurlHandle(Clock::duration d, settings::RunMode) : _handle(nullptr), _minDurationBetweenQueries(d) {}

std::string CurlHandle::query(std::string_view url, const CurlOptions &) {
  json j;
  if (url.find("currencies") != std::string_view::npos) {
    // Currencies
    j["results"] = {"EUR", "USD", "GBP", "KRW"};
  } else {
    // Rates
    std::string_view marketStr(url.begin() + url.find("q=") + 2, url.begin() + url.find("q=") + 9);
    std::string_view fromCurrency = marketStr.substr(0, 3);
    std::string_view targetCurrency = marketStr.substr(4);
    double rate = 0;
    if (fromCurrency == "EUR") {
      if (targetCurrency == "KRW") {
        rate = kKRW;
      } else if (targetCurrency == "USD") {
        rate = kUSD;
      } else if (targetCurrency == "GBP") {
        rate = kGBP;
      }
    } else if (fromCurrency == "KRW") {
      if (targetCurrency == "EUR") {
        rate = 1 / kKRW;
      } else if (targetCurrency == "USD") {
        rate = kUSD / kKRW;
      } else if (targetCurrency == "GBP") {
        rate = kGBP / kKRW;
      }
    } else if (fromCurrency == "GBP") {
      if (targetCurrency == "USD") {
        rate = kUSD / kGBP;
      }
    }

    if (rate != 0) {
      j["results"][std::string(marketStr)]["val"] = rate;
    }
  }
  return j.dump();
}

CurlHandle::~CurlHandle() {}

TEST(FiatConverterTest, DirectConversion) {
  FiatConverter converter(std::chrono::hours(2), false);
  double amount = 10;
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "KRW", "KRW"), amount));
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "EUR", "KRW"), amount * kKRW));
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "EUR", "USD"), amount * kUSD));
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "EUR", "GBP"), amount * kGBP));
  EXPECT_ANY_THROW(converter.convert(amount, "EUR", "SUSHI"));
}

TEST(FiatConverterTest, DoubleConversion) {
  FiatConverter converter(std::chrono::hours(2), false);
  double amount = 20'000'000;
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "KRW", "EUR"), amount / kKRW));
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "KRW", "USD"), (amount / kKRW) * kUSD));
  EXPECT_TRUE(AreDoubleEqual(converter.convert(amount, "GBP", "USD"), (amount / kGBP) * kUSD));
  EXPECT_ANY_THROW(converter.convert(amount, "SUSHI", "EUR"));
}

}  // namespace cct
