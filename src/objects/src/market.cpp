#include "market.hpp"

#include "cct_exception.hpp"

namespace cct {
Market::Market(std::string_view marketStrRep, char currencyCodeSep) {
  std::size_t sepPos = marketStrRep.find(currencyCodeSep);
  if (sepPos == std::string_view::npos) {
    string ex("Market string representation ");
    ex.append(marketStrRep).append(" should have a separator");
    throw exception(std::move(ex));
  }
  _assets.front() = std::string_view(marketStrRep.begin(), marketStrRep.begin() + sepPos);
  _assets.back() = std::string_view(marketStrRep.begin() + sepPos + 1, marketStrRep.end());
}

string Market::assetsPairStr(char sep, bool lowerCase) const {
  string ret(_assets.front().str());
  if (sep != 0) {
    ret.push_back(sep);
  }
  _assets.back().appendStr(ret);
  if (lowerCase) {
    std::ranges::transform(ret, ret.begin(), tolower);
  }
  return ret;
}

std::ostream& operator<<(std::ostream& os, const Market& mk) {
  os << mk.str();
  return os;
}

}  // namespace cct