#pragma once

#include <glaze/glaze.hpp>  // IWYU pragma: export
#include <string_view>
#include <type_traits>

namespace cct {

namespace json {
using glz::error_ctx;
using glz::format_error;
using glz::meta;
using glz::opts;
using glz::read;
using glz::reflect;
using glz::write;
}  // namespace json

/**
 * Get the string representation of an enum value, provided that enum values are contiguous and start at 0, and that
 * they are specialized with glz::meta.
 */
constexpr std::string_view EnumToString(auto enumValue) {
  using T = std::remove_cvref_t<decltype(enumValue)>;
  return json::reflect<T>::keys[static_cast<std::underlying_type_t<T>>(enumValue)];
}

}  // namespace cct