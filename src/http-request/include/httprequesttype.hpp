#pragma once

#include <cstdint>
#include <string_view>

#include "unreachable.hpp"

namespace cct {
enum class HttpRequestType : int8_t { kGet, kPut, kPost, kDelete };

inline constexpr HttpRequestType kHttpRequestTypes[] = {HttpRequestType::kGet, HttpRequestType::kPost,
                                                        HttpRequestType::kPut, HttpRequestType::kDelete};

constexpr std::string_view HttpRequestTypeToString(HttpRequestType requestType) {
  switch (requestType) {
    case HttpRequestType::kGet:
      return "GET";
    case HttpRequestType::kPost:
      return "POST";
    case HttpRequestType::kPut:
      return "PUT";
    case HttpRequestType::kDelete:
      return "DELETE";
    default:
      unreachable();
  }
}

}  // namespace cct