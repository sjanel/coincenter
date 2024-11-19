#pragma once

#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"
#include "cct_string.hpp"

namespace cct {

template <bool prettify = false>
string WriteJsonOrThrow(const auto &obj) {
  string buf;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::write<json::opts{.prettify = prettify, .indentation_width = 2, .raw_string = true}>(obj, buf);

  if (ec) {
    throw exception("Error while writing json content: {}", format_error(ec, buf));
  }

  return buf;
}

}  // namespace cct