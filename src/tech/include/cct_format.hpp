#pragma once

#ifdef CCT_DISABLE_SPDLOG
#include <string_view>
#else
#include <spdlog/fmt/bundled/format.h>
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
using fmt::format_string;
using fmt::format_to;
#endif

}  // namespace cct