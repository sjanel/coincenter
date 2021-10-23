#include "cct_file.hpp"

#include <filesystem>
#include <fstream>
#include <streambuf>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {
namespace {
string FullFileName(std::string_view fileName, File::Type fileType) {
  string fullFilePath(kDataDir);
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

string File::read() const {
  string filePath = FullFileName(_name, _type);
  log::debug("Opening file {} for reading", filePath);
  string data;
  if (_ifNotFound == IfNotFound::kThrow || std::filesystem::exists(filePath)) {
    std::ifstream file(filePath);
    if (!file) {
      throw exception("Unable to open " + filePath + " for reading");
    }
    data = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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

void File::write(const json &data) const {
  string filePath = FullFileName(_name, _type);
  log::debug("Opening file {} for writing", filePath);
  std::ofstream file(filePath);
  if (!file) {
    throw exception("Unable to open " + filePath + " for writing");
  }
  file << data.dump(2);
}

}  // namespace cct