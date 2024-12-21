#pragma once

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

static constexpr auto kPrettifyJsonOptions =
    json::opts{.prettify = true,        // NOLINT(readability-implicit-bool-conversion)
               .indentation_width = 2,  // NOLINT(readability-implicit-bool-conversion)
               .raw_string = true};     // NOLINT(readability-implicit-bool-conversion)

static constexpr auto kMinifiedJsonOptions =
    json::opts{.prettify = false,    // NOLINT(readability-implicit-bool-conversion)
               .minified = true,     // NOLINT(readability-implicit-bool-conversion)
               .raw_string = true};  // NOLINT(readability-implicit-bool-conversion)

template <json::opts opts = kMinifiedJsonOptions>
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