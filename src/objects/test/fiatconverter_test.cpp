
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

CurlHandle::CurlHandle(const BestURLPicker &, AbstractMetricGateway *, Duration, settings::RunMode)
    : _handle(nullptr), _bestUrlPicker(kSomeFakeURL) {}

string CurlHandle::query(std::string_view url, const CurlOptions &) {
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
      j["results"][string(marketStr)]["val"] = rate;
    }
  }
  return j.dump();
}

CurlHandle::~CurlHandle() = default;

class FiatConverterTest : public ::testing::Test {
 protected:
  FiatConverterTest() : converter(coincenterInfo, std::chrono::milliseconds(1)) {}

  void SetUp() override {}
  void TearDown() override {}

  CoincenterInfo coincenterInfo;
  FiatConverter converter;
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
