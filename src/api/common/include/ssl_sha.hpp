#pragma once

#include <array>
#include <climits>
#include <span>
#include <string>

namespace cct {
namespace ssl {

//------------------------------------------------------------------------------
// helper function to compute SHA256:
using Sha256 = std::array<char, 256 / CHAR_BIT>;

Sha256 ComputeSha256(const std::string& data);

enum class ShaType { kSha256, kSha512 };

std::string ShaBin(ShaType shaType, const std::string& data, const char* secret);

std::string ShaHex(ShaType shaType, const std::string& data, const char* secret);

std::string ShaDigest(ShaType shaType, std::span<const std::string> data);

inline std::string ShaDigest(ShaType shaType, const std::string& data) {
  return ShaDigest(shaType, std::span<const std::string>(std::addressof(data), 1));
}
}  // namespace ssl
}  // namespace cct