#pragma once

#include <cctype>

namespace cct {
/// Safe std::isalnum version. See https://en.cppreference.com/w/cpp/string/byte/isalnum
inline bool isalnum(char c) { return std::isalnum(static_cast<unsigned char>(c)); }

/// Safe std::isalpha version. See https://en.cppreference.com/w/cpp/string/byte/isalpha
inline bool isalpha(char c) { return std::isalpha(static_cast<unsigned char>(c)); }

/// Safe std::isblank version. See https://en.cppreference.com/w/cpp/string/byte/isblank
inline bool isblank(char c) { return std::isblank(static_cast<unsigned char>(c)); }

/// Safe std::isdigit version. See https://en.cppreference.com/w/cpp/string/byte/isdigit
inline bool isdigit(char c) { return std::isdigit(static_cast<unsigned char>(c)); }

/// Safe std::islower version. See https://en.cppreference.com/w/cpp/string/byte/islower
inline bool islower(char c) { return std::islower(static_cast<unsigned char>(c)); }

/// Safe std::isspace version. See https://en.cppreference.com/w/cpp/string/byte/isspace
inline bool isspace(char c) { return std::isspace(static_cast<unsigned char>(c)); }

}  // namespace cct