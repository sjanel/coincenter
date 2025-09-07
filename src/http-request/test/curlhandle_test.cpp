#include "curlhandle.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string_view>
#include <thread>
#include <utility>

#include "cct_exception.hpp"
#include "cct_string.hpp"
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

// Simple heuristic to detect a transient server-side unavailability response we want to retry on.
// We do not have HTTP status codes exposed here, so we inspect body content.
bool IsLikelyTransientServiceUnavailable(std::string_view resp) {
  if (resp.empty()) {
    return true;  // empty body is unexpected for the endpoints we target
  }
  // Lowercase copy for case-insensitive search (responses are tiny)
  string lower(resp);
  std::ranges::transform(lower, lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return lower.find("service unavailable") != string::npos || lower.find("unavailable") != string::npos;
}

// Test-side retry wrapper for transient 5xx-like conditions (observed random 503 from httpbin.org).
// Keeps production code unchanged (Step 1). Exponential backoff with small caps to avoid slowing CI.
std::string_view QueryWithTransientRetry(CurlHandle &handle, std::string_view path, const CurlOptions &opts,
                                         int maxAttempts = 5) {
  using namespace std::chrono_literals;
  std::string_view lastResp;
  for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
    try {
      lastResp = handle.query(path, opts);
      if (!IsLikelyTransientServiceUnavailable(lastResp)) {
        return lastResp;  // success
      }
    } catch (const exception &) {
      if (attempt == maxAttempts) {
        throw;  // propagate after final attempt
      }
    }
    if (attempt < maxAttempts) {
      // Backoff: 100ms, 150ms, 225ms, 337ms ... capped implicitly by attempts
      auto sleepDur = 100ms;
      for (int i = 1; i < attempt; ++i) {
        sleepDur += sleepDur / 2;  // multiply by 1.5 each time
      }
      std::this_thread::sleep_for(sleepDur);
    }
  }
  return lastResp;  // Return last response (may still be transient error text)
}
}  // namespace

class ExampleBaseCurlHandle : public ::testing::Test {
 protected:
  static constexpr std::string_view kHttpBinBase = "https://httpbin.org";

  CurlHandle handle{kHttpBinBase};
};

TEST_F(ExampleBaseCurlHandle, CurlVersion) { EXPECT_FALSE(GetCurlVersionInfo().empty()); }

TEST_F(ExampleBaseCurlHandle, QueryJsonAndMoveConstruct) {
  CurlOptions opts = kVerboseHttpGetOptions;
  opts.mutableHttpHeaders().emplace_back("MyHeaderIsVeryLongToAvoidSSO", "Val1");
  auto jsonResp = QueryWithTransientRetry(handle, "/json", opts);
  EXPECT_NE(std::string_view(jsonResp).find("slideshow"), std::string_view::npos);

  CurlHandle newCurlHandle = std::move(handle);
  auto jsonResp2 = QueryWithTransientRetry(newCurlHandle, "/json", opts);
  EXPECT_NE(std::string_view(jsonResp2).find("slideshow"), std::string_view::npos);
}

TEST_F(ExampleBaseCurlHandle, QueryXmlAndMoveAssign) {
  auto xmlResp = QueryWithTransientRetry(handle, "/xml", kVerboseHttpGetOptions);
  EXPECT_NE(std::string_view(xmlResp).find("<?xml"), std::string_view::npos);

  CurlHandle newCurlHandle;
  newCurlHandle = std::move(handle);
  auto xmlResp2 = QueryWithTransientRetry(newCurlHandle, "/xml", kVerboseHttpGetOptions);
  EXPECT_NE(std::string_view(xmlResp2).find("<?xml"), std::string_view::npos);
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