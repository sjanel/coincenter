#include "curlhandle.hpp"

#include <curl/curl.h>
#include <gtest/gtest.h>

#include "cct_log.hpp"
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

  CurlHandle handle;
};

TEST_F(CurlSetup, BasicCurlTest) {
  std::string out = handle.query(kTestUrl, CurlOptions(CurlOptions::RequestType::kGet));
  EXPECT_EQ(out, "POOL_UP");
}

TEST_F(CurlSetup, Queries) {
  {
    std::string header = "MyHeaderIsVeryLongToAvoidSSO";
    CurlOptions opts(CurlOptions::RequestType::kGet);
#ifdef DEBUG
    opts.verbose = true;
#endif
    opts.httpHeaders.push_back(header);

    std::string s = handle.query("https://api.kraken.com/0/public/Time", opts);
    EXPECT_TRUE(s.find("unixtime") != std::string::npos);
  }
  {
    CurlOptions opts(CurlOptions::RequestType::kGet);
#ifdef DEBUG
    opts.verbose = true;
#endif
    log::set_level(log::level::trace);
    std::string s = handle.query("https://api.kraken.com/0/public/SystemStatus", opts);
    EXPECT_TRUE(s.find("online") != std::string::npos || s.find("maintenance") != std::string::npos ||
                s.find("cancel_only") != std::string::npos || s.find("post_only") != std::string::npos);
  }
}

TEST_F(CurlSetup, ProxyMockTest) {
  if (IsProxyAvailable()) {
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