#include "market.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "toupperlower.hpp"
#include "unreachable.hpp"

namespace cct {
Market::Market(std::string_view marketStrRep, char currencyCodeSep, Type type) {
  const std::size_t sepPos = marketStrRep.find(currencyCodeSep);
  if (sepPos == std::string_view::npos) {
    throw exception("Market string representation {} should have a separator {}", marketStrRep, currencyCodeSep);
  }
  if (marketStrRep.find(currencyCodeSep, sepPos + 1) != std::string_view::npos) {
    throw exception("Market string representation {} should have a unique separator {}", marketStrRep, currencyCodeSep);
  }
  _assets.front() = std::string_view(marketStrRep.begin(), marketStrRep.begin() + sepPos);
  _assets.back() = std::string_view(marketStrRep.begin() + sepPos + 1, marketStrRep.end());

  setType(type);
}

string Market::assetsPairStr(char sep, bool lowerCase) const {
  string ret;
  switch (type()) {
    case Type::kRegularExchangeMarket:
      break;
    case Type::kFiatConversionMarket:
      ret.push_back('*');
      break;
    default:
      unreachable();
  }
  base().appendStrTo(ret);
  if (sep != 0) {
    ret.push_back(sep);
  }
  quote().appendStrTo(ret);
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