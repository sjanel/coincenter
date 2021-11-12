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
  static constexpr int trace{};
};

}  // namespace log
#else
namespace log = spdlog;

#endif
}  // namespace cct