#include "curlhandle.hpp"

#include <gtest/gtest.h>

#include "cct_proxy.hpp"

// #define DEBUG
/* URL available to test HTTPS, cf
 * https://support.nmi.com/hc/en-gb/articles/360021544791-How-to-Check-If-the-Correct-Certificates-Are-Installed-on-Linux
 */

namespace cct {
namespace {
constexpr char kTestUrl[] = "https://live.cardeasexml.com/ultradns.php";
}

class CurlSetup : public ::testing::Test {
 protected:
  CurlSetup() : handle(nullptr, Clock::duration::zero(), settings::RunMode::kProd) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CurlHandle handle;
};

TEST_F(CurlSetup, BasicCurlTest) { EXPECT_EQ(handle.query(kTestUrl, CurlOptions(HttpRequestType::kGet)), "POOL_UP"); }

TEST_F(CurlSetup, QueryKrakenTime) {
  CurlOptions opts(HttpRequestType::kGet);
#ifdef DEBUG
  opts.verbose = true;
#endif
  opts.httpHeaders.push_back("MyHeaderIsVeryLongToAvoidSSO");

  string s = handle.query("https://api.kraken.com/0/public/Time", opts);
  EXPECT_TRUE(s.find("unixtime") != string::npos);
}

TEST_F(CurlSetup, QueryKrakenSystemStatus) {
  CurlOptions opts(HttpRequestType::kGet);
#ifdef DEBUG
  opts.verbose = true;
#endif
  string s = handle.query("https://api.kraken.com/0/public/SystemStatus", opts);
  EXPECT_TRUE(s.find("online") != string::npos || s.find("maintenance") != string::npos ||
              s.find("cancel_only") != string::npos || s.find("post_only") != string::npos);
}

TEST_F(CurlSetup, ProxyMockTest) {
  if (IsProxyAvailable()) {
    CurlOptions opts(HttpRequestType::kGet);
    opts.proxy._url = GetProxyURL();
#ifdef DEBUG
    opts.verbose = true;
#endif
    string out = handle.query(kTestUrl, opts);
    EXPECT_EQ(out, "POOL_LEFT");
  }
}
}  // namespace cct