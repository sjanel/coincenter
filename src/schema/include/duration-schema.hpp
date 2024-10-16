#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "cct_hash.hpp"
#include "cct_json-serialization.hpp"
#include "durationstring.hpp"
#include "generic-object-json.hpp"
#include "timedef.hpp"

namespace cct::schema {

struct Duration {
  auto operator<=>(const Duration &) const noexcept = default;

  ::cct::Duration duration{};
};

}  // namespace cct::schema

namespace std {
template <>
struct hash<::cct::schema::Duration> {
  auto operator()(const ::cct::schema::Duration &val) const {
    return ::cct::HashValue64(
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(val.duration).count()));
  }
};
}  // namespace std

template <>
struct glz::meta<::cct::schema::Duration> {
  static constexpr auto value{&::cct::schema::Duration::duration};
};

namespace glz::detail {
template <>
struct from<JSON, ::cct::schema::Duration> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value.duration = ::cct::ParseDuration(std::string_view(it, endIt));
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::schema::Duration> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    char buf[30];
    static constexpr int kNbSignificantUnits = 10;
    auto adjustedBuf = ::cct::DurationToBuffer(value.duration, buf, kNbSignificantUnits);
    auto valueLen = adjustedBuf.size();
    bool withQuotes = ::cct::details::JsonWithQuotes<Opts>(b, ix);
    int64_t additionalSize = (withQuotes ? 2L : 0L) + static_cast<int64_t>(ix) + static_cast<int64_t>(valueLen) -
                             static_cast<int64_t>(b.size());
    if (additionalSize > 0) {
      b.append(additionalSize, ' ');
    }

    if (withQuotes) {
      b[ix++] = '"';
    }
    std::ranges::copy(adjustedBuf, b.data() + ix);
    ix += valueLen;
    if (withQuotes) {
      b[ix++] = '"';
    }
  }
};
}  // namespace glz::detail