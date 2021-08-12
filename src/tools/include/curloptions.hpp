#pragma once

#include <charconv>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "cct_json.hpp"
#include "cct_vector.hpp"

namespace cct {

class CurlPostData {
 public:
  struct KeyValuePair {
    using IntegralType = int64_t;
    using value_type = std::variant<std::string, std::string_view, IntegralType>;

    KeyValuePair(std::string_view k, std::string_view v) : key(k), val(v) {}

    KeyValuePair(std::string_view k, const char *v) : key(k), val(std::string_view(v)) {}

    KeyValuePair(std::string_view k, std::string v) : key(k), val(std::move(v)) {}

    KeyValuePair(std::string_view k, IntegralType v) : key(k), val(v) {}

    std::string_view key;
    value_type val;
  };

  CurlPostData() noexcept(std::is_nothrow_default_constructible_v<std::string>) = default;

  CurlPostData(std::initializer_list<KeyValuePair> init);

  explicit CurlPostData(std::string &&rawPostData) noexcept(std::is_nothrow_move_constructible_v<std::string>)
      : _postdata(std::move(rawPostData)) {}

  /// Append a new URL option from given key value pair.
  void append(std::string_view key, std::string_view value);

  template <class T, typename std::enable_if_t<std::is_integral_v<T>, bool> = true>
  void append(std::string_view key, T value) {
    char buf[std::numeric_limits<T>::digits10 + 2];  // + 1 for minus, +1 for additional partial ranges coverage
    auto ret = std::to_chars(std::begin(buf), std::end(buf), value);
    append(key, std::string_view(buf, ret.ptr));
  }

  /// Appends content of other CurlPostData into 'this'.
  /// No check is made on duplicated keys, it is client's responsibility to make sure keys are not duplicated.
  void append(const CurlPostData &o);

  /// Updates the value for given key, or append if not existing.
  void set(std::string_view key, std::string_view value);

  template <class T, typename std::enable_if_t<std::is_integral_v<T>, bool> = true>
  void set(std::string_view key, T value) {
    char buf[std::numeric_limits<T>::digits10 + 2];  // + 1 for minus, +1 for additional partial ranges coverage
    auto ret = std::to_chars(std::begin(buf), std::end(buf), value);
    set(key, std::string_view(buf, ret.ptr));
  }

  /// Erases given key if present.
  void erase(std::string_view key);

  bool contains(std::string_view key) const { return find(key) != std::string::npos; }

  /// Get the value associated to given key
  std::string_view get(std::string_view key) const;

  bool empty() const { return _postdata.empty(); }

  const char *c_str() const { return _postdata.c_str(); }

  void clear() noexcept { _postdata.clear(); }

  std::string_view toStringView() const { return _postdata; }

  const std::string &toString() const { return _postdata; }

  json toJson() const;

 private:
  /// Finds the position of the given key
  std::size_t find(std::string_view key) const;

  std::string _postdata;
};

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
        postdataInJsonFormat(false),
        followLocation(false),
        _requestType(requestType) {}

  RequestType requestType() const { return _requestType; }

  std::string_view requestTypeStr() const {
    return _requestType == RequestType::kGet ? "GET" : (_requestType == RequestType::kPost ? "POST" : "DELETE");
  }

  cct::vector<std::string> httpHeaders;

  const char *userAgent;

  struct ProxySettings {
    explicit ProxySettings(bool reset = false, const char *url = nullptr) : _reset(reset), _url(url) {}
    // Required if at query level we want to avoid use of proxy
    bool _reset;
    const char *_url;
  } proxy;

  CurlPostData postdata;
  bool verbose;
  bool postdataInJsonFormat;
  bool followLocation;

 private:
  RequestType _requestType;
};

}  // namespace cct