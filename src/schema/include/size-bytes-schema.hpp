#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "cct_hash.hpp"
#include "cct_json.hpp"
#include "generic-object-json.hpp"
#include "unitsparser.hpp"

namespace cct::schema {

struct SizeBytes {
  auto operator<=>(const SizeBytes &) const noexcept = default;

  int64_t sizeInBytes{};
};

}  // namespace cct::schema

namespace std {
template <>
struct hash<::cct::schema::SizeBytes> {
  auto operator()(const ::cct::schema::SizeBytes &val) const {
    return ::cct::HashValue64(static_cast<uint64_t>(val.sizeInBytes));
  }
};
}  // namespace std

template <>
struct glz::meta<::cct::schema::SizeBytes> {
  static constexpr auto value{&::cct::schema::SizeBytes::sizeInBytes};
};

namespace glz {
template <>
struct from<JSON, ::cct::schema::SizeBytes> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value.sizeInBytes = ::cct::ParseNumberOfBytes(std::string_view(it, endIt));
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::schema::SizeBytes> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    char buf[30];
    auto adjustedBuf = ::cct::BytesToStr(value.sizeInBytes, buf);
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
}  // namespace glz