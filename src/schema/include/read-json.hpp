#pragma once

#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"
#include "file.hpp"
#include "reader.hpp"
#include "write-json.hpp"

namespace cct {

namespace {
constexpr auto JsonOptions = json::opts{.raw_string = true};  // NOLINT(readability-implicit-bool-conversion)
}

void ReadJsonOrThrow(std::string_view strContent, auto &outObject) {
  auto ec = json::read<JsonOptions>(outObject, strContent);

  if (ec) {
    std::string_view prefixJsonContent = strContent.substr(0, std::min<int>(strContent.size(), 20));
    throw exception("Error while reading json content '{}{}': {}", prefixJsonContent,
                    prefixJsonContent.size() < strContent.size() ? "..." : "", json::format_error(ec, strContent));
  }
}

template <class T>
T ReadJsonOrThrow(std::string_view strContent) {
  T outObject;
  ReadJsonOrThrow(strContent, outObject);
  return outObject;
}

template <class T>
T ReadJsonOrThrow(const Reader &reader) {
  return ReadJsonOrThrow<T>(reader.readAll());
}

template <class T>
T ReadJsonOrCreateFile(const File &file) {
  T outObject;
  if (file.exists()) {
    ReadJsonOrThrow(file.readAll(), outObject);
  } else {
    file.write(WriteJsonOrThrow<true>(outObject));
  }
  return outObject;
}

}  // namespace cct