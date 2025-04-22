#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "cct_hash.hpp"
#include "cct_json.hpp"
#include "generic-object-json.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct::schema {

struct TimePoint {
  auto operator<=>(const TimePoint &) const noexcept = default;

  //                             2023   -   01    -   01    T   12    :   34    :   56    Z
  static constexpr std::size_t strLen() { return 4U + 1U + 2U + 1U + 2U + 1U + 2U + 1U + 2U + 1U + 2U + 1U; }

  char *appendTo(char *buf) const { return std::ranges::copy(TimeToString(ts), buf).out; }

  ::cct::TimePoint ts{};
};

}  // namespace cct::schema

namespace std {
template <>
struct hash<::cct::schema::TimePoint> {
  auto operator()(const ::cct::schema::TimePoint &val) const {
    return ::cct::HashValue64(static_cast<uint64_t>(val.ts.time_since_epoch().count()));
  }
};
}  // namespace std

template <>
struct glz::meta<::cct::schema::TimePoint> {
  static constexpr auto value{&::cct::schema::TimePoint::ts};
};

namespace glz {
template <>
struct from<JSON, ::cct::schema::TimePoint> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value.ts = ::cct::StringToTime(std::string_view(it, endIt));
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::schema::TimePoint> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    ::cct::details::ToStrLikeJson<Opts>(value, b, ix);
  }
};
}  // namespace glz