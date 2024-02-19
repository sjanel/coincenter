#include "fiatconverter.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string_view>

#include "besturlpicker.hpp"
#include "cct_json.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "permanentcurloptions.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {

namespace {
void AreDoubleEqual(double lhs, double rhs) {
  static constexpr double kEpsilon = 0.00000001;
  EXPECT_LT(std::abs(rhs - lhs), kEpsilon);
}

constexpr double kKRW = 1341.88;
constexpr double kUSD = 1.21;
constexpr double kGBP = 0.88;

constexpr std::string_view kSomeFakeURL = "some/fake/url";
}  // namespace

CurlHandle::CurlHandle([[maybe_unused]] BestURLPicker bestURLPicker,
                       [[maybe_unused]] AbstractMetricGateway *pMetricGateway,
                       [[maybe_unused]] const PermanentCurlOptions &permanentCurlOptions,
                       [[maybe_unused]] settings::RunMode runMode)
    : _bestURLPicker(kSomeFakeURL) {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::string_view CurlHandle::query([[maybe_unused]] std::string_view endpoint, const CurlOptions &opts) {
  json jsonData;

  // Rates
  std::string_view marketStr = opts.postData().get("q");
  if (!marketStr.empty()) {
    double rate = 0;

    std::string_view fromCurrency = marketStr.substr(0, 3);
    std::string_view targetCurrency = marketStr.substr(4);

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
  } else {
    // second source
    jsonData = R"(
{
  "base": "EUR",
  "rates": {
    "SUSHI": 36.78,
    "KRW": 1341.88,
    "NOK": 11.3375
  }
}
)"_json;
  }

  _queryData = jsonData.dump();
  return _queryData;
}

CurlHandle::~CurlHandle() {}  // NOLINT

class FiatConverterTest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode};
  FiatConverter converter{coincenterInfo, TimeInMs(1)};
};

TEST_F(FiatConverterTest, DirectConversion) {
  constexpr double amount = 10;

  AreDoubleEqual(converter.convert(amount, "KRW", "KRW").value(), amount);
  AreDoubleEqual(converter.convert(amount, "EUR", "KRW").value(), amount * kKRW);
  AreDoubleEqual(converter.convert(amount, "EUR", "USD").value(), amount * kUSD);
  AreDoubleEqual(converter.convert(amount, "EUR", "GBP").value(), amount * kGBP);

  EXPECT_EQ(converter.convert(amount, "EUR", "SUSHI"), 367.8);
}

TEST_F(FiatConverterTest, DoubleConversion) {
  constexpr double amount = 20'000'000;

  AreDoubleEqual(converter.convert(amount, "KRW", "EUR").value(), amount / kKRW);
  AreDoubleEqual(converter.convert(amount, "KRW", "USD").value(), (amount / kKRW) * kUSD);
  AreDoubleEqual(converter.convert(amount, "GBP", "USD").value(), (amount / kGBP) * kUSD);

  EXPECT_EQ(converter.convert(amount, "SUSHI", "KRW"), 729679173.46383917);
}

TEST_F(FiatConverterTest, NoConversionPossible) {
  constexpr double amount = 10;
  EXPECT_EQ(converter.convert(amount, "SUSHI", "USD"), std::nullopt);
}

}  // namespace cct
