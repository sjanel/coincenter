#pragma once

#include <algorithm>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "file.hpp"
#include "reader.hpp"
#include "write-json.hpp"

namespace cct {

// {raw_string, error_on_const_read} are opts_ex (derived) members; base members
// (error_on_unknown_keys...) go in the nested brace.
inline constexpr auto kExactJsonOptions =
    json::opts_ex{{.error_on_unknown_keys = true}, /*raw_string*/ true, /*error_on_const_read*/ true};

inline constexpr auto kPartialJsonOptions =
    json::opts_ex{{.error_on_unknown_keys = false}, /*raw_string*/ true, /*error_on_const_read*/ true};

template <auto opts>
json::error_ctx ReadJson(std::string_view strContent, std::string_view serviceName, auto& outObject) {
  if (strContent.empty()) {
    return json::error_ctx{};
  }

  auto ec = json::read<opts>(outObject, strContent);

  if (ec) {
    std::string_view prefixJsonContent = strContent.substr(0, std::min<int>(strContent.size(), 20));
    log::error("Error while reading {} json content '{}{}': {}", serviceName, prefixJsonContent,
               prefixJsonContent.size() < strContent.size() ? "..." : "", json::format_error(ec, strContent));
  }

  return ec;
}

/**
 * Read json content from a string ignoring unknown keys
 */
json::error_ctx ReadPartialJson(std::string_view strContent, std::string_view serviceName, auto& outObject) {
  return ReadJson<kPartialJsonOptions>(strContent, serviceName, outObject);
}

template <auto opts>
void ReadJsonOrThrow(std::string_view strContent, auto& outObject) {
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

/**
 * Read json content from a string raising an error for unknown keys
 */
void ReadExactJsonOrThrow(std::string_view strContent, auto& outObject) {
  ReadJsonOrThrow<kExactJsonOptions>(strContent, outObject);
}

template <class T, auto opts = kExactJsonOptions>
T ReadJsonOrThrow(std::string_view strContent) {
  T outObject;
  ReadJsonOrThrow<opts>(strContent, outObject);
  return outObject;
}

template <class T, auto opts = kExactJsonOptions>
T ReadJsonOrThrow(const Reader& reader) {
  return ReadJsonOrThrow<T, opts>(reader.readAll());
}

template <class T, auto opts = kExactJsonOptions>
T ReadJsonOrCreateFile(const File& file) {
  T outObject;
  if (file.exists()) {
    ReadJsonOrThrow<opts>(file.readAll(), outObject);
  } else {
    file.write(WritePrettyJsonOrThrow(outObject));
  }
  return outObject;
}

}  // namespace cct