#pragma once

#ifdef CCT_DISABLE_SPDLOG
#include <string>
#include <string_view>
#else
#include <spdlog/spdlog.h>
#endif

namespace cct {

#ifdef CCT_DISABLE_SPDLOG

template <typename... Args>
using format_string = std::string_view;

template <typename OutputIt, typename... T>
auto format_to(OutputIt out, format_string<T...>, T&&...) -> OutputIt {
  return out;
}

template <typename... T>
auto format(format_string<T...> fmt, [[maybe_unused]] T&&... args) -> std::string {
  return std::string{fmt};
}

#else
template <typename... Args>
using format_string = spdlog::format_string_t<Args...>;
using fmt::format;
using fmt::format_to;
#endif

}  // namespace cct
