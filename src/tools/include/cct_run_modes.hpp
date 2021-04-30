#pragma once

namespace cct {
namespace settings {
enum RunMode {
  kProd = 0,
  kProxy,  // proxy : capture & match requests from proxy
  kTest,   // proxy + use test keys
};

}  // namespace settings

inline bool ProxyRequested(settings::RunMode mode) { return mode >= settings::kProxy; }
}  // namespace cct
