#pragma once

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>

#include "cct_vector.hpp"

namespace cct {
class CurlPostData {
 public:
  using KeyValuePair = std::array<std::string_view, 2>;

  CurlPostData() = default;

  CurlPostData(std::initializer_list<KeyValuePair> init);

  explicit CurlPostData(std::string &&rawPostData) : _postdata(std::move(rawPostData)) {}

  CurlPostData &operator=(std::initializer_list<KeyValuePair> init);

  /// Append a new URL option from given key value pair.
  void append(std::string_view key, std::string_view value);

  template <class T, typename std::enable_if_t<std::is_integral_v<T>, bool> = true>
  void append(std::string_view key, T value) {
    append(key, std::to_string(value));
  }

  /// Updates the value for given key, or append if not existing.
  void set(std::string_view key, std::string_view value);

  template <class T, typename std::enable_if_t<std::is_integral_v<T>, bool> = true>
  void set(std::string_view key, T value) {
    set(key, std::to_string(value));
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

 private:
  /// Finds the position of the given key
  std::size_t find(std::string_view key) const;

  std::string _postdata;
};

class CurlOptions {
 public:
  enum class RequestType { kGet, kPost, kDelete };

  explicit CurlOptions(RequestType requestType) : CurlOptions(requestType, CurlPostData()) {}

  template <class CurlPostDataT>
  CurlOptions(RequestType requestType, CurlPostDataT &&ipostData, const char *ua = nullptr, const char *pUrl = nullptr,
              bool v = false)
      : userAgent(ua),
        proxy(false, pUrl),
        postdata(std::forward<CurlPostDataT>(ipostData)),
        verbose(v),
        _requestType(requestType) {}

  RequestType requestType() const { return _requestType; }
  std::string_view requestTypeStr() const {
    return _requestType == RequestType::kGet ? "GET" : (_requestType == RequestType::kPost ? "POST" : "DELETE");
  }

  cct::vector<std::string> httpHeaders;
  const char *userAgent;

  struct ProxySettings {
    ProxySettings(bool reset = false, const char *url = nullptr) : _reset(reset), _url(url) {}
    /// Required if at query level we want to avoid use of proxy
    bool _reset;
    const char *_url;
  } proxy;

  CurlPostData postdata;
  bool verbose;

 private:
  RequestType _requestType;
};

}  // namespace cct