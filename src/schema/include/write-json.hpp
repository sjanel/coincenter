#pragma once

#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"
#include "cct_string.hpp"
#include "writer.hpp"

namespace cct {

template <bool formatted = false>
string WriteJsonOrThrow(const auto &obj) {
  string buf;
  auto ec = write<glz::opts{.raw_string = true, .prettify = formatted, .indentation_width = 2}>(obj, buf);

  if (ec) {
    throw exception("Error while writing json content: {}", format_error(ec, strContent));
  }

  return buf;
}

template <class T>
T ReadJsonOrThrow(const Reader &reader) {
  return ReadJsonOrThrow<T>(reader.readAll());
}

}  // namespace cct