#pragma once

#include <filesystem>
#include <string_view>

namespace cct {

/// Constructs a protobuf sub path containing the data directory, the exchange name and the object to be serialized.
std::filesystem::path ComputeProtoSubPath(std::string_view dataDir, std::string_view exchangeName,
                                          std::string_view protobufObjectName);

/// Converts a month integer (starting at 1) into a 2 fixed-sized string.
/// Example:
///    - 1  -> "01"
///    - 11 -> "11"
std::string_view MonthStr(int month);

/// Converts a day of month int (starting at 1) into a 2 fixed-sized string.
/// Example:
///    - 1  -> "01"
///    - 11 -> "11"
std::string_view DayOfMonthStr(int dayOfMonth);

/// From an hour of day in [0, 23], return the file name for a protobuf binary serialization file.
/// Example:
///  ComputeProtoFileName(4) -> "04:00:00_04:59:59.binpb"
std::string_view ComputeProtoFileName(int hourOfDay);

}  // namespace cct