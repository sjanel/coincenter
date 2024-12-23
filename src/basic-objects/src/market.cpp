#include "market.hpp"

#include <cstddef>
#include <ostream>
#include <string_view>

#include "cct_exception.hpp"

namespace cct {
Market::Market(std::string_view marketStrRep, char currencyCodeSep, Type type) {
  const std::size_t sepPos = marketStrRep.find(currencyCodeSep);
  if (sepPos == std::string_view::npos) {
    throw exception("Market string representation {} should have a separator {}", marketStrRep, currencyCodeSep);
  }
  if (marketStrRep.find(currencyCodeSep, sepPos + 1) != std::string_view::npos) {
    throw exception("Market string representation {} should have a unique separator {}", marketStrRep, currencyCodeSep);
  }

  _assets = {std::string_view(marketStrRep.begin(), marketStrRep.begin() + sepPos),
             std::string_view(marketStrRep.begin() + sepPos + 1, marketStrRep.end())};

  setType(type);
}

std::ostream& operator<<(std::ostream& os, const Market& mk) {
  os << mk.str();
  return os;
}

}  // namespace cct