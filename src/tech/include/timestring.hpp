#pragma once

#include <cstdint>
#include <string_view>

#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

static constexpr const char* const kTimeYearToSecondSpaceSeparatedFormat = "%Y-%m-%d %H:%M:%S";

static constexpr const char* const kTimeYearToSecondTSeparatedFormat = "%Y-%m-%dT%H:%M:%S";

/// Nonce represents an increasing value only over time - in string format.
/// Mostly for documentation purposes
using Nonce = string;

/// Get a string representation of a given time point.
/// 'format' specifies the string style, with the default argument value given as example:
///    'YYYY-MM-DD HH:MM:SS'
string ToString(TimePoint timePoint, const char* format = kTimeYearToSecondSpaceSeparatedFormat);

/// Parse a string representation of a given time point and return a time_point.
TimePoint FromString(std::string_view timeStr, const char* format = kTimeYearToSecondSpaceSeparatedFormat);

/// Create a Nonce as the number of milliseconds since Epoch time in string format.
Nonce Nonce_TimeSinceEpochInMs(Duration msDelay = Duration{});

/// Create a Nonce in literal format.
/// Example: '2021-06-01T14:44:13'
inline Nonce Nonce_LiteralDate(const char* format) { return ToString(Clock::now(), format); }
}  // namespace cct
