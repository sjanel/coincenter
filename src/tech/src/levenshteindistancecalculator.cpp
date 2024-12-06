#include "levenshteindistancecalculator.hpp"

#include <algorithm>
#include <numeric>
#include <string_view>

namespace cct {
int LevenshteinDistanceCalculator::operator()(std::string_view word1, std::string_view word2) {
  if (word1.size() > word2.size()) {
    std::swap(word1, word2);
  }

  using size_type = std::string_view::size_type;

  const auto l1 = word1.size() + static_cast<size_type>(1);
  if (l1 > _minDistance.size()) {
    // Favor insert instead of resize to ensure reallocations are exponential
    _minDistance.insert(_minDistance.end(), l1 - _minDistance.size(), 0);
  }

  std::iota(_minDistance.begin(), _minDistance.end(), 0);

  const auto l2 = word2.size() + static_cast<size_type>(1);
  for (size_type word2Pos = 1; word2Pos < l2; ++word2Pos) {
    auto previousDiagonal = _minDistance[0];

    ++_minDistance[0];

    for (size_type word1Pos = 1; word1Pos < l1; ++word1Pos) {
      const auto previousDiagonalSave = _minDistance[word1Pos];
      if (word1[word1Pos - 1] == word2[word2Pos - 1]) {
        _minDistance[word1Pos] = previousDiagonal;
      } else {
        _minDistance[word1Pos] = std::min({_minDistance[word1Pos - 1], _minDistance[word1Pos], previousDiagonal}) + 1;
      }
      previousDiagonal = previousDiagonalSave;
    }
  }

  return _minDistance[l1 - 1];
}
}  // namespace cct