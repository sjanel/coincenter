#pragma once

#include <cstdint>

#include <glaze/glaze.hpp>  // IWYU pragma: export

namespace cct::json {
using glz::colwise;
using glz::CSV;
using glz::error_ctx;
using glz::format_error;
using glz::meta;
using glz::opts;
using glz::opts_csv;
using glz::read;
using glz::reflect;
using glz::write;

// Extended Glaze options.
// Since Glaze v7, several options are no longer part of the core glz::opts struct. To use them they
// must be provided through a user-defined options struct inheriting from glz::opts (Glaze detects the
// extra members generically via `requires`). This bundles the extended options coincenter relies on.
// Note: only opts_ex's own (derived) members may be set with designated initializers; base members
// (prettify, minified, error_on_unknown_keys...) must be set through a nested brace, e.g.
//   json::opts_ex{{.minified = true}, /*raw_string*/ true}
struct opts_ex : opts {
  bool raw_string = false;           // skip escape encode/decode for strings (faster; no escaped chars)
  bool error_on_const_read = false;  // error when reading into a const value instead of silently skipping
  uint8_t indentation_width = 3;     // prettified JSON indentation width
};
}  // namespace cct::json
