#pragma once

#include <utility>

#include "cachedresultvault.hpp"

namespace cct::api {

/// RAII object whose lifetime triggers a special behavior of the CachedResults contained by the vault of this
/// Exchange:
///  - Next query will force external call and refresh the cache
///  - All subsequent queries will return the same cached value
/// This is to ensure constant, deterministic and up to date behavior of search algorithms during their process.
/// At the destruction of the returned handle, all the CachedResults' behavior will come back to standard.
class CacheFreezerRAII {
 public:
  CacheFreezerRAII() noexcept = default;

  explicit CacheFreezerRAII(CachedResultVault &cachedResultVault) : _pCachedResultVault(&cachedResultVault) {
    cachedResultVault.freezeAll();
  }

  CacheFreezerRAII(const CacheFreezerRAII &) = delete;
  CacheFreezerRAII(CacheFreezerRAII &&rhs) noexcept
      : _pCachedResultVault(std::exchange(rhs._pCachedResultVault, nullptr)) {}

  CacheFreezerRAII &operator=(const CacheFreezerRAII &) = delete;
  CacheFreezerRAII &operator=(CacheFreezerRAII &&rhs) noexcept {
    if (this != &rhs) {
      _pCachedResultVault = std::exchange(rhs._pCachedResultVault, nullptr);
    }
    return *this;
  }

  ~CacheFreezerRAII() {
    if (_pCachedResultVault != nullptr) {
      _pCachedResultVault->unfreezeAll();
    }
  }

 private:
  CachedResultVault *_pCachedResultVault{};
};

class ExchangeBase {
 public:
  virtual ~ExchangeBase() = default;

  virtual void updateCacheFile() const {}

 protected:
  ExchangeBase() = default;
};
}  // namespace cct::api