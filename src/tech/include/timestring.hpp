#pragma once

#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

/// Nonce represents an increasing value only over time - in string format.
/// Mostly for documentation purposes
using Nonce = string;

/// Get a string representation of a given time point.
/// 'format' specifies the string style, with the default argument value given as example:
///    'YYYY-MM-DD HH:MM:SS'
string ToString(TimePoint p, const char* format = "%Y-%m-%d %H:%M:%S");

/// Parse a string representation of a given time point and return a time_point.
TimePoint FromString(const char* timeStr, const char* format = "%Y-%m-%d %H:%M:%S");

/// Create a Nonce as the number of milliseconds since Epoch time in string format.
Nonce Nonce_TimeSinceEpochInMs(int64_t msDelay = 0);

/// Create a Nonce in litteral format.
/// Example: '2021-06-01T14:44:13'
inline Nonce Nonce_LiteralDate(const char* format) { return ToString(Clock::now(), format); }
}  // namespace cct
