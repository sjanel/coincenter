#include "parseloglevel.hpp"

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "cct_exception.hpp"
#include "logginginfo.hpp"
#include "static_string_view_helpers.hpp"

namespace cct {
int8_t LogPosFromLogStr(std::string_view logStr) {
  if (logStr.size() == 1) {
    const int8_t logLevelPos = logStr.front() - '0';
    if (logLevelPos < 0 ||
        logLevelPos >= static_cast<std::remove_const_t<decltype(logLevelPos)>>(LoggingInfo::kNbLogLevels)) {
      throw exception("Unrecognized log level {}. Possible values are [0-{}]", logStr,
                      '0' + (LoggingInfo::kNbLogLevels - 1U));
    }
    return logLevelPos;
  }

  int8_t logLevel = 0;
  for (const auto logName : LoggingInfo::kLogLevelNames) {
    if (logStr == logName) {
      return logLevel;
    }
    ++logLevel;
  }

  static constexpr std::string_view kPrintLogNameSep = "|";
  throw exception("Unrecognized log level name {}. Possible values are {}", logStr,
                  make_joined_string_view<kPrintLogNameSep, LoggingInfo::kLogLevelNames>::value);
}
}  // namespace cct