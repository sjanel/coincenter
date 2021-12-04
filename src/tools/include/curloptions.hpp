#pragma once

#include <string_view>
#include <type_traits>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "flatkeyvaluestring.hpp"
#include "httprequesttype.hpp"

namespace cct {

using CurlPostData = FlatKeyValueString<'&', '='>;

class CurlOptions {
 public:
  template <class CurlPostDataT = CurlPostData>
  explicit CurlOptions(HttpRequestType requestType, CurlPostDataT &&ipostData = CurlPostDataT(),
                       const char *ua = nullptr, const char *pUrl = nullptr, bool v = false)
      : userAgent(ua),
        proxy(false, pUrl),
        postdata(std::forward<CurlPostDataT>(ipostData)),
        verbose(v),
        _requestType(requestType) {}

  HttpRequestType requestType() const { return _requestType; }

  std::string_view requestTypeStr() const { return ToString(_requestType); }

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
  HttpRequestType _requestType;
};

}  // namespace cct