#pragma once

#include <memory>
#include <utility>

#include "cachedresultvault.hpp"

namespace cct::api {

/// RAII object whose lifetime triggers a special behavior of the CachedResults contained by the vault of this
/// Exchange:
///  - Next query will force external call and refresh the cache
///  - All subsequent queries will return the same cached value
/// This is to ensure constant, deterministic and up to date behavior of search algorithms during their process.
/// At the destruction of the returned handle, all the CachedResults' behavior will come back to standard.
class UniqueQueryHandle {
 public:
  explicit UniqueQueryHandle(CachedResultVault &cachedResultVault)
      : _pCachedResultVault(std::addressof(cachedResultVault)) {
    cachedResultVault.freezeAll();
  }

  UniqueQueryHandle(const UniqueQueryHandle &) = delete;
  UniqueQueryHandle(UniqueQueryHandle &&rhs) noexcept
      : _pCachedResultVault(std::exchange(rhs._pCachedResultVault, nullptr)) {}

  UniqueQueryHandle &operator=(const UniqueQueryHandle &) = delete;
  UniqueQueryHandle &operator=(UniqueQueryHandle &&rhs) noexcept {
    if (this != std::addressof(rhs)) {
      _pCachedResultVault = std::exchange(rhs._pCachedResultVault, nullptr);
    }
    return *this;
  }

  ~UniqueQueryHandle() {
    if (_pCachedResultVault != nullptr) {
      _pCachedResultVault->unfreezeAll();
    }
  }

 private:
  CachedResultVault *_pCachedResultVault;
};

class ExchangeBase {
 public:
  virtual ~ExchangeBase() = default;

  virtual void updateCacheFile() const {}

 protected:
  ExchangeBase() = default;
};
}  // namespace cct::api