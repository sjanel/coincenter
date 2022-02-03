#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {
/// Parse given string representation of a duration and return the duration.
/// Amounts and units may be separated by spaces. For example:
/// "1h45min" is allowed, as well as "1h 45min" and "1 h 45      min "
Duration ParseDuration(std::string_view durationStr);

/// Create a string representation of given duration.
/// No spaces are inserted between a couple of units. For example:
/// "1y6mon" instead of "1y 6mon" will be returne
string DurationToString(Duration d);

}  // namespace cct