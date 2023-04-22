#pragma once

#include <cstdint>

namespace cct {
namespace settings {
enum class RunMode : int8_t {
  kProd,
  kTestKeys,
  kProxy,              // proxy : capture & match requests from proxy
  kTestKeysWithProxy,  // proxy + use test keys
};

}  // namespace settings

constexpr bool IsProxyRequested(settings::RunMode runMode) {
  switch (runMode) {
    case settings::RunMode::kProxy:
      [[fallthrough]];
    case settings::RunMode::kTestKeysWithProxy:
      return true;
    default:
      return false;
  }
}

constexpr bool AreTestKeysRequested(settings::RunMode runMode) {
  switch (runMode) {
    case settings::RunMode::kTestKeys:
      [[fallthrough]];
    case settings::RunMode::kTestKeysWithProxy:
      return true;
    default:
      return false;
  }
}
}  // namespace cct
