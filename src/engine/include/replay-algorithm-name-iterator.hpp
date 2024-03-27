#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace cct {

/// Convenient class to iterate on the algorithm names, comma separated.
/// If 'algorithmNames' is empty, it will loop on all available ones (given by 'allAlgorithms')
class ReplayAlgorithmNameIterator {
 public:
  ReplayAlgorithmNameIterator(std::string_view algorithmNames, std::span<const std::string_view> allAlgorithms);

  /// Returns true if and only if there is at least one additional algorithm name to iterate on.
  bool hasNext() const;

  /// Get next algorithm name and advance the iterator.
  /// Undefined behavior if 'hasNext' is 'false'.
  std::string_view next();

 private:
  std::span<const std::string_view> _allAlgorithms;
  std::string_view _algorithmNames;
  int32_t _begPos;
  int32_t _endPos;
};

}  // namespace cct