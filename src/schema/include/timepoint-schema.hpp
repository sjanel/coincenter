#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "cct_hash.hpp"
#include "cct_json.hpp"
#include "generic-object-json.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct::schema {

struct TimePointIso8601UTC {
  auto operator<=>(const TimePointIso8601UTC &) const noexcept = default;

  // 'YYYY-MM-DDTHH:MM:SSZ'
  static constexpr std::size_t strLen() { return 20U; }

  char *appendTo(char *buf) const { return TimeToStringIso8601UTC(ts, buf); }

  ::cct::TimePoint ts;
};

}  // namespace cct::schema

namespace std {
template <>
struct hash<::cct::schema::TimePointIso8601UTC> {
  auto operator()(const ::cct::schema::TimePointIso8601UTC &val) const {
    return ::cct::HashValue64(static_cast<uint64_t>(val.ts.time_since_epoch().count()));
  }
};
}  // namespace std

template <>
struct glz::meta<::cct::schema::TimePointIso8601UTC> {
  static constexpr auto value{&::cct::schema::TimePointIso8601UTC::ts};
};

namespace glz {
template <>
struct from<JSON, ::cct::schema::TimePointIso8601UTC> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value.ts = ::cct::StringToTimeISO8601UTC(it, endIt);
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::schema::TimePointIso8601UTC> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    ::cct::details::ToStrLikeJson<Opts>(value, b, ix);
  }
};
}  // namespace glz