#include "fiatconverter.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"

namespace cct {

namespace {
void AreDoubleEqual(double lhs, double rhs) {
  static constexpr double kEpsilon = 0.000001;
  if (lhs < rhs) {
    EXPECT_LT(rhs - lhs, kEpsilon);
  } else {
    EXPECT_LT(lhs - rhs, kEpsilon);
  }
}

constexpr double kKRW = 1341.88;
constexpr double kUSD = 1.21;
constexpr double kGBP = 0.88;

constexpr std::string_view kSomeFakeURL = "some/fake/url";
}  // namespace

CurlHandle::CurlHandle([[maybe_unused]] const BestURLPicker &bestURLPicker,
                       [[maybe_unused]] AbstractMetricGateway *pMetricGateway,
                       [[maybe_unused]] Duration minDurationBetweenQueries, [[maybe_unused]] settings::RunMode runMode)
    : _handle(nullptr), _bestUrlPicker(kSomeFakeURL) {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::string_view CurlHandle::query(std::string_view endpoint, [[maybe_unused]] const CurlOptions &opts) {
  json jsonData;
  if (endpoint.find("currencies") != std::string_view::npos) {
    // Currencies
    jsonData["results"] = {"EUR", "USD", "GBP", "KRW"};
  } else {
    // Rates
    std::string_view marketStr(endpoint.begin() + endpoint.find("q=") + 2, endpoint.begin() + endpoint.find("q=") + 9);
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
      jsonData["results"][marketStr]["val"] = rate;
    }
  }
  _queryData = jsonData.dump();
  return _queryData;
}

CurlHandle::~CurlHandle() {}  // NOLINT

class FiatConverterTest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode};
  FiatConverter converter{coincenterInfo, std::chrono::milliseconds(1)};
};

TEST_F(FiatConverterTest, DirectConversion) {
  const double amount = 10;
  AreDoubleEqual(converter.convert(amount, "KRW", "KRW"), amount);
  AreDoubleEqual(converter.convert(amount, "EUR", "KRW"), amount * kKRW);
  AreDoubleEqual(converter.convert(amount, "EUR", "USD"), amount * kUSD);
  AreDoubleEqual(converter.convert(amount, "EUR", "GBP"), amount * kGBP);
  EXPECT_THROW(converter.convert(amount, "EUR", "SUSHI"), exception);
}

TEST_F(FiatConverterTest, DoubleConversion) {
  const double amount = 20'000'000;
  AreDoubleEqual(converter.convert(amount, "KRW", "EUR"), amount / kKRW);
  AreDoubleEqual(converter.convert(amount, "KRW", "USD"), (amount / kKRW) * kUSD);
  AreDoubleEqual(converter.convert(amount, "GBP", "USD"), (amount / kGBP) * kUSD);
  EXPECT_THROW(converter.convert(amount, "SUSHI", "EUR"), exception);
}

}  // namespace cct
