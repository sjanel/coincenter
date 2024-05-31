#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

static constexpr const char* const kTimeYearToSecondSpaceSeparatedFormat = "%Y-%m-%d %H:%M:%S";

static constexpr const char* const kTimeYearToSecondTSeparatedFormat = "%Y-%m-%dT%H:%M:%S";

static constexpr const char* const kTimeYearToSecondTSeparatedFormatUTC = "%Y-%m-%dT%H:%M:%SZ";

/// Nonce represents an increasing value only over time - in string format.
/// Mostly for documentation purposes
using Nonce = string;

/// Get a string representation of a given time point, printed in UTC ISO 8061 format by default.
/// 'format' specifies the string style, with the default argument value given as example:
///    'YYYY-MM-DD HH:MM:SS'
string TimeToString(TimePoint timePoint, const char* format = kTimeYearToSecondTSeparatedFormatUTC);

/// Parse a string representation of a given time point and return a time_point.
TimePoint StringToTime(std::string_view timeStr, const char* format = kTimeYearToSecondTSeparatedFormatUTC);

/// Create a Nonce as the number of milliseconds since Epoch time in string format.
Nonce Nonce_TimeSinceEpochInMs(Duration delay = Duration{});

/// Create a Nonce in literal format.
/// Example: '2021-06-01T14:44:13'
inline Nonce Nonce_LiteralDate(const char* format) { return TimeToString(Clock::now(), format); }

}  // namespace cct
