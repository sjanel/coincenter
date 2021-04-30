#include "jsonhelpers.hpp"

#include <filesystem>
#include <fstream>
#include <streambuf>

#include "cct_const.hpp"
#include "cct_exception.hpp"

namespace cct {
namespace {
std::string FullJsonFilePath(std::string_view filenameNoExtension) {
  std::string filePath = std::string(kDataPath);
  filePath.push_back('/');
  filePath.append(filenameNoExtension);
  filePath.append(".json");
  return filePath;
}
}  // namespace

/// Open, read and return a parsed json object from file name "<filenameNoExtension>.json" file.
/// @param filenameNoExtension base name file without extension ('.json' extension expected)
///                            File should be contained in the data directory.
/// @param fileNotFoundMode kThrow: file is expected to be always present, exception will be thrown if it is not,
///                         kNoThrow: file may not be present, in this case an empty json will be returned
json OpenJsonFile(std::string_view filenameNoExtension, FileNotFoundMode fileNotFoundMode) {
  std::string filePath = FullJsonFilePath(filenameNoExtension);
  json ret;
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
void WriteJsonFile(std::string_view filenameNoExtension, const json &data) {
  std::string filePath = FullJsonFilePath(filenameNoExtension);
  std::ofstream file(filePath);
  if (!file) {
    throw exception("Unable to open " + filePath + " for writing");
  }
  constexpr int kNbDefaultJsonIndentSpaces = 2;
  file << data.dump(kNbDefaultJsonIndentSpaces);
}

}  // namespace cct