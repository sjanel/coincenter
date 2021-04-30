#pragma once

namespace cct {

constexpr const char* GetProxyURL() {
#ifdef CCT_URL_PROXY
  return CCT_URL_PROXY;
#else
  return nullptr;
#endif
}

constexpr bool IsProxyAvailable() {
#ifdef CCT_URL_PROXY
  return true;
#else
  return false;
#endif
}

/// Return path to TLS certificates
constexpr const char* GetProxyCAInfo() {
#ifdef CCT_PROXY_CAINFO
  return CCT_PROXY_CAINFO;
#else
  return "";
#endif
}
}  // namespace cct