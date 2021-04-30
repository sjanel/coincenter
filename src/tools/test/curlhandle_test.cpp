#include "curlhandle.hpp"

#include <curl/curl.h>
#include <gtest/gtest.h>

#include "cct_proxy.hpp"

// #define DEBUG
/* URL available to test HTTPS, cf
 * https://support.nmi.com/hc/en-gb/articles/360021544791-How-to-Check-If-the-Correct-Certificates-Are-Installed-on-Linux
 */

static const char *const kTestUrl = "https://live.cardeasexml.com/ultradns.php";

namespace cct {
class CurlSetup : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  CurlInitRAII _raiiCurl;
};

TEST_F(CurlSetup, BasicCurlTest) {
  CurlHandle handle;
  std::string out = handle.query(kTestUrl, CurlOptions(CurlOptions::RequestType::kGet));
  EXPECT_EQ(out, "POOL_UP");
}

TEST_F(CurlSetup, Queries) {
  CurlHandle curl;
  {
    std::string header = "MyHeaderIsVeryLongToAvoidSSO";
    CurlOptions opts(CurlOptions::RequestType::kGet);
    opts.httpHeaders.push_back(header);
    opts.postdata.append("opts", "dummy");

    std::string s = curl.query("https://api.kraken.com/0/public/Time", opts);
    EXPECT_TRUE(s.find("unixtime") != std::string::npos);
  }
  {
    CurlOptions opts(CurlOptions::RequestType::kGet);
    opts.verbose = true;
    std::string s = curl.query("https://api.kraken.com/0/public/SystemStatus", opts);
    EXPECT_TRUE(s.find("online") != std::string::npos);
  }
}

TEST_F(CurlSetup, ProxyMockTest) {
  if (IsProxyAvailable()) {
    CurlHandle handle;
    CurlOptions opts(CurlOptions::RequestType::kGet);
    opts.proxy._url = GetProxyURL();
#ifdef DEBUG
    opts.verbose = true;
#endif
    std::string out = handle.query(kTestUrl, opts);
    EXPECT_EQ(out, "POOL_LEFT");
  }
}
}  // namespace cct