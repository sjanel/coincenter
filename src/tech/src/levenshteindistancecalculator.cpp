#include "levenshteindistancecalculator.hpp"

#include <algorithm>
#include <numeric>
#include <string_view>

namespace cct {
int LevenshteinDistanceCalculator::operator()(std::string_view word1, std::string_view word2) {
  if (word1.size() > word2.size()) {
    std::swap(word1, word2);
  }

  const int l1 = static_cast<int>(word1.size()) + 1;
  if (l1 > static_cast<int>(_minDistance.size())) {
    // Favor insert instead of resize to ensure reallocations are exponential
    _minDistance.insert(_minDistance.end(), l1 - _minDistance.size(), 0);
  }

  std::iota(_minDistance.begin(), _minDistance.end(), 0);

  const int l2 = static_cast<int>(word2.size()) + 1;
  for (int word2Pos = 1; word2Pos < l2; ++word2Pos) {
    int previousDiagonal = _minDistance[0];

    ++_minDistance[0];

    for (int word1Pos = 1; word1Pos < l1; ++word1Pos) {
      const int previousDiagonalSave = _minDistance[word1Pos];
      if (word1[word1Pos - 1] == word2[word2Pos - 1]) {
        _minDistance[word1Pos] = previousDiagonal;
      } else {
        _minDistance[word1Pos] =
            std::min(std::min(_minDistance[word1Pos - 1], _minDistance[word1Pos]), previousDiagonal) + 1;
      }
      previousDiagonal = previousDiagonalSave;
    }
  }

  return _minDistance[l1 - 1];
}
}  // namespace cct