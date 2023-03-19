#pragma once

#include <string_view>
#include <type_traits>

#include "flatkeyvaluestring.hpp"
#include "httprequesttype.hpp"

namespace cct {

using CurlPostData = FlatKeyValueString<'&', '='>;

class CurlOptions {
 public:
  // Trick: may get a null-terminated const char * for each kv pair.
  // See usage in CurlHandle for more information.
  using HttpHeaders = FlatKeyValueString<'\0', ':'>;

  enum class Verbose : int8_t { kOff, kOn };
  enum class PostDataFormat : int8_t { kString, kJson };

  explicit CurlOptions(HttpRequestType requestType, const char *userAgent = nullptr, Verbose verbose = Verbose::kOff)
      : _userAgent(userAgent), _verbose(verbose == Verbose::kOn), _requestType(requestType) {}

  CurlOptions(HttpRequestType requestType, const CurlPostData &postData, const char *userAgent = nullptr,
              PostDataFormat postDataFormat = PostDataFormat::kString, Verbose verbose = Verbose::kOff)
      : _userAgent(userAgent), _postdata(postData), _verbose(verbose == Verbose::kOn), _requestType(requestType) {
    if (postDataFormat == PostDataFormat::kJson) {
      setPostDataInJsonFormat();
    }
  }

  CurlOptions(HttpRequestType requestType, CurlPostData &&postData, const char *userAgent = nullptr,
              PostDataFormat postDataFormat = PostDataFormat::kString, Verbose verbose = Verbose::kOff)
      : _userAgent(userAgent),
        _postdata(std::move(postData)),
        _verbose(verbose == Verbose::kOn),
        _requestType(requestType) {
    if (postDataFormat == PostDataFormat::kJson) {
      setPostDataInJsonFormat();
    }
  }

  const HttpHeaders &getHttpHeaders() const { return _httpHeaders; }

  const char *getUserAgent() const { return _userAgent; }

  const char *getProxyUrl() const { return _proxyUrl; }

  void setProxyUrl(const char *proxyUrl, bool reset = false) {
    _proxyUrl = proxyUrl;
    _proxyReset = reset;
  }

  CurlPostData &getPostData() { return _postdata; }
  const CurlPostData &getPostData() const { return _postdata; }

  bool isProxyReset() const { return _proxyReset; }

  bool isVerbose() const { return _verbose; }

  bool isPostDataInJsonFormat() const { return _postdataInJsonFormat; }

  bool isFollowLocation() const { return _followLocation; }

  HttpRequestType requestType() const { return _requestType; }

  void clearHttpHeaders() { _httpHeaders.clear(); }

  void appendHttpHeader(std::string_view key, std::string_view value) { _httpHeaders.append(key, value); }
  void appendHttpHeader(std::string_view key, std::integral auto value) { _httpHeaders.append(key, value); }

  void setHttpHeader(std::string_view key, std::string_view value) { _httpHeaders.set(key, value); }
  void setHttpHeader(std::string_view key, std::integral auto value) { _httpHeaders.set(key, value); }

  using trivially_relocatable = is_trivially_relocatable<HttpHeaders>::type;

 private:
  void setPostDataInJsonFormat() {
    _httpHeaders.append("Content-Type", "application/json");
    _postdataInJsonFormat = true;
  }

  HttpHeaders _httpHeaders;
  const char *_userAgent = nullptr;
  const char *_proxyUrl = nullptr;
  CurlPostData _postdata;
  bool _proxyReset = false;
  bool _verbose = false;
  bool _postdataInJsonFormat = false;
  bool _followLocation = false;
  HttpRequestType _requestType = HttpRequestType::kGet;
};

}  // namespace cct