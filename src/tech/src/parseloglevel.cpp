#include "parseloglevel.hpp"

#include "cct_exception.hpp"

namespace cct {
int8_t LogPosFromLogStr(std::string_view logStr) {
  if (logStr.size() == 1) {
    static constexpr int8_t kMaxLogLevel = 6;
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
}  // namespace cct