#include "cachedresultvault.hpp"

namespace cct {
void CachedResultVault::registerCachedResult(CachedResultBase &cacheResult) {
  _cachedResults.push_back(std::addressof(cacheResult));
}

void CachedResultVault::freezeAll() {
  if (!_allFrozen) {
    for (CachedResultBase *p : _cachedResults) {
      p->freeze();
    }
    _allFrozen = true;
  }
}

void CachedResultVault::unfreezeAll() noexcept {
  if (_allFrozen) {
    for (CachedResultBase *p : _cachedResults) {
      p->unfreeze();
    }
    _allFrozen = false;
  }
}
}  // namespace cct