#include "curlhandle.hpp"

#include <gtest/gtest.h>

#include "cct_proxy.hpp"
#include "curloptions.hpp"

/* URL available to test HTTPS, cf
 * https://support.nmi.com/hc/en-gb/articles/360021544791-How-to-Check-If-the-Correct-Certificates-Are-Installed-on-Linux
 */

namespace cct {
namespace {
const CurlOptions kVerboseHttpGetOptions(HttpRequestType::kGet, "CurlHandle C++ Test", CurlOptions::Verbose::kOn);
}  // namespace

class KrakenBaseCurlHandle : public ::testing::Test {
 protected:
  static constexpr std::string_view kKrakenUrlBase = "https://api.kraken.com/0";

  KrakenBaseCurlHandle() : handle(kKrakenUrlBase) {}

  void SetUp() override {}
  void TearDown() override {}

  CurlHandle handle;
};

TEST_F(KrakenBaseCurlHandle, QueryKrakenTime) {
  CurlOptions opts = kVerboseHttpGetOptions;
  opts.appendHttpHeader("MyHeaderIsVeryLongToAvoidSSO", "Val1");

  string s = handle.query("/public/Time", opts);
  EXPECT_TRUE(s.find("unixtime") != string::npos);
}

TEST_F(KrakenBaseCurlHandle, QueryKrakenSystemStatus) {
  string s = handle.query("/public/SystemStatus", kVerboseHttpGetOptions);
  EXPECT_TRUE(s.find("online") != string::npos || s.find("maintenance") != string::npos ||
              s.find("cancel_only") != string::npos || s.find("post_only") != string::npos);
}

class TestCurlHandle : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://live.cardeasexml.com/ultradns.php";

  TestCurlHandle() : handle(kTestUrl) {}

  void SetUp() override {}
  void TearDown() override {}

  CurlHandle handle;
};

TEST_F(TestCurlHandle, BasicCurlTest) { EXPECT_EQ(handle.query("", kVerboseHttpGetOptions), "POOL_UP"); }

TEST_F(TestCurlHandle, ProxyMockTest) {
  if (IsProxyAvailable()) {
    CurlOptions opts = kVerboseHttpGetOptions;
    opts.setProxyUrl(GetProxyURL());
    string out = handle.query("", opts);
    EXPECT_EQ(out, "POOL_LEFT");
  }
}
}  // namespace cct