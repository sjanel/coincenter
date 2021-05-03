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
  if (logLevel.empty()) {
    log::set_level(log::level::info);
  } else {
    log::set_level(log::level::from_str(logLevel));
  }
}

void CoincenterCmdLineOptions::setLogFile() const {
  if (logFile) {
    constexpr int max_size = 1048576 * 5;
    constexpr int max_files = 10;
    log::set_default_logger(log::rotating_logger_st("main", "log/log.txt", max_size, max_files));
  }
}

}  // namespace cct
