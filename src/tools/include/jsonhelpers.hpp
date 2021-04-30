#pragma once

#include <string_view>

#include "cct_json.hpp"

namespace cct {

enum class FileNotFoundMode { kThrow, kNoThrow };

/// Open, read and return a parsed json object from file name "<filenameNoExtension>.json" file.
json OpenJsonFile(std::string_view filenameNoExtension, FileNotFoundMode fileNotFoundMode);

/// Writes json into file
void WriteJsonFile(std::string_view filenameNoExtension, const json &data);
}  // namespace cct