#pragma once

#include <string_view>

#include "cct_json.hpp"

namespace cct {

enum class FileNotFoundMode { kThrow, kNoThrow };

enum class FileType { kConfig, kData };

/// Open, read and return a parsed json object from file.
json OpenJsonFile(std::string_view fileName, FileNotFoundMode fileNotFoundMode, FileType fileType);

/// Writes json into file
void WriteJsonFile(std::string_view fileName, const json &data, FileType FileType);
}  // namespace cct