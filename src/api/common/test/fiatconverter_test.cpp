#include "fiatconverter.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string_view>

#include "../src/fiats-converter-responses-schema.hpp"
#include "besturlpicker.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "permanentcurloptions.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"
#include "write-json.hpp"

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

class DummyThirdPartyReader : public Reader {
  [[nodiscard]] string readAll() const override {
    return R"(
{
    "freecurrencyconverter": "blabla",
    "exchangeratesapi": "blabla"
}
)";
  }
};
}  // namespace

CurlHandle::CurlHandle([[maybe_unused]] BestURLPicker bestURLPicker,
                       [[maybe_unused]] AbstractMetricGateway *pMetricGateway,
                       [[maybe_unused]] const PermanentCurlOptions &permanentCurlOptions,
                       [[maybe_unused]] settings::RunMode runMode)
    : _bestURLPicker(kSomeFakeURL) {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::string_view CurlHandle::query([[maybe_unused]] std::string_view endpoint, const CurlOptions &opts) {
  // Rates
  std::string_view marketStr = opts.postData().get("q");
  if (!marketStr.empty()) {
    // First source

    std::string_view fromCurrency = marketStr.substr(0, 3);
    std::string_view targetCurrency = marketStr.substr(4);
    schema::FreeCurrencyConverterResponse response;

    auto &res = response.results[string(marketStr)];

    res.to = string(targetCurrency);
    res.fr = string(fromCurrency);

    if (fromCurrency == "EUR") {
      if (targetCurrency == "KRW") {
        res.val = kKRW;
      } else if (targetCurrency == "USD") {
        res.val = kUSD;
      } else if (targetCurrency == "GBP") {
        res.val = kGBP;
      }
    } else if (fromCurrency == "KRW") {
      if (targetCurrency == "EUR") {
        res.val = 1 / kKRW;
      } else if (targetCurrency == "USD") {
        res.val = kUSD / kKRW;
      } else if (targetCurrency == "GBP") {
        res.val = kGBP / kKRW;
      }
    } else if (fromCurrency == "GBP") {
      if (targetCurrency == "USD") {
        res.val = kUSD / kGBP;
      }
    }
    if (res.val != 0) {
      _queryData = WriteMiniJsonOrThrow(response);
    }

  } else {
    // second source
    schema::FiatRatesSource2Response response;
    response.base = "EUR";
    response.rates["SUSHI"] = 36.78;
    response.rates["KRW"] = 1341.88;
    response.rates["NOK"] = 11.3375;
    _queryData = WriteMiniJsonOrThrow(response);
  }

  return _queryData;
}

CurlHandle::~CurlHandle() = default;  // NOLINT

class FiatConverterTest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode};
  FiatConverter converter{coincenterInfo, milliseconds(1), Reader(), DummyThirdPartyReader()};
};

TEST_F(FiatConverterTest, DirectConversion) {
  constexpr double amount = 10;

  AreDoubleEqual(converter.convert(amount, "KRW", "KRW").value_or(0), amount);
  AreDoubleEqual(converter.convert(amount, "EUR", "KRW").value_or(0), amount * kKRW);
  AreDoubleEqual(converter.convert(amount, "EUR", "USD").value_or(0), amount * kUSD);
  AreDoubleEqual(converter.convert(amount, "EUR", "GBP").value_or(0), amount * kGBP);

  EXPECT_EQ(converter.convert(amount, "EUR", "SUSHI"), 367.8);
}

TEST_F(FiatConverterTest, DoubleConversion) {
  constexpr double amount = 20'000'000;

  AreDoubleEqual(converter.convert(amount, "KRW", "EUR").value_or(0), amount / kKRW);
  AreDoubleEqual(converter.convert(amount, "KRW", "USD").value_or(0), (amount / kKRW) * kUSD);
  AreDoubleEqual(converter.convert(amount, "GBP", "USD").value_or(0), (amount / kGBP) * kUSD);

  EXPECT_EQ(converter.convert(amount, "SUSHI", "KRW"), 729679173.46383893);
}

TEST_F(FiatConverterTest, NoConversionPossible) {
  constexpr double amount = 10;
  EXPECT_EQ(converter.convert(amount, "SUSHI", "USD"), std::nullopt);
}

}  // namespace cct
