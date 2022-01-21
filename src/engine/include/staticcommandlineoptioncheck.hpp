#pragma once

#include <algorithm>
#include <array>
#include <numeric>
#include <string_view>

#include "commandlineoption.hpp"

namespace cct {

/// Compile time checker of arguments. Currently, the following checks are made:
///  - Unicity of short hand flags
///  - Unicity of long names
template <class T, size_t... N>
constexpr bool StaticCommandLineOptionsCheck(std::array<T, N>... ar) {
  constexpr size_t kNbArrays = sizeof...(ar);

  const T* datas[kNbArrays] = {&ar[0]...};
  constexpr size_t lengths[kNbArrays] = {ar.size()...};

  constexpr size_t kSumLen = std::accumulate(lengths, lengths + kNbArrays, 0);

  std::array<CommandLineOption, kSumLen> all{};

  size_t allIdx = 0;
  for (size_t dataIdx = 0; dataIdx < kNbArrays; ++dataIdx) {
    for (size_t lenIdx = 0; lenIdx < lengths[dataIdx]; ++lenIdx) {
      all[allIdx++] = std::get<0>(datas[dataIdx][lenIdx]);
    }
  }

  // Check short names equality with a bistet hashmap of presence
  // (std::bitset is unfortunately not constexpr yet)
  uint64_t charPresenceBmp[8]{};
  for (const auto& f : all) {
    if (f.hasShortName()) {
      uint8_t c = static_cast<uint8_t>(f.shortNameChar());
      uint64_t& subBmp = charPresenceBmp[c / 64];
      if ((subBmp & (static_cast<uint64_t>(1) << (c % 64)))) {
        return false;
      }
      subBmp |= (static_cast<uint64_t>(1) << (c % 64));
    }
  }

  // Check long names equality by sorting on them
  std::ranges::sort(all, [](const auto& lhs, const auto& rhs) { return lhs.fullName() < rhs.fullName(); });
  return std::ranges::adjacent_find(
             all, [](const auto& lhs, const auto& rhs) { return lhs.fullName() == rhs.fullName(); }) == all.end();
}

}  // namespace cct