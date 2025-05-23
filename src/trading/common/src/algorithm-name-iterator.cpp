#include "algorithm-name-iterator.hpp"

#include <algorithm>
#include <span>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"

namespace cct {

namespace {
constexpr std::string_view kAlgorithmNameSeparator = ",";

auto FindNextSeparatorPos(std::string_view str, std::string_view::size_type pos = 0) {
  pos = str.find(kAlgorithmNameSeparator, pos);
  if (pos == std::string_view::npos) {
    pos = str.length();
  }
  return pos;
}
}  // namespace

AlgorithmNameIterator::AlgorithmNameIterator(std::string_view algorithmNames,
                                             std::span<const std::string_view> allAlgorithms)
    : _allAlgorithms(allAlgorithms),
      _algorithmNames(algorithmNames),
      _begPos(0),
      _endPos(FindNextSeparatorPos(_algorithmNames)) {
  if (std::ranges::any_of(allAlgorithms, [](const auto algName) {
        return algName.find(kAlgorithmNameSeparator) != std::string_view::npos;
      })) {
    throw exception("Algorithm names cannot contain '{}' as it's used as a separator", kAlgorithmNameSeparator);
  }
}

bool AlgorithmNameIterator::hasNext() const {
  if (_algorithmNames.empty()) {
    return std::cmp_less(_begPos, _allAlgorithms.size());
  }

  return std::cmp_not_equal(_begPos, _algorithmNames.length());
}

std::string_view AlgorithmNameIterator::next() {
  if (_algorithmNames.empty()) {
    return _allAlgorithms[_begPos++];
  }

  std::string_view nextAlgorithmName(_algorithmNames.begin() + _begPos, _algorithmNames.begin() + _endPos);

  if (std::cmp_equal(_endPos, _algorithmNames.length())) {
    _begPos = _endPos;
  } else {
    _begPos = _endPos + kAlgorithmNameSeparator.length();
    _endPos = FindNextSeparatorPos(_algorithmNames, _begPos);
  }

  return nextAlgorithmName;
}

}  // namespace cct