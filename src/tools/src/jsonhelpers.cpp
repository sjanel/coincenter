#include "jsonhelpers.hpp"

#include <filesystem>
#include <fstream>
#include <streambuf>
#include <string>

#include "cct_const.hpp"
#include "cct_exception.hpp"

namespace cct {
namespace {
std::string FullFileName(std::string_view fileName, FileType fileType) {
  std::string fullFilePath(fileType == FileType::kData ? kDataPath : kConfigPath);
  fullFilePath.push_back('/');
  fullFilePath.append(fileName);
  return fullFilePath;
}
}  // namespace

/// Open, read and return a parsed json object from file name "<filenameNoExtension>.json" file.
/// @param fileName File name of the json file to open
/// @param fileNotFoundMode kThrow: file is expected to be always present, exception will be thrown if it is not,
///                         kNoThrow: file may not be present, in this case an empty json will be returned
json OpenJsonFile(std::string_view fileName, FileNotFoundMode fileNotFoundMode, FileType fileType) {
  json ret;
  std::string filePath = FullFileName(fileName, fileType);
  if (fileNotFoundMode == FileNotFoundMode::kThrow || std::filesystem::exists(filePath)) {
    std::ifstream file(filePath);
    if (!file) {
      throw exception("Unable to open " + filePath + " for reading");
    }
    ret = json::parse(std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
  }
  return ret;
}

/// Writes json into file
void WriteJsonFile(std::string_view fileName, const json &data, FileType fileType) {
  std::string filePath = FullFileName(fileName, fileType);
  std::ofstream file(filePath);
  if (!file) {
    throw exception("Unable to open " + filePath + " for writing");
  }
  constexpr int kNbDefaultJsonIndentSpaces = 2;
  file << data.dump(kNbDefaultJsonIndentSpaces);
}

}  // namespace cct