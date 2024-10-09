#pragma once

#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"
#include "reader.hpp"

namespace cct {

namespace {
constexpr auto JsonOptions = opts{.raw_string = true};
}

template <class T>
T ReadJsonOrThrow(std::string_view strContent) {
  T outObject;

  auto ec = read<JsonOptions>(outObject, strContent);

  if (ec) {
    throw exception("Error while reading json content: {}", format_error(ec, strContent));
  }

  return outObject;
}

template <class T>
T ReadJsonOrThrow(const Reader &reader) {
  return ReadJsonOrThrow<T>(reader.readAll());
}

}  // namespace cct