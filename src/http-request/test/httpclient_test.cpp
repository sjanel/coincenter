#include "httpclient.hpp"

#include <gtest/gtest.h>

#include <aeronet/aeronet-server.hpp>
#include <cstdint>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "httprequestoptions.hpp"
#include "httppostdata.hpp"
#include "httprequesttype.hpp"
#include "permanentrequestoptions.hpp"
#include "proxy.hpp"
#include "runmodes.hpp"
#include "stringconv.hpp"

namespace cct {
namespace {
const HttpRequestOptions kVerboseHttpGetOptions(HttpRequestType::kGet, HttpRequestOptions::Verbose::kOn);

constexpr std::string_view kJsonBody =
    R"({"slideshow":{"author":"Yours Truly","title":"Sample Slide Show","slides":[{"title":"Wake up to WonderWidgets!","type":"all"}]}})";
constexpr std::string_view kXmlBody =
    R"(<?xml version="1.0" encoding="utf-8"?><slideshow title="Sample Slide Show"><slide type="all"><title>Wake up to WonderWidgets!</title></slide></slideshow>)";

// Local server bound to an ephemeral port, serving the payloads previously fetched from httpbin.org.
// Keeps this test hermetic - no dependency on the availability of an external service.
aeronet::SingleHttpServer CreateTestServer() {
  aeronet::Router router;
  router.setPath(aeronet::http::Method::GET, "/json", [](const aeronet::HttpRequest& req) {
    return req.makeResponse(kJsonBody, "application/json");
  });
  router.setPath(aeronet::http::Method::GET, "/xml", [](const aeronet::HttpRequest& req) {
    return req.makeResponse(kXmlBody, "application/xml");
  });
  return aeronet::SingleHttpServer(aeronet::HttpServerConfig{}, std::move(router));
}

string BuildBaseUrl(uint16_t port) {
  string baseUrl("http://127.0.0.1:");
  AppendIntegralToString(baseUrl, port);
  return baseUrl;
}
}  // namespace

class ExampleBaseHttpClient : public ::testing::Test {
 protected:
  // start() runs the event loop in a background thread owned by the server; the destructor stops it.
  ExampleBaseHttpClient() { _server.start(); }

  aeronet::SingleHttpServer _server{CreateTestServer()};
  string _baseUrl{BuildBaseUrl(_server.port())};
  // BestURLPicker keeps the address of the given string_view - it must outlive 'handle'.
  std::string_view _baseUrlView{_baseUrl};

  HttpClient handle{_baseUrlView};
};

TEST_F(ExampleBaseHttpClient, HttpClientVersion) { EXPECT_FALSE(GetHttpClientVersionInfo().empty()); }

TEST_F(ExampleBaseHttpClient, QueryJsonAndMoveConstruct) {
  HttpRequestOptions opts = kVerboseHttpGetOptions;
  opts.mutableHttpHeaders().emplace_back("MyHeaderIsVeryLongToAvoidSSO", "Val1");
  auto jsonResp = handle.query("/json", opts);
  EXPECT_NE(std::string_view(jsonResp).find("slideshow"), std::string_view::npos);

  HttpClient newHttpClient = std::move(handle);
  auto jsonResp2 = newHttpClient.query("/json", opts);
  EXPECT_NE(std::string_view(jsonResp2).find("slideshow"), std::string_view::npos);
}

TEST_F(ExampleBaseHttpClient, QueryXmlAndMoveAssign) {
  auto xmlResp = handle.query("/xml", kVerboseHttpGetOptions);
  EXPECT_NE(std::string_view(xmlResp).find("<?xml"), std::string_view::npos);

  HttpClient newHttpClient;
  newHttpClient = std::move(handle);
  auto xmlResp2 = newHttpClient.query("/xml", kVerboseHttpGetOptions);
  EXPECT_NE(std::string_view(xmlResp2).find("<?xml"), std::string_view::npos);
}

class HttpClientProxyTest : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://live.cardeasexml.com/ultradns.php";

  HttpClient handle{kTestUrl};
};

TEST_F(HttpClientProxyTest, ProxyMockTest) {
  if (IsProxyAvailable()) {
    HttpRequestOptions opts = kVerboseHttpGetOptions;
    opts.setProxyUrl(GetProxyURL());
    EXPECT_EQ(handle.query("", opts), "POOL_LEFT");
  }
}

class TestOverrideQueryResponses : public ::testing::Test {
 protected:
  static constexpr std::string_view kTestUrl = "https://this-url-does-not-exist-12345";

  HttpRequestOptions emptyOpts{HttpRequestType::kGet};
  HttpRequestOptions param1OptsGet{HttpRequestType::kGet, HttpPostData{{"param1", "v"}}};
  HttpRequestOptions param1OptsPost{HttpRequestType::kPost, HttpPostData{{"param1", "v"}}};

  AbstractMetricGateway *pAbstractMetricGateway = nullptr;
  settings::RunMode runMode = settings::RunMode::kQueryResponseOverriden;
  HttpClient handle{kTestUrl, pAbstractMetricGateway, PermanentRequestOptions(), runMode};
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
