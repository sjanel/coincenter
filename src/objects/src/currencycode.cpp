#include "currencycode.hpp"

namespace cct {
const CurrencyCode CurrencyCode::kNeutral;

std::ostream &operator<<(std::ostream &os, const CurrencyCode &c) {
  c.print(os);
  return os;
}
}  // namespace cct