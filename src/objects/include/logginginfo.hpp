#pragma once

#include <cstdint>
#include <string_view>

#include "cct_const.hpp"
#include "cct_flatset.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincentercommandtype.hpp"
#include "file.hpp"

namespace cct {

/// @brief Encapsulates loggers lifetime and set-up.
class LoggingInfo {
 public:
  static constexpr int64_t kDefaultFileSizeInBytes = 5L * 1024 * 1024;
  static constexpr int32_t kDefaultNbMaxFiles = 10;
  static constexpr char const *const kOutputLoggerName = "output";

  enum class WithLoggersCreation : int8_t { kNo, kYes };

  /// Creates a default logging info, with level 'info' on standard output.
  explicit LoggingInfo(WithLoggersCreation withLoggersCreation, std::string_view dataDir = kDefaultDataDir);

  /// Creates a logging info from general config json file.
  LoggingInfo(WithLoggersCreation withLoggersCreation, std::string_view dataDir, const json &generalConfigJsonLogPart);

  LoggingInfo(const LoggingInfo &) = delete;
  LoggingInfo(LoggingInfo &&loggingInfo) noexcept;
  LoggingInfo &operator=(const LoggingInfo &) = delete;
  LoggingInfo &operator=(LoggingInfo &&loggingInfo) = delete;

  ~LoggingInfo();

  int64_t maxFileSizeLogFileInBytes() const { return _maxFileSizeLogFileInBytes; }

  int32_t maxNbLogFiles() const { return _maxNbLogFiles; }

  log::level::level_enum logConsole() const { return LevelFromPos(_logLevelConsolePos); }
  log::level::level_enum logFile() const { return LevelFromPos(_logLevelFilePos); }

  bool isCommandTypeTracked(CoincenterCommandType cmd) const { return _trackedCommandTypes.contains(cmd); }

  File getActivityFile() const;

 private:
  void createLoggers();

  static void CreateOutputLogger();

  static constexpr int8_t PosFromLevel(log::level::level_enum level) {
    return static_cast<int8_t>(log::level::off) - static_cast<int8_t>(level);
  }
  static constexpr log::level::level_enum LevelFromPos(int8_t levelPos) {
    return static_cast<log::level::level_enum>(static_cast<int8_t>(log::level::off) - levelPos);
  }

  using TrackedCommandTypes = FlatSet<CoincenterCommandType>;

  std::string_view _dataDir = kDefaultDataDir;
  string _dateFormatStrActivityFiles;
  TrackedCommandTypes _trackedCommandTypes;
  int64_t _maxFileSizeLogFileInBytes = kDefaultFileSizeInBytes;
  int32_t _maxNbLogFiles = kDefaultNbMaxFiles;
  int8_t _logLevelConsolePos = PosFromLevel(log::level::info);
  int8_t _logLevelFilePos = PosFromLevel(log::level::off);
  bool _destroyLoggers = false;
};

}  // namespace cct