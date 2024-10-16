#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include "cct_type_traits.hpp"
#include "curlpostdata.hpp"
#include "flatkeyvaluestring.hpp"
#include "httprequesttype.hpp"

namespace cct {

class CurlOptions {
 public:
  // Optimization: may get a null-terminated const char * for each kv pair.
  // See usage in CurlHandle for more information.
  using HttpHeaders = FlatKeyValueString<'\0', ':'>;

  enum class Verbose : int8_t { kOff, kOn };
  enum class PostDataFormat : int8_t { kString, json };

  explicit CurlOptions(HttpRequestType requestType, Verbose verbose = Verbose::kOff)
      : _verbose(verbose == Verbose::kOn), _requestType(requestType) {}

  CurlOptions(HttpRequestType requestType, CurlPostData postData,
              PostDataFormat postDataFormat = PostDataFormat::kString, Verbose verbose = Verbose::kOff)
      : _postdata(std::move(postData)), _verbose(verbose == Verbose::kOn), _requestType(requestType) {
    if (postDataFormat == PostDataFormat::json) {
      setPostDataInJsonFormat();
    }
  }

  HttpHeaders &mutableHttpHeaders() { return _httpHeaders; }
  const HttpHeaders &httpHeaders() const { return _httpHeaders; }

  const char *proxyUrl() const { return _proxyUrl; }

  void setProxyUrl(const char *proxyUrl, bool reset = false) {
    _proxyUrl = proxyUrl;
    _proxyReset = reset;
  }

  CurlPostData &mutablePostData() { return _postdata; }
  const CurlPostData &postData() const { return _postdata; }

  bool isProxyReset() const { return _proxyReset; }

  bool isVerbose() const { return _verbose; }

  bool isPostDataInJsonFormat() const { return _postdataInJsonFormat; }

  HttpRequestType requestType() const { return _requestType; }

  using trivially_relocatable =
      std::bool_constant<is_trivially_relocatable_v<HttpHeaders> && is_trivially_relocatable_v<CurlPostData>>::type;

 private:
  void setPostDataInJsonFormat() {
    _httpHeaders.emplace_back("Content-Type", "application/json");
    _postdataInJsonFormat = true;
  }

  HttpHeaders _httpHeaders;
  const char *_proxyUrl = nullptr;
  CurlPostData _postdata;
  bool _proxyReset = false;
  bool _verbose = false;
  bool _postdataInJsonFormat = false;
  HttpRequestType _requestType = HttpRequestType::kGet;
};

}  // namespace cct