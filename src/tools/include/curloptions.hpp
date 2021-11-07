#pragma once

#include <string_view>
#include <type_traits>

#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "flatkeyvaluestring.hpp"

namespace cct {

using CurlPostData = FlatKeyValueString<'&', '='>;

class CurlOptions {
 public:
  enum class RequestType { kGet, kPost, kDelete };

  template <class CurlPostDataT = CurlPostData>
  explicit CurlOptions(RequestType requestType, CurlPostDataT &&ipostData = CurlPostDataT(), const char *ua = nullptr,
                       const char *pUrl = nullptr, bool v = false)
      : userAgent(ua),
        proxy(false, pUrl),
        postdata(std::forward<CurlPostDataT>(ipostData)),
        verbose(v),
        _requestType(requestType) {}

  RequestType requestType() const { return _requestType; }

  std::string_view requestTypeStr() const {
    return _requestType == RequestType::kGet ? "GET" : (_requestType == RequestType::kPost ? "POST" : "DELETE");
  }

  vector<string> httpHeaders;

  const char *userAgent;

  struct ProxySettings {
    explicit ProxySettings(bool reset = false, const char *url = nullptr) : _reset(reset), _url(url) {}
    // Required if at query level we want to avoid use of proxy
    bool _reset;
    const char *_url;
  } proxy;

  CurlPostData postdata;
  bool verbose;
  bool postdataInJsonFormat = false;
  bool followLocation = false;

 private:
  RequestType _requestType;
};

}  // namespace cct