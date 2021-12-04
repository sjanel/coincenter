#pragma once

#include <cstdint>
#include <string_view>

#include "unreachable.hpp"

namespace cct {
enum class HttpRequestType : int8_t { kGet, kPost, kDelete };

// TODO: avoid code duplication below
static constexpr HttpRequestType kAllHttpRequestsTypes[] = {HttpRequestType::kGet, HttpRequestType::kPost,
                                                            HttpRequestType::kDelete};

constexpr std::string_view ToString(HttpRequestType requestType) {
  switch (requestType) {
    case HttpRequestType::kGet:
      return "GET";
    case HttpRequestType::kPost:
      return "POST";
    case HttpRequestType::kDelete:
      return "DELETE";
    default:
      unreachable();
  }
}

}  // namespace cct