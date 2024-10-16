#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace cct {

/// Parses a string representation of a number of bytes.
/// string should contain an integral number (decimal not supported) possibly followed by one of these units:
///  - T, G, M, k for multiples of 1000
///  - Ti, Gi, Mi, Ki for multiples of 1024
/// Note: it is a simplified version of the syntax used by Kubernetes:
/// https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/
int64_t ParseNumberOfBytes(std::string_view sizeStr);

/// Writes to 'buf' the string representation of the number of bytes.
/// If given buffer is too small, it will throw an exception.
/// Returns a span of the buffer containing the string representation.
std::span<char> BytesToStr(int64_t numberOfBytes, std::span<char> buf);

}  // namespace cct