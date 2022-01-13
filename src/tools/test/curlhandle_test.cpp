#include "curlhandle.hpp"

#include <gtest/gtest.h>

#include "cct_proxy.hpp"

/* URL available to test HTTPS, cf
 * https://support.nmi.com/hc/en-gb/articles/360021544791-How-to-Check-If-the-Correct-Certificates-Are-Installed-on-Linux
 */

namespace cct {
namespace {
constexpr std::string_view kTestUrl = "https://live.cardeasexml.com/ultradns.php";
const CurlOptions kVerboseHttpGetOptions(HttpRequestType::kGet, "CurlHandle C++ Test", CurlOptions::Verbose::kOn);
}  // namespace

class CurlSetup : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  CurlHandle handle;
};

TEST_F(CurlSetup, BasicCurlTest) { EXPECT_EQ(handle.query(kTestUrl, kVerboseHttpGetOptions), "POOL_UP"); }

TEST_F(CurlSetup, QueryKrakenTime) {
  CurlOptions opts = kVerboseHttpGetOptions;
  opts.appendHttpHeader("MyHeaderIsVeryLongToAvoidSSO", "Val1");

  string s = handle.query("https://api.kraken.com/0/public/Time", opts);
  EXPECT_TRUE(s.find("unixtime") != string::npos);
}

TEST_F(CurlSetup, QueryKrakenSystemStatus) {
  string s = handle.query("https://api.kraken.com/0/public/SystemStatus", kVerboseHttpGetOptions);
  EXPECT_TRUE(s.find("online") != string::npos || s.find("maintenance") != string::npos ||
              s.find("cancel_only") != string::npos || s.find("post_only") != string::npos);
}

TEST_F(CurlSetup, ProxyMockTest) {
  if (IsProxyAvailable()) {
    CurlOptions opts = kVerboseHttpGetOptions;
    opts.setProxyUrl(GetProxyURL());
    string out = handle.query(kTestUrl, opts);
    EXPECT_EQ(out, "POOL_LEFT");
  }
}
}  // namespace cct