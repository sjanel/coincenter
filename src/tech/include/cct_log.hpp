#pragma once

#include <cstdint>

#ifdef CCT_DISABLE_SPDLOG
#include <string_view>
#else
#include <spdlog/common.h>  // IWYU pragma: export
#include <spdlog/spdlog.h>  // IWYU pragma: export
#endif

namespace cct {
#ifdef CCT_DISABLE_SPDLOG
namespace log {
constexpr void critical(std::string_view, ...) {}
constexpr void error(std::string_view, ...) {}
constexpr void warn(std::string_view, ...) {}
constexpr void info(std::string_view, ...) {}
constexpr void debug(std::string_view, ...) {}
constexpr void trace(std::string_view, ...) {}

struct level {
  using level_enum = int;
  static constexpr int trace = 0;
  static constexpr int debug = 1;
  static constexpr int info = 2;
  static constexpr int warn = 3;
  static constexpr int error = 4;
  static constexpr int critical = 5;
  static constexpr int off = 6;
};

constexpr int get_level() { return static_cast<int>(level::off); }

}  // namespace log
#else
namespace log = spdlog;

#endif

constexpr int8_t PosFromLevel(log::level::level_enum level) {
  return static_cast<int8_t>(log::level::off) - static_cast<int8_t>(level);
}
constexpr log::level::level_enum LevelFromPos(int8_t levelPos) {
  return static_cast<log::level::level_enum>(static_cast<int8_t>(log::level::off) - levelPos);
}
}  // namespace cct