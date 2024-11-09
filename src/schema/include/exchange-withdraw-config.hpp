#pragma once

#include <type_traits>

#include "optional-or-type.hpp"

namespace cct::schema {

namespace details {

template <bool Optional>
struct ExchangeWithdrawConfig {
  template <class T, std::enable_if_t<std::is_same_v<T, ExchangeWithdrawConfig<true>> && !Optional, bool> = true>
  void mergeWith(const T &other) {
    if (other.validateDepositAddressesInFile) {
      validateDepositAddressesInFile = *other.validateDepositAddressesInFile;
    }
  }

  optional_or_t<bool, Optional> validateDepositAddressesInFile{};
};

using ExchangeWithdrawConfigOptional = ExchangeWithdrawConfig<true>;

}  // namespace details

using ExchangeWithdrawConfig = details::ExchangeWithdrawConfig<false>;

}  // namespace cct::schema