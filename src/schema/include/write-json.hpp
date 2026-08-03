#pragma once

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

// {raw_string, error_on_const_read, indentation_width} are opts_ex (derived) members; base members
// (prettify, minified...) go in the nested brace.
inline constexpr auto kPrettifyJsonOptions =
    json::opts_ex{{.prettify = true}, /*raw_string*/ true, /*error_on_const_read*/ false,
                  /*indentation_width*/ 2};

inline constexpr auto kMinifiedJsonOptions = json::opts_ex{{.minified = true}, /*raw_string*/ true};

template <auto opts = kMinifiedJsonOptions>
string WriteJsonOrThrow(const auto &obj) {
  string buf;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::write<opts>(obj, buf);

  if (ec) {
    throw exception("Error while writing json content: {}", format_error(ec, buf));
  }

  return buf;
}

string WritePrettyJsonOrThrow(const auto &obj) { return WriteJsonOrThrow<kPrettifyJsonOptions>(obj); }

}  // namespace cct