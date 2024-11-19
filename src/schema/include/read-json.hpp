#pragma once

#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"
#include "file.hpp"
#include "reader.hpp"
#include "write-json.hpp"

namespace cct {

namespace {
constexpr auto kJsonOptions =
    json::opts{.error_on_const_read = true, .raw_string = true};  // NOLINT(readability-implicit-bool-conversion)
}

template <json::opts opts = kJsonOptions>
void ReadJsonOrThrow(std::string_view strContent, auto &outObject) {
  if (strContent.empty()) {
    return;
  }

  auto ec = json::read<opts>(outObject, strContent);

  if (ec) {
    std::string_view prefixJsonContent = strContent.substr(0, std::min<int>(strContent.size(), 20));
    throw exception("Error while reading json content '{}{}': {}", prefixJsonContent,
                    prefixJsonContent.size() < strContent.size() ? "..." : "", json::format_error(ec, strContent));
  }
}

template <class T, json::opts opts = kJsonOptions>
T ReadJsonOrThrow(std::string_view strContent) {
  T outObject;
  ReadJsonOrThrow<opts>(strContent, outObject);
  return outObject;
}

template <class T, json::opts opts = kJsonOptions>
T ReadJsonOrThrow(const Reader &reader) {
  return ReadJsonOrThrow<T, opts>(reader.readAll());
}

template <class T, json::opts opts = kJsonOptions>
T ReadJsonOrCreateFile(const File &file) {
  T outObject;
  if (file.exists()) {
    ReadJsonOrThrow<opts>(file.readAll(), outObject);
  } else {
    file.write(WriteJsonOrThrow<true>(outObject));
  }
  return outObject;
}

}  // namespace cct