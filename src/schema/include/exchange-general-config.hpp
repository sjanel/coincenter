#pragma once

#include <type_traits>

#include "optional-or-type.hpp"

namespace cct::schema {

namespace details {
template <bool Optional>
struct ExchangeGeneralConfig {
  template <class T>
  void mergeWith(const T &other)
    requires(std::is_same_v<T, ExchangeGeneralConfig<true>> && !Optional)
  {
    if (other.enabled) {
      enabled = *other.enabled;
    }
  }

  optional_or_t<bool, Optional> enabled{true};
};

using ExchangeGeneralConfigOptional = ExchangeGeneralConfig<true>;

}  // namespace details

using ExchangeGeneralConfig = details::ExchangeGeneralConfig<false>;

}  // namespace cct::schema