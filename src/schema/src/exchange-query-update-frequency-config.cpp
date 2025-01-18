#include "exchange-query-update-frequency-config.hpp"

#include <algorithm>

namespace cct::schema {

void MergeWith(ExchangeQueryUpdateFrequencyConfig &src, ExchangeQueryUpdateFrequencyConfig &des) {
  const auto sortByQueryType = [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; };

  std::ranges::sort(src, sortByQueryType);
  std::ranges::sort(des, sortByQueryType);

  const auto srcEnd = src.end();
  for (auto desIt = des.begin(), srcIt = src.begin(); desIt != des.end() || srcIt != srcEnd; ++desIt) {
    if (desIt == des.end()) {
      des.insert(des.end(), srcIt, srcEnd);
      break;
    }
    if (srcIt == srcEnd) {
      break;
    }
    if (desIt->first == srcIt->first) {
      desIt->second = std::min(desIt->second, srcIt->second);
      ++srcIt;
    } else if (desIt->first > srcIt->first) {
      desIt = des.insert(desIt, *srcIt);
      ++srcIt;
    }
  }
}

}  // namespace cct::schema