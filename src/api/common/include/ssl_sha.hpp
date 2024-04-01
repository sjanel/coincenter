#pragma once

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace cct::ssl {

/// @brief Helper type containing the number of bytes of the SHA
enum class ShaType : uint8_t { kSha256 = 256 / CHAR_BIT, kSha512 = 512 / CHAR_BIT };

std::string_view GetOpenSSLVersion();

using Md256 = std::array<char, static_cast<std::size_t>(ShaType::kSha256)>;
using Md512 = std::array<char, static_cast<std::size_t>(ShaType::kSha512)>;

using Sha256HexArray = std::array<char, 2UL * static_cast<std::size_t>(ShaType::kSha256)>;
using Sha512HexArray = std::array<char, 2UL * static_cast<std::size_t>(ShaType::kSha512)>;

using Sha256DigestArray = std::array<char, 2UL * static_cast<std::size_t>(ShaType::kSha256)>;
using Sha512DigestArray = std::array<char, 2UL * static_cast<std::size_t>(ShaType::kSha512)>;

Md256 Sha256Bin(std::string_view data, std::string_view secret);
Md512 Sha512Bin(std::string_view data, std::string_view secret);

Md256 Sha256(std::string_view data);

Sha256HexArray Sha256Hex(std::string_view data, std::string_view secret);
Sha512HexArray Sha512Hex(std::string_view data, std::string_view secret);

Sha256DigestArray Sha256Digest(std::string_view data);
Sha512DigestArray Sha512Digest(std::string_view data);

Sha256DigestArray Sha256Digest(std::span<const std::string_view> data);
Sha512DigestArray Sha512Digest(std::span<const std::string_view> data);

}  // namespace cct::ssl