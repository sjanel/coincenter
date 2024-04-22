#include "logginginfo.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string_view>
#include <utility>

#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincentercommandtype.hpp"
#include "file.hpp"
#include "parseloglevel.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "unitsparser.hpp"

namespace cct {

LoggingInfo::LoggingInfo(WithLoggersCreation withLoggersCreation, std::string_view dataDir) : _dataDir(dataDir) {
  if (withLoggersCreation == WithLoggersCreation::kYes) {
    createLoggers();
  }
}

LoggingInfo::LoggingInfo(WithLoggersCreation withLoggersCreation, std::string_view dataDir,
                         const json &generalConfigJsonLogPart)
    : _dataDir(dataDir),
      _maxFileSizeLogFileInBytes(ParseNumberOfBytes(generalConfigJsonLogPart["maxFileSize"].get<std::string_view>())),
      _maxNbLogFiles(generalConfigJsonLogPart["maxNbFiles"].get<int>()),
      _logLevelConsolePos(
          LogPosFromLogStr(generalConfigJsonLogPart[LoggingInfo::kJsonFieldConsoleLevelName].get<std::string_view>())),
      _logLevelFilePos(
          LogPosFromLogStr(generalConfigJsonLogPart[LoggingInfo::kJsonFieldFileLevelName].get<std::string_view>())) {
  if (withLoggersCreation == WithLoggersCreation::kYes) {
    createLoggers();
  }

  const json &activityTrackingPart = generalConfigJsonLogPart["activityTracking"];
  const json &commandTypes = activityTrackingPart["commandTypes"];

  _trackedCommandTypes.reserve(static_cast<decltype(_trackedCommandTypes)::size_type>(commandTypes.size()));
  std::ranges::transform(
      commandTypes, std::inserter(_trackedCommandTypes, _trackedCommandTypes.end()),
      [](const json &elem) { return CoincenterCommandTypeFromString(elem.get<std::string_view>()); });

  _dateFormatStrActivityFiles = activityTrackingPart["dateFileNameFormat"];
  _alsoLogActivityForSimulatedCommands = activityTrackingPart["withSimulatedCommands"].get<bool>();
}

LoggingInfo::LoggingInfo(LoggingInfo &&loggingInfo) noexcept
    : _dataDir(loggingInfo._dataDir),
      _dateFormatStrActivityFiles(std::move(loggingInfo._dateFormatStrActivityFiles)),
      _trackedCommandTypes(std::move(loggingInfo._trackedCommandTypes)),
      _maxFileSizeLogFileInBytes(loggingInfo._maxFileSizeLogFileInBytes),
      _maxNbLogFiles(loggingInfo._maxNbLogFiles),
      _logLevelConsolePos(loggingInfo._logLevelConsolePos),
      _logLevelFilePos(loggingInfo._logLevelFilePos),
      _destroyLoggers(std::exchange(loggingInfo._destroyLoggers, false)),
      _alsoLogActivityForSimulatedCommands(loggingInfo._alsoLogActivityForSimulatedCommands) {}

LoggingInfo::~LoggingInfo() {
  if (_destroyLoggers) {
    log::drop(kOutputLoggerName);
  }
}

File LoggingInfo::getActivityFile() const {
  string activityFileName("activity_history_");
  activityFileName.append(ToString(Clock::now(), _dateFormatStrActivityFiles.data()));
  activityFileName.append(".txt");
  return {_dataDir, File::Type::kLog, activityFileName, File::IfError::kThrow};
}

void LoggingInfo::createLoggers() {
  FixedCapacityVector<log::sink_ptr, 2> sinks;

  if (_logLevelConsolePos != 0) {
    auto &consoleSink = sinks.emplace_back(std::make_shared<log::sinks::stderr_color_sink_mt>());
    consoleSink->set_level(LevelFromPos(_logLevelConsolePos));
  }

  if (_logLevelFilePos != 0) {
    auto logFileName = log::filename_t(_dataDir) + "/log/log.txt";
    auto &rotatingSink = sinks.emplace_back(
        std::make_shared<log::sinks::rotating_file_sink_mt>(logFileName, _maxFileSizeLogFileInBytes, _maxNbLogFiles));
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

  _destroyLoggers = true;
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