#pragma once

#include <string>
#include <string_view>

namespace cct {

std::string ComputeProtoSubPath(std::string_view dataDir, std::string_view exchangeName,
                                std::string_view protobufObjectName);

/// From an hour of day in [0, 23], return the file name for a protobuf binary serialization file.
/// Example:
///  ComputeProtoFileName(4) -> "04:00:00_04:59:59.binpb"
std::string_view ComputeProtoFileName(int hourOfDay);

}  // namespace cct