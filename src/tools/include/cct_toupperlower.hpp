#pragma once

#include <algorithm>
#include <string_view>

#include "cct_string.hpp"

namespace cct {
/// constexpr version of std::toupper with chars, as unfortunately for now (May 2021) std::toupper is not constexpr
constexpr char toupper(char c) noexcept {
  switch (c) {
    // clang-format off
    case 'a': return 'A';
    case 'b': return 'B';
    case 'c': return 'C';
    case 'd': return 'D';
    case 'e': return 'E';
    case 'f': return 'F';
    case 'g': return 'G';
    case 'h': return 'H';
    case 'i': return 'I';
    case 'j': return 'J';
    case 'k': return 'K';
    case 'l': return 'L';
    case 'm': return 'M';
    case 'n': return 'N';
    case 'o': return 'O';
    case 'p': return 'P';
    case 'q': return 'Q';
    case 'r': return 'R';
    case 's': return 'S';
    case 't': return 'T';
    case 'u': return 'U';
    case 'v': return 'V';
    case 'w': return 'W';
    case 'x': return 'X';
    case 'y': return 'Y';
    case 'z': return 'Z';

    default: return c;
      // clang-format on
  }
}

/// constexpr version of std::tolower with chars, as unfortunately for now (May 2021) std::tolower is not constexpr
constexpr char tolower(char c) noexcept {
  switch (c) {
    // clang-format off
    case 'A': return 'a';
    case 'B': return 'b';
    case 'C': return 'c';
    case 'D': return 'd';
    case 'E': return 'e';
    case 'F': return 'f';
    case 'G': return 'g';
    case 'H': return 'h';
    case 'I': return 'i';
    case 'J': return 'j';
    case 'K': return 'k';
    case 'L': return 'l';
    case 'M': return 'm';
    case 'N': return 'n';
    case 'O': return 'o';
    case 'P': return 'p';
    case 'Q': return 'q';
    case 'R': return 'r';
    case 'S': return 's';
    case 'T': return 't';
    case 'U': return 'u';
    case 'V': return 'v';
    case 'W': return 'w';
    case 'X': return 'x';
    case 'Y': return 'y';
    case 'Z': return 'z';

    default: return c;
      // clang-format on
  }
}

inline string toupper(std::string_view str) {
  string ret(str);
  std::transform(ret.begin(), ret.end(), ret.begin(), [](char c) { return toupper(c); });
  return ret;
}

inline string tolower(std::string_view str) {
  string ret(str);
  std::transform(ret.begin(), ret.end(), ret.begin(), [](char c) { return tolower(c); });
  return ret;
}
}  // namespace cct