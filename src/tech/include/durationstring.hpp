#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

/// Check if 'str' starts with a Duration.
/// Returns the duration string length (0 if no duration detected)
std::string_view::size_type DurationLen(std::string_view str);

/// Parse given string representation of a duration and return the duration.
/// Amounts and units may be separated by spaces. For example:
/// "1h45min" is allowed, as well as "1h 45min" and "1 h 45      min "
Duration ParseDuration(std::string_view durationStr);

/// Create a string representation of given duration.
/// No spaces are inserted between a couple of units. For example:
/// "1y6mon" instead of "1y 6mon" will be returned
/// The 'nbSignificantUnits' parameter allows to specify the number of units to display.
string DurationToString(Duration dur, int nbSignificantUnits = 2);

}  // namespace cct
