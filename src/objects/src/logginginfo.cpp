#include "logginginfo.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

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

}  // namespace

LoggingInfo::LoggingInfo(const json &generalConfigJsonLogPart)
    : _maxFileSizeInBytes(ParseNumberOfBytes(generalConfigJsonLogPart["maxFileSize"].get<std::string_view>())),
      _maxNbFiles(generalConfigJsonLogPart["maxNbFiles"].get<int>()),
      _logLevelConsolePos(LogPosFromLogStr(generalConfigJsonLogPart["console"].get<std::string_view>())),
      _logLevelFilePos(LogPosFromLogStr(generalConfigJsonLogPart["file"].get<std::string_view>())) {
  activateLogOptions();
}

void LoggingInfo::activateLogOptions() const {
  FixedCapacityVector<spdlog::sink_ptr, 2> sinks;

  if (_logLevelConsolePos != 0) {
    auto &consoleSink = sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    consoleSink->set_level(LevelFromPos(_logLevelConsolePos));
  }

  if (_logLevelFilePos != 0) {
#ifdef CCT_MSVC
    auto logFile = spdlog::filename_t(kDefaultDataDir) + "/log/log.txt";
#else
    // Internal error in MSVC for below code...
    static constexpr std::string_view kLogDir2 = "/log/log.txt";
    static constexpr std::string_view kLogDir = JoinStringView_v<kDefaultDataDir, kLogDir2>;
    spdlog::filename_t logFile(kLogDir);
#endif
    auto &rotatingSink = sinks.emplace_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(std::move(logFile), _maxFileSizeInBytes, _maxNbFiles));
    rotatingSink->set_level(LevelFromPos(_logLevelFilePos));
  }

  if (sinks.empty()) {
    log::set_level(log::level::level_enum::off);
  } else {
    constexpr int nbThreads = 1;
    spdlog::init_thread_pool(8192, nbThreads);
    auto logger = std::make_shared<spdlog::async_logger>("", sinks.begin(), sinks.end(), spdlog::thread_pool(),
                                                         spdlog::async_overflow_policy::block);

    // spdlog level is present in sink context, and also logger context (why?)
    // in addition of the levels of each sink, we need to set the main level of the logger based on the max log level of
    // both
    logger->set_level(LevelFromPos(std::max(_logLevelConsolePos, _logLevelFilePos)));

    spdlog::set_default_logger(logger);
  }
}

}  // namespace cct