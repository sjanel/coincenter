#include "currencycode.hpp"

#include "cct_invalid_argument_exception.hpp"

namespace cct {
CurrencyCode CurrencyCode::fromStrSafe(std::string_view acronym) {
  if (acronym.length() > kAcronymMaxLen) {
    string msg("Currency ");
    msg.append(acronym).append(" is too long");
    throw invalid_argument(std::move(msg));
  }
  return CurrencyCode(acronym);
}
}  // namespace cct