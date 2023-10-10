#pragma once

#include <string_view>

#include "cct_vector.hpp"

namespace cct {
class LevenshteinDistanceCalculator {
 public:
  LevenshteinDistanceCalculator() noexcept = default;

  /// Computes the levenshtein distance between both input words.
  /// Complexity is in 'word1.length() * word2.length()' in time,
  /// min(word1.length(), word2.length()) in space.
  int operator()(std::string_view word1, std::string_view word2);

 private:
  // This is only for caching purposes, so that repeated calls to distance calculation do not allocate memory each time
  vector<int> _minDistance;
};
}  // namespace cct