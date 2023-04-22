#include "logginginfo.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <utility>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#ifndef CCT_MSVC
#include "static_string_view_helpers.hpp"
#endif
#include "unitsparser.hpp"

namespace cct {

namespace {
constexpr int8_t kMaxLogLevel = 6;

int8_t LogPosFromLogStr(std::string_view logStr) {
  if (logStr.size() == 1) {
    int8_t logLevelPos = logStr.front() - '0';
    if (logLevelPos < 0 || logLevelPos > kMaxLogLevel) {
      throw exception("Unrecognized log level {}. Possible values are 0-{}", logStr, '0' + kMaxLogLevel);
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
  throw exception("Unrecognized log level name {}. Possible values are off|critical|error|warning|info|debug|trace",
                  logStr);
}

log::filename_t GetLogFilename() {
#ifdef CCT_MSVC
  return log::filename_t(kDefaultDataDir) + "/log/log.txt";
#else
  // Internal error in MSVC for below code...
  static constexpr std::string_view kLogDir2 = "/log/log.txt";
  static constexpr std::string_view kLogDir = JoinStringView_v<kDefaultDataDir, kLogDir2>;
  return log::filename_t(kLogDir);
#endif
}

}  // namespace

LoggingInfo::LoggingInfo(const json &generalConfigJsonLogPart)
    : _maxFileSizeInBytes(ParseNumberOfBytes(generalConfigJsonLogPart["maxFileSize"].get<std::string_view>())),
      _maxNbFiles(generalConfigJsonLogPart["maxNbFiles"].get<int>()),
      _logLevelConsolePos(LogPosFromLogStr(generalConfigJsonLogPart["console"].get<std::string_view>())),
      _logLevelFilePos(LogPosFromLogStr(generalConfigJsonLogPart["file"].get<std::string_view>())) {
  createLoggers();
}

LoggingInfo::LoggingInfo(LoggingInfo &&loggingInfo) noexcept
    : _maxFileSizeInBytes(loggingInfo._maxFileSizeInBytes),
      _maxNbFiles(loggingInfo._maxNbFiles),
      _logLevelConsolePos(loggingInfo._logLevelConsolePos),
      _logLevelFilePos(loggingInfo._logLevelFilePos),
      _destroyLoggers(std::exchange(loggingInfo._destroyLoggers, false)) {}

LoggingInfo::~LoggingInfo() {
  if (_destroyLoggers) {
    log::drop(kOutputLoggerName);
  }
}

void LoggingInfo::createLoggers() const {
  FixedCapacityVector<log::sink_ptr, 2> sinks;

  if (_logLevelConsolePos != 0) {
    auto &consoleSink = sinks.emplace_back(std::make_shared<log::sinks::stderr_color_sink_mt>());
    consoleSink->set_level(LevelFromPos(_logLevelConsolePos));
  }

  if (_logLevelFilePos != 0) {
    auto &rotatingSink = sinks.emplace_back(
        std::make_shared<log::sinks::rotating_file_sink_mt>(GetLogFilename(), _maxFileSizeInBytes, _maxNbFiles));
    rotatingSink->set_level(LevelFromPos(_logLevelFilePos));
  }

  constexpr int nbThreads = 1;  // only one logger thread is important to keep order between output logger and others
  log::init_thread_pool(8192, nbThreads);
  auto logger = std::make_shared<log::async_logger>("", sinks.begin(), sinks.end(), log::thread_pool(),
                                                    log::async_overflow_policy::block);

  // spdlog level is present in sink context, and also logger context (why?)
  // in addition of the levels of each sink, we need to set the main level of the logger based on the max log level of
  // both
  logger->set_level(LevelFromPos(std::max(_logLevelConsolePos, _logLevelFilePos)));

  log::set_default_logger(logger);

  CreateOutputLogger();
}

void LoggingInfo::CreateOutputLogger() {
  auto outputLogger =
      std::make_shared<log::async_logger>(kOutputLoggerName, std::make_shared<log::sinks::stdout_color_sink_mt>(),
                                          log::thread_pool(), log::async_overflow_policy::block);
  outputLogger->set_level(log::level::level_enum::info);
  outputLogger->set_pattern("%v");
  log::register_logger(outputLogger);
}

}  // namespace cct