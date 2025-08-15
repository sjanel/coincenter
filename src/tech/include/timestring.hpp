#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

inline constexpr const char* const kTimeYearToSecondSpaceSeparatedFormat = "%Y-%m-%d %H:%M:%S";

inline constexpr const char* const kTimeYearToSecondTSeparatedFormat = "%Y-%m-%dT%H:%M:%S";

inline constexpr const char* const kTimeYearToSecondTSeparatedFormatUTC = "%Y-%m-%dT%H:%M:%SZ";

/// Nonce represents an increasing value only over time - in string format.
/// Mostly for documentation purposes
using Nonce = string;

/// Get a string representation of a given time point, printed in UTC ISO 8061 format by default.
/// 'format' specifies the string style, with the default argument value given as example:
///    'YYYY-MM-DD HH:MM:SS'
string TimeToString(TimePoint timePoint, const char* format = kTimeYearToSecondTSeparatedFormatUTC);

/// Writes chars of the representation of a given time point in ISO 8601 UTC format with maximum performance and return
/// a pointer after the last char written. The written format will be (in millisecond precision):
///   - 'YYYY-MM-DDTHH:MM:SS.sssZ'
/// The buffer should have a space of at least 24 chars.
/// This function is around 8 times faster than TimeToString.
char* TimeToStringIso8601UTC(TimePoint timePoint, char* buffer);

char* TimeToStringIso8601UTCWithMillis(TimePoint timePoint, char* buffer);

/// Parse a string representation of a given time point and return a time_point.
TimePoint StringToTime(std::string_view timeStr, const char* format = kTimeYearToSecondTSeparatedFormatUTC);

/// Parse a string representation of a given time point in ISO 8601 UTC format with maximum performance and return a
/// time_point. Accepted formats are (even without trailing Z, the time will be considered UTC):
///   - 'YYYY-MM-DDTHH:MM:SSZ'
///   - 'YYYY-MM-DDTHH:MM:SS.[0-9]+Z'
///   - 'YYYY-MM-DD HH:MM:SS'
///   - 'YYYY-MM-DD HH:MM:SS.[0-9]+'
/// Warning: Few checks are done on the input. It should contain at least 19 chars (up to the seconds part).
/// This function is around 40 times faster than StringToTime.
TimePoint StringToTimeISO8601UTC(const char* begPtr, const char* endPtr);

inline TimePoint StringToTimeISO8601UTC(std::string_view timeStr) {
  return StringToTimeISO8601UTC(timeStr.data(), timeStr.data() + timeStr.size());
}

/// Create a Nonce as the number of milliseconds since Epoch time in string format.
Nonce Nonce_TimeSinceEpochInMs(Duration delay = Duration{});

/// Create a Nonce in literal format.
/// Example: '2021-06-01T14:44:13'
inline Nonce Nonce_LiteralDate(const char* format) { return TimeToString(Clock::now(), format); }

}  // namespace cct
