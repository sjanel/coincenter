#include "curlhandle.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "httprequesttype.hpp"
#include "permanentcurloptions.hpp"
#include "proxy.hpp"
#include "runmodes.hpp"

/* URL available to test HTTPS, cf
 * https://support.nmi.com/hc/en-gb/articles/360021544791-How-to-Check-If-the-Correct-Certificates-Are-Installed-on-Linux
 */

namespace cct {
namespace {
const CurlOptions kVerboseHttpGetOptions(HttpRequestType::kGet, CurlOptions::Verbose::kOn);
}  // namespace

class ExampleBaseCurlHandle : public ::testing::Test {
 protected:
  static constexpr std::string_view kHttpBinBase = "https://httpbin.org";

  CurlHandle handle{kHttpBinBase};
};

TEST_F(ExampleBaseCurlHandle, CurlVersion) { EXPECT_FALSE(GetCurlVersionInfo().empty()); }

TEST_F(ExampleBaseCurlHandle, QueryJsonAndMoveConstruct) {
  CurlOptions opts = kVerboseHttpGetOptions;
  opts.appendHttpHeader("MyHeaderIsVeryLongToAvoidSSO", "Val1");

  EXPECT_NE(handle.query("/json", opts).find("slideshow"), std::string_view::npos);

  CurlHandle newCurlHandle = std::move(handle);
  EXPECT_NE(newCurlHandle.query("/json", opts).find("slideshow"), std::string_view::npos);
}

TEST_F(ExampleBaseCurlHandle, QueryXmlAndMoveAssign) {
  EXPECT_NE(handle.query("/xml", kVerboseHttpGetOptions).find("<?xml"), std::string_view::npos);

  CurlHandle newCurlHandle;
  newCurlHandle = std::move(handle);

  EXPECT_NE(newCurlHandle.query("/xml", kVerboseHttpGetOptions).find("<?xml"), std::string_view::npos);
}

class CurlHandleProxyTest : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://live.cardeasexml.com/ultradns.php";

  CurlHandle handle{kTestUrl};
};

TEST_F(CurlHandleProxyTest, ProxyMockTest) {
  if (IsProxyAvailable()) {
    CurlOptions opts = kVerboseHttpGetOptions;
    opts.setProxyUrl(GetProxyURL());
    EXPECT_EQ(handle.query("", opts), "POOL_LEFT");
  }
}

class TestOverrideQueryResponses : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://this-url-does-not-exist-12345";

  CurlOptions emptyOpts{HttpRequestType::kGet};
  CurlOptions param1OptsGet{HttpRequestType::kGet, CurlPostData{{"param1", "v"}}};
  CurlOptions param1OptsPost{HttpRequestType::kPost, CurlPostData{{"param1", "v"}}};

  AbstractMetricGateway *pAbstractMetricGateway = nullptr;
  settings::RunMode runMode = settings::RunMode::kQueryResponseOverriden;
  CurlHandle handle{kTestUrl, pAbstractMetricGateway, PermanentCurlOptions(), runMode};
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