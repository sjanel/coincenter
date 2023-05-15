#include "curlhandle.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"
#include "curloptions.hpp"
#include "proxy.hpp"

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

  CurlHandle handle{kKrakenUrlBase};
};

TEST_F(KrakenBaseCurlHandle, QueryKrakenTime) {
  CurlOptions opts = kVerboseHttpGetOptions;
  opts.appendHttpHeader("MyHeaderIsVeryLongToAvoidSSO", "Val1");

  EXPECT_TRUE(handle.query("/public/Time", opts).find("unixtime") != std::string_view::npos);
}

TEST_F(KrakenBaseCurlHandle, QueryKrakenSystemStatus) {
  std::string_view str = handle.query("/public/SystemStatus", kVerboseHttpGetOptions);
  EXPECT_TRUE(str.find("online") != std::string_view::npos || str.find("maintenance") != std::string_view::npos ||
              str.find("cancel_only") != std::string_view::npos || str.find("post_only") != std::string_view::npos);
}

class TestCurlHandle : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://live.cardeasexml.com/ultradns.php";

  CurlHandle handle{kTestUrl};
};

TEST_F(TestCurlHandle, BasicCurlTest) {
  EXPECT_EQ(handle.query("", kVerboseHttpGetOptions), "POOL_UP");
  EXPECT_EQ(handle.queryRelease("", kVerboseHttpGetOptions), "POOL_UP");
}

TEST_F(TestCurlHandle, ProxyMockTest) {
  if (IsProxyAvailable()) {
    CurlOptions opts = kVerboseHttpGetOptions;
    opts.setProxyUrl(GetProxyURL());
    EXPECT_EQ(handle.query("", opts), "POOL_LEFT");
  }
}

TEST_F(TestCurlHandle, CurlVersion) { EXPECT_NE(GetCurlVersionInfo(), string()); }

class TestOverrideQueryResponses : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://this-url-does-not-exist-12345";

  CurlOptions emptyOpts{HttpRequestType::kGet};
  CurlOptions param1OptsGet{HttpRequestType::kGet, CurlPostData{{"param1", "v"}}};
  CurlOptions param1OptsPost{HttpRequestType::kPost, CurlPostData{{"param1", "v"}}};

  AbstractMetricGateway *pAbstractMetricGateway = nullptr;
  Duration minDurationBetweenQueries = Duration::zero();
  settings::RunMode runMode = settings::RunMode::kQueryResponseOverriden;
  CurlHandle handle{kTestUrl, pAbstractMetricGateway, minDurationBetweenQueries, runMode};
};

TEST_F(TestOverrideQueryResponses, NoQueryResponses) { EXPECT_THROW(handle.query("/endpoint", emptyOpts), exception); }

TEST_F(TestOverrideQueryResponses, WithQueryResponses) {
  handle.setOverridenQueryResponses({{"/path1", "{}"}, {"/path3?param1=v", "42"}});

  for (int testPos = 0; testPos < 2; ++testPos) {
    EXPECT_EQ(handle.query("/path1", emptyOpts), "{}");
    EXPECT_THROW(handle.query("/path2", emptyOpts), exception);
    EXPECT_EQ(handle.query("/path3", param1OptsGet), "42");
    EXPECT_EQ(handle.query("/path3", param1OptsPost), "42");
  }

  handle.setOverridenQueryResponses({});
  EXPECT_THROW(handle.query("/path1", emptyOpts), exception);
}
}  // namespace cct