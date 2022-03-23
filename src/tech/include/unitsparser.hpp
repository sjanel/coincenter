#pragma once

#include <cstdint>
#include <string_view>

namespace cct {
/// Parses a string representation of a number of bytes.
/// string should contain an integral number (decimal not supported) possibly followed by one of these units:
///  - T, G, M, k for multiples of 1000
///  - Ti, Gi, Mi, Ki for multiples of 1024
/// Note: it is a simplified version of the syntax used by Kubernetes:
/// https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/
int64_t ParseNumberOfBytes(std::string_view sizeStr);
}  // namespace cct