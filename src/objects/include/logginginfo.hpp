#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json.hpp"
#include "cct_log.hpp"

namespace cct {

/// @brief Singleton encapsulating loggers lifetime and set-up.
class LoggingInfo {
 public:
  static constexpr int64_t kDefaultFileSizeInBytes = 5 * 1024 * 1024;
  static constexpr int32_t kDefaultNbMaxFiles = 10;
  static constexpr char const *const kOutputLoggerName = "output";

  /// Creates a default logging info, with level 'info' on standard output.
  LoggingInfo() { createLoggers(); }

  /// Creates a logging info from general config json file.
  explicit LoggingInfo(const json &generalConfigJsonLogPart);

  LoggingInfo(const LoggingInfo &) = delete;
  LoggingInfo(LoggingInfo &&loggingInfo) noexcept;
  LoggingInfo &operator=(const LoggingInfo &) = delete;
  LoggingInfo &operator=(LoggingInfo &&loggingInfo) noexcept;

  ~LoggingInfo();

  int64_t maxFileSizeInBytes() const { return _maxFileSizeInBytes; }

  int32_t maxNbFiles() const { return _maxNbFiles; }

  log::level::level_enum logConsole() const { return LevelFromPos(_logLevelConsolePos); }
  log::level::level_enum logFile() const { return LevelFromPos(_logLevelFilePos); }

 private:
  void createLoggers() const;

  static void CreateOutputLogger();

  static constexpr int8_t PosFromLevel(log::level::level_enum level) {
    return static_cast<int8_t>(log::level::off) - static_cast<int8_t>(level);
  }
  static constexpr log::level::level_enum LevelFromPos(int8_t levelPos) {
    return static_cast<log::level::level_enum>(static_cast<int8_t>(log::level::off) - levelPos);
  }

  int64_t _maxFileSizeInBytes = kDefaultFileSizeInBytes;
  int32_t _maxNbFiles = kDefaultNbMaxFiles;
  int8_t _logLevelConsolePos = PosFromLevel(log::level::info);
  int8_t _logLevelFilePos = PosFromLevel(log::level::off);
  bool _destroyLoggers = true;
};

}  // namespace cct