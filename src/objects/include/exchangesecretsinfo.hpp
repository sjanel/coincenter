#pragma once

#include "exchangename.hpp"

namespace cct {
class ExchangeSecretsInfo {
 public:
  /// Constructs a default Exchange secrets info.
  /// All private keys will be loaded and used.
  ExchangeSecretsInfo() noexcept = default;

  /// Constructs a custom Exchange secrets info from a list of public exchange names.
  /// For each exchange in the list, private keys will not be loaded and used.
  /// However, if the list is empty, no secrets will be loaded. To use all secrets, call the default constructor.
  ExchangeSecretsInfo(PublicExchangeNames &&exchangesWithoutSecrets)
      : _exchangesWithoutSecrets(std::move(exchangesWithoutSecrets)),
        _allExchangesWithoutSecrets(_exchangesWithoutSecrets.empty()) {}

  const PublicExchangeNames &exchangesWithoutSecrets() const { return _exchangesWithoutSecrets; }

  bool allExchangesWithoutSecrets() const { return _allExchangesWithoutSecrets; }

 private:
  PublicExchangeNames _exchangesWithoutSecrets;
  bool _allExchangesWithoutSecrets = false;
};
}  // namespace cct