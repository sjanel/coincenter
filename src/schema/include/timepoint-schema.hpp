#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "cct_hash.hpp"
#include "cct_json-serialization.hpp"
#include "generic-object-json.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct::schema {

struct TimePoint {
  auto operator<=>(const TimePoint &) const noexcept = default;

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

namespace glz::detail {
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
    auto timeStr = ::cct::TimeToString(value.ts);
    auto valueLen = timeStr.size();
    bool withQuotes = ::cct::details::JsonWithQuotes<Opts>(b, ix);
    int64_t additionalSize = (withQuotes ? 2L : 0L) + static_cast<int64_t>(ix) + static_cast<int64_t>(valueLen) -
                             static_cast<int64_t>(b.size());
    if (additionalSize > 0) {
      b.append(additionalSize, ' ');
    }

    if (withQuotes) {
      b[ix++] = '"';
    }
    std::ranges::copy(timeStr, b.data() + ix);
    ix += valueLen;
    if (withQuotes) {
      b[ix++] = '"';
    }
  }
};
}  // namespace glz::detail