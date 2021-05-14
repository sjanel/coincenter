#pragma once

#include "cachedresultvault.hpp"

namespace cct {
namespace api {
class ExchangeBase {
 public:
  class UniqueQueryRefresherHandle {
   public:
    explicit UniqueQueryRefresherHandle(CachedResultVault &cachedResultVault)
        : _pCachedResultVault(std::addressof(cachedResultVault)) {
      cachedResultVault.freezeAll();
    }

    UniqueQueryRefresherHandle(const UniqueQueryRefresherHandle &) = delete;
    UniqueQueryRefresherHandle(UniqueQueryRefresherHandle &&o)
        : _pCachedResultVault(std::exchange(o._pCachedResultVault, nullptr)) {}

    UniqueQueryRefresherHandle &operator=(const UniqueQueryRefresherHandle &) = delete;
    UniqueQueryRefresherHandle &operator=(UniqueQueryRefresherHandle &&o) {
      if (this != std::addressof(o)) {
        _pCachedResultVault = std::exchange(o._pCachedResultVault, nullptr);
      }
      return *this;
    }

    ~UniqueQueryRefresherHandle() {
      if (_pCachedResultVault) {
        _pCachedResultVault->unfreezeAll();
      }
    }

   private:
    CachedResultVault *_pCachedResultVault;
  };

  /// Get a RAII object whose lifetime triggers a special behavior of the CachedResults contained by the vault of this
  /// Exchange:
  ///  - Next query will force external call and refresh the cache
  ///  - All subsequent queries will return the same cached value
  /// This is to ensure constant, deterministic and up to date behavior of search algorithms during their process.
  /// At the destruction of the returned handle, all the CachedResults' behavior will come back to standard.
  UniqueQueryRefresherHandle freezeAll() { return UniqueQueryRefresherHandle(_cachedResultVault); }

  virtual void updateCacheFile() const {}

 protected:
  ExchangeBase() = default;

  CachedResultVault _cachedResultVault;
};
}  // namespace api
}  // namespace cct