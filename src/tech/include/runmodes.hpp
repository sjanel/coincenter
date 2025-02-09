#pragma once

#include <cstdint>

namespace cct::settings {

enum class RunMode : int8_t {
  kProd,
  kTestKeys,
  kProxy,                   // proxy : capture & match requests from proxy
  kTestKeysWithProxy,       // proxy + use test keys
  kQueryResponseOverriden,  // Unit test mode - no external call is made, response is provided with test keys
};

constexpr bool IsProxyRequested(RunMode runMode) {
  switch (runMode) {
    case RunMode::kProxy:
      [[fallthrough]];
    case RunMode::kTestKeysWithProxy:
      return true;
    default:
      return false;
  }
}

constexpr bool AreTestKeysRequested(RunMode runMode) {
  switch (runMode) {
    case RunMode::kTestKeys:
      [[fallthrough]];
    case RunMode::kQueryResponseOverriden:
      [[fallthrough]];
    case RunMode::kTestKeysWithProxy:
      return true;
    default:
      return false;
  }
}

constexpr bool AreQueryResponsesOverriden(RunMode runMode) {
  switch (runMode) {
    case RunMode::kQueryResponseOverriden:
      return true;
    default:
      return false;
  }
}

}  // namespace cct::settings
