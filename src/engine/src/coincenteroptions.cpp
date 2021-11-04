#include "coincenteroptions.hpp"

#include <spdlog/sinks/rotating_file_sink.h>

#include <iostream>

#include "cct_const.hpp"
#include "cct_log.hpp"
#include "stringoptionparser.hpp"

namespace cct {

void CoincenterCmdLineOptions::PrintVersion(const char* programName) {
  std::cout << programName << " version " << kVersion << std::endl;
  std::cout << "compiled with " << CCT_COMPILER_VERSION << " on " << __DATE__ << " at " << __TIME__ << std::endl;
}

void CoincenterCmdLineOptions::setLogLevel() const {
  switch (logLevel.size()) {
    case 0:
      break;
    case 1: {
      const int levelInt = static_cast<int>(log::level::off) - (logLevel.front() - '0');
      log::set_level(static_cast<log::level::level_enum>(levelInt));
      break;
    }
    default:
      log::set_level(log::level::from_str(logLevel));
      break;
  }
}

void CoincenterCmdLineOptions::setLogFile() const {
  if (logFile) {
    constexpr int kMaxFileSize = 5 * 1024 * 1024;
    constexpr int kMaxNbFiles = 10;
    log::set_default_logger(log::rotating_logger_st("main", "log/log.txt", kMaxFileSize, kMaxNbFiles));
  }
}

}  // namespace cct
