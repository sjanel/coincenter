#pragma once

namespace cct {
namespace settings {
enum class RunMode {
  kProd,
  kProxy,  // proxy : capture & match requests from proxy
  kTest,   // proxy + use test keys
};

}  // namespace settings

inline bool IsProxyRequested(settings::RunMode mode) { return mode >= settings::RunMode::kProxy; }
}  // namespace cct
