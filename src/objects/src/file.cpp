#include "file.hpp"

#include <filesystem>
#include <fstream>
#include <streambuf>

#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {
namespace {
string FullFileName(std::string_view dataDir, std::string_view fileName, File::Type fileType) {
  string fullFilePath(dataDir);
  switch (fileType) {
    case File::Type::kCache:
      fullFilePath.append("/cache/");
      break;
    case File::Type::kLog:
      fullFilePath.append("/log/");
      break;
    case File::Type::kSecret:
      fullFilePath.append("/secret/");
      break;
    case File::Type::kStatic:
      fullFilePath.append("/static/");
      break;
  }
  fullFilePath.append(fileName);
  return fullFilePath;
}
}  // namespace

File::File(std::string_view dataDir, Type type, std::string_view name, IfError ifError)
    : _filePath(FullFileName(dataDir, name, type)), _ifError(ifError) {}

string File::readAll() const {
  log::debug("Opening file {} for reading", _filePath);
  string data;
  if (_ifError == IfError::kThrow || std::filesystem::exists(_filePath.c_str())) {
    std::ifstream file(_filePath.c_str());
    if (!file) {
      throw exception("Unable to open {} for reading", _filePath);
    }
    try {
      data = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    } catch (const std::exception& e) {
      if (_ifError == IfError::kThrow) {
        throw e;
      }
      log::error("Error while reading file {}: {}", _filePath, e.what());
    }
  }
  return data;
}

int File::write(const json& data, Writer::Mode mode) const {
  log::debug("Opening file {} for writing", _filePath);
  auto openMode = mode == Writer::Mode::FromStart ? std::ios_base::out : std::ios_base::app;
  std::ofstream fileOfStream(_filePath.c_str(), openMode);
  if (!fileOfStream) {
    string err("Unable to open ");
    err.append(_filePath).append(" for writing");
    if (_ifError == IfError::kThrow) {
      throw exception(std::move(err));
    }
    log::error(err);
    return 0;
  }
  try {
    if (data.empty()) {
      static constexpr std::string_view kEmptyJsonStr = "{}";
      fileOfStream << kEmptyJsonStr << std::endl;
      return kEmptyJsonStr.length() + 1;
    }
    const int indent = mode == Writer::Mode::FromStart ? 2 : -1;
    string outStr = data.dump(indent);
    fileOfStream << outStr << std::endl;
    return outStr.length() + 1;
  } catch (const std::exception& e) {
    if (_ifError == IfError::kThrow) {
      throw e;
    }
    log::error("Error while writing file {}: {}", _filePath, e.what());
  }
  return 0;
}

}  // namespace cct