#include "market.hpp"

#include "cct_exception.hpp"

namespace cct {
Market::Market(std::string_view marketStrRep, char currencyCodeSep) {
  std::size_t sepPos = marketStrRep.find_first_of(currencyCodeSep);
  if (sepPos == std::string_view::npos) {
    throw exception("Market string representation " + std::string(marketStrRep) + " should have a separator");
  }
  _assets.front() = std::string_view(marketStrRep.begin(), marketStrRep.begin() + sepPos);
  _assets.back() = std::string_view(marketStrRep.begin() + sepPos + 1, marketStrRep.end());
}

std::string Market::assetsPairStr(char sep) const {
  std::string ret(_assets.front().str());
  if (sep != 0) {
    ret.push_back(sep);
  }
  ret += _assets.back().str();
  return ret;
}

std::ostream& operator<<(std::ostream& os, const Market& mk) {
  os << mk.str();
  return os;
}

}  // namespace cct