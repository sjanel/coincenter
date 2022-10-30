#pragma once

#ifdef CCT_DISABLE_SPDLOG
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
#else
template <typename... Args>
using format_string = spdlog::format_string_t<Args...>;
using fmt::format_to;
#endif

}  // namespace cct