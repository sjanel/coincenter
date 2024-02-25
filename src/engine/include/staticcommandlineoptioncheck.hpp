#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>

#include "commandlineoption.hpp"

namespace cct {

/// Compile time checker of arguments. Currently, the following checks are made:
///  - Uniqueness of short hand flags
///  - Uniqueness of long names
template <class T, std::size_t... N>
consteval bool StaticCommandLineOptionsDuplicatesCheck(std::array<T, N>... ar) {
  auto all = ComputeAllCommandLineOptions(ar...);

  // Check short names equality with a bitset hashmap of presence
  // (std::bitset is unfortunately not constexpr in C++20)
  uint64_t charPresenceBmp[8]{};
  for (const auto& commandLineOption : all) {
    if (commandLineOption.hasShortName()) {
      uint8_t shortNameChar = static_cast<uint8_t>(commandLineOption.shortNameChar());
      uint64_t& subBmp = charPresenceBmp[shortNameChar / 64];
      if ((subBmp & (static_cast<uint64_t>(1) << (shortNameChar % 64)))) {
        return false;
      }
      subBmp |= (static_cast<uint64_t>(1) << (shortNameChar % 64));
    }
  }

  auto endIt = std::remove_if(all.begin(), all.end(), [](const auto& cmd) { return cmd.fullName().front() == '-'; });

  // Check long names equality by sorting on them
  std::sort(all.begin(), endIt, [](const auto& lhs, const auto& rhs) { return lhs.fullName() < rhs.fullName(); });

  return std::adjacent_find(all.begin(), endIt,
                            [](const auto& lhs, const auto& rhs) { return lhs.fullName() == rhs.fullName(); }) == endIt;
}

/// Compile time checker of descriptions. Following checks are made:
///  - Should not start nor end with a '\n'
///  - Should not start no end with a space
template <class T, std::size_t... N>
consteval bool StaticCommandLineOptionsDescriptionCheck(std::array<T, N>... ar) {
  const auto all = ComputeAllCommandLineOptions(ar...);
  const auto isSpaceOrNewLine = [](char ch) { return ch == '\n' || ch == ' '; };

  if (std::ranges::any_of(
          all, [&isSpaceOrNewLine](const auto& cmd) { return isSpaceOrNewLine(cmd.description().front()); })) {
    return false;
  }

  if (std::ranges::any_of(
          all, [&isSpaceOrNewLine](const auto& cmd) { return isSpaceOrNewLine(cmd.description().back()); })) {
    return false;
  }

  return true;
}

template <class T, std::size_t... N>
consteval auto ComputeAllCommandLineOptions(std::array<T, N>... ar) {
  constexpr std::size_t kNbArrays = sizeof...(ar);

  const T* arr[kNbArrays] = {&ar[0]...};
  constexpr std::size_t lengths[kNbArrays] = {ar.size()...};

  constexpr std::size_t kSumLen = std::accumulate(lengths, lengths + kNbArrays, 0);

  std::array<CommandLineOption, kSumLen> all;

  std::size_t allIdx = 0;
  for (std::size_t dataIdx = 0; dataIdx < kNbArrays; ++dataIdx) {
    for (std::size_t lenIdx = 0; lenIdx < lengths[dataIdx]; ++lenIdx) {
      all[allIdx] = std::get<0>(arr[dataIdx][lenIdx]);
      ++allIdx;
    }
  }
  return all;
}

}  // namespace cct
