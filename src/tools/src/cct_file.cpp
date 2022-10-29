#include "cct_file.hpp"

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

string File::read() const {
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
      } else {
        log::error("Error while reading file {}: {}", _filePath, e.what());
      }
    }
  }
  return data;
}

json File::readJson() const {
  string dataS = read();
  if (dataS.empty()) {
    dataS = "{}";
  }
  return json::parse(std::move(dataS));
}

void File::write(const json& data) const {
  log::debug("Opening file {} for writing", _filePath);
  std::ofstream file(_filePath.c_str());
  if (!file) {
    string err("Unable to open ");
    err.append(_filePath).append(" for writing");
    if (_ifError == IfError::kThrow) {
      throw exception(std::move(err));
    }
    log::error(err);
    return;
  }
  try {
    if (data.empty()) {
      file << "{}" << std::endl;
    } else {
      file << data.dump(2) << std::endl;
    }
  } catch (const std::exception& e) {
    if (_ifError == IfError::kThrow) {
      throw e;
    } else {
      log::error("Error while writing file {}: {}", _filePath, e.what());
    }
  }
}

}  // namespace cct