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
  LoggingInfo() noexcept = default;

  /// Creates a logging info in standard output
  /// @param logLevelPos from 0 (no log at all) to 6 (most verbose level of logs)
  explicit LoggingInfo(int8_t logLevelPos);

  /// Creates a logging info from general config json file.
  explicit LoggingInfo(const json &generalConfigJsonLogPart);

  /// Creates a logging info with full information, from a position of log level
  /// @param logLevelPos from 0 (no log at all) to 6 (most verbose level of logs)
  LoggingInfo(int8_t logLevelPos, int maxNbFiles, int64_t maxFileSizeInBytes, bool logFile);

  /// Creates a logging info with full information, from a position of log level
  /// @param logStr in (off|critical|error|warning|info|debug|trace) or 0-6
  LoggingInfo(std::string_view logStr, int maxNbFiles, int64_t maxFileSizeInBytes, bool logFile);

  int64_t maxFileSizeInBytes() const { return _maxFileSizeInBytes; }

  int maxNbFiles() const { return _maxNbFiles; }

  log::level::level_enum logLevel() const {
    return static_cast<log::level::level_enum>(static_cast<int>(log::level::off) - _logLevelPos);
  }

  bool logFile() const { return _logFile; }

 private:
  void activateLogOptions() const;

  int64_t _maxFileSizeInBytes = kDefaultFileSizeInBytes;
  int _maxNbFiles = kDefaultNbMaxFiles;
  int8_t _logLevelPos = static_cast<int8_t>(log::level::off) - static_cast<int8_t>(log::level::info);
  bool _logFile = false;
};

}  // namespace cct