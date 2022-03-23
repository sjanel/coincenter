#include "logginginfo.hpp"

#include <spdlog/sinks/rotating_file_sink.h>

#include "cct_exception.hpp"
#include "unitsparser.hpp"

namespace cct {

namespace {
static constexpr int8_t kMaxLogLevel = 6;

int8_t LogPosFromLogStr(std::string_view logStr) {
  if (logStr.size() == 1) {
    int8_t logLevelPos = logStr.front() - '0';
    if (logLevelPos < 0 || logLevelPos > kMaxLogLevel) {
      string err("Unrecognized log level ");
      err.append(logStr).append(". Possible values are 0-");
      err.push_back('0' + kMaxLogLevel);
      throw exception(std::move(err));
    }
    return logLevelPos;
  }
  if (logStr == "off") {
    return 0;
  }
  if (logStr == "critical") {
    return 1;
  }
  if (logStr == "error") {
    return 2;
  }
  if (logStr == "warning") {
    return 3;
  }
  if (logStr == "info") {
    return 4;
  }
  if (logStr == "debug") {
    return 5;
  }
  if (logStr == "trace") {
    return 6;
  }
  string err("Unrecognized log level name ");
  err.append(logStr).append(". Possible values are off|critical|error|warning|info|debug|trace");
  throw exception(std::move(err));
}

}  // namespace

LoggingInfo::LoggingInfo(int8_t logLevelPos) : _logLevelPos(logLevelPos) { activateLogOptions(); }

LoggingInfo::LoggingInfo(const json &generalConfigJsonLogPart)
    : _maxFileSizeInBytes(ParseNumberOfBytes(generalConfigJsonLogPart["maxFileSize"].get<std::string_view>())),
      _maxNbFiles(generalConfigJsonLogPart["maxNbFiles"].get<int>()),
      _logLevelPos(LogPosFromLogStr(generalConfigJsonLogPart["level"].get<std::string_view>())),
      _logFile(generalConfigJsonLogPart["file"].get<bool>()) {
  activateLogOptions();
}

LoggingInfo::LoggingInfo(int8_t logLevelPos, int maxNbFiles, int64_t maxFileSizeInBytes, bool logFile)
    : _maxFileSizeInBytes(maxFileSizeInBytes), _maxNbFiles(maxNbFiles), _logLevelPos(logLevelPos), _logFile(logFile) {
  activateLogOptions();
}

LoggingInfo::LoggingInfo(std::string_view logStr, int maxNbFiles, int64_t maxFileSizeInBytes, bool logFile = true)
    : _maxFileSizeInBytes(maxFileSizeInBytes),
      _maxNbFiles(maxNbFiles),
      _logLevelPos(LogPosFromLogStr(logStr)),
      _logFile(logFile) {
  activateLogOptions();
}

void LoggingInfo::activateLogOptions() const {
  const int levelInt = static_cast<int>(log::level::off) - _logLevelPos;
  log::set_level(static_cast<log::level::level_enum>(levelInt));
  if (_logFile) {
    log::set_default_logger(log::rotating_logger_st("main", "log/log.txt", _maxFileSizeInBytes, _maxNbFiles));
  }
}

}  // namespace cct