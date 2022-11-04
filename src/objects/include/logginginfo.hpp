#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json.hpp"
#include "cct_log.hpp"

namespace cct {

class LoggingInfo {
 public:
  static constexpr int kDefaultNbMaxFiles = 10;
  static constexpr int64_t kDefaultFileSizeInBytes = 5 * 1024 * 1024;

  /// Creates a default logging info, with level 'info' on standard output.
  constexpr LoggingInfo() noexcept = default;

  /// Creates a logging info from general config json file.
  explicit LoggingInfo(const json &generalConfigJsonLogPart);

  int64_t maxFileSizeInBytes() const { return _maxFileSizeInBytes; }

  int maxNbFiles() const { return _maxNbFiles; }

  log::level::level_enum logConsole() const { return LevelFromPos(_logLevelConsolePos); }
  log::level::level_enum logFile() const { return LevelFromPos(_logLevelFilePos); }

 private:
  void activateLogOptions() const;

  static constexpr int8_t PosFromLevel(log::level::level_enum level) {
    return static_cast<int8_t>(log::level::off) - static_cast<int8_t>(level);
  }
  static constexpr log::level::level_enum LevelFromPos(int8_t levelPos) {
    return static_cast<log::level::level_enum>(static_cast<int8_t>(log::level::off) - levelPos);
  }

  int64_t _maxFileSizeInBytes = kDefaultFileSizeInBytes;
  int _maxNbFiles = kDefaultNbMaxFiles;
  int8_t _logLevelConsolePos = PosFromLevel(log::level::info);
  int8_t _logLevelFilePos = PosFromLevel(log::level::off);
};

}  // namespace cct