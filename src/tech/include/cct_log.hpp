#pragma once

#ifdef CCT_DISABLE_SPDLOG
#include <string_view>
#else
#include <spdlog/spdlog.h>
#endif

namespace cct {
#ifdef CCT_DISABLE_SPDLOG
namespace log {
inline void critical(std::string_view, ...) {}
inline void error(std::string_view, ...) {}
inline void warn(std::string_view, ...) {}
inline void info(std::string_view, ...) {}
inline void debug(std::string_view, ...) {}
inline void trace(std::string_view, ...) {}

inline int get_level() { return 0; }

struct level {
  using level_enum = int;
  static constexpr int trace = 6;
  static constexpr int debug = 5;
  static constexpr int info = 4;
  static constexpr int warn = 3;
  static constexpr int error = 2;
  static constexpr int critical = 1;
  static constexpr int off = 0;
};

}  // namespace log
#else
namespace log = spdlog;

#endif
}  // namespace cct