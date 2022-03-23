#include "generalconfig.hpp"

#include "cct_file.hpp"

namespace cct {

GeneralConfig::GeneralConfig(const LoggingInfo &loggingInfo, Duration fiatConversionQueryRate, bool printResults)
    : _loggingInfo(loggingInfo), _fiatConversionQueryRate(fiatConversionQueryRate), _printResults(printResults) {}

GeneralConfig::GeneralConfig(LoggingInfo &&loggingInfo, Duration fiatConversionQueryRate, bool printResults)
    : _loggingInfo(std::move(loggingInfo)),
      _fiatConversionQueryRate(fiatConversionQueryRate),
      _printResults(printResults) {}

json GeneralConfig::LoadFile(std::string_view dataDir) {
  File generalConfigFile(dataDir, File::Type::kStatic, GeneralConfig::kFilename, File::IfNotFound::kNoThrow);
  static const json kDefaultGeneralConfig = R"(
{
  "log": {
    "level": "info",
    "file": false,
    "maxNbFiles": 10,
    "maxFileSize": "5Mi"
  },
  "fiatConversion": {
    "rate": "8h"
  },
  "printResults": true
}
)"_json;
  json jsonData = kDefaultGeneralConfig;
  json generalConfigJsonData = generalConfigFile.readJson();
  if (generalConfigJsonData.empty()) {
    // Create a file with default values. User can then update them as he wishes.
    log::warn("No {} file found. Creating a default one which can be updated freely at your convenience",
              GeneralConfig::kFilename);
    generalConfigFile.write(jsonData);
  } else {
    jsonData.update(generalConfigJsonData);
    if (jsonData != generalConfigJsonData) {
      log::warn("File {} updated with default values of all supported options", GeneralConfig::kFilename);
      generalConfigFile.write(jsonData);
    }
  }
  return jsonData;
}

}  // namespace cct