#include "currencycode.hpp"

#include <stdexcept>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {
const CurrencyCode CurrencyCode::kNeutral;

CurrencyCode::CurrencyCode(std::string_view acronym) {
  if (CCT_UNLIKELY(_data.size() < acronym.size())) {
    log::warn("Acronym {} is too long, truncating to {}", acronym, acronym.substr(0, _data.size()));
    acronym.remove_suffix(acronym.size() - _data.size());
  }
  std::fill(std::transform(acronym.begin(), acronym.end(), _data.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); }),
            _data.end(), '\0');  // Fill extra chars to 0 is important as we always read them for code generation
}

std::ostream &operator<<(std::ostream &os, const CurrencyCode &c) {
  c.print(os);
  return os;
}
}  // namespace cct