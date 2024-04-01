#pragma once

#include <climits>
#include <cstdint>
#include <span>
#include <string_view>

#include "cct_fixedcapacityvector.hpp"
#include "cct_string.hpp"

namespace cct::ssl {

std::string_view GetOpenSSLVersion();

/// @brief Helper type containing the number of bytes of the SHA
enum class ShaType : int16_t { kSha256 = 256 / CHAR_BIT, kSha512 = 512 / CHAR_BIT };

using Md256 = FixedCapacityVector<char, static_cast<int16_t>(ShaType::kSha256)>;
using Md512 = FixedCapacityVector<char, static_cast<int16_t>(ShaType::kSha512)>;

/// @brief Compute Sha256 from 'data'
Md256 Sha256(std::string_view data);

Md512 ShaBin(ShaType shaType, std::string_view data, std::string_view secret);

string ShaHex(ShaType shaType, std::string_view data, std::string_view secret);

string ShaDigest(ShaType shaType, std::string_view data);

string ShaDigest(ShaType shaType, std::span<const string> data);

}  // namespace cct::ssl