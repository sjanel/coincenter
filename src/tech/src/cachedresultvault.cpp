#include "cachedresultvault.hpp"

#include "cct_exception.hpp"

namespace cct {
void CachedResultVault::registerCachedResult(CachedResultBase &cacheResult) {
  _cachedResults.push_back(std::addressof(cacheResult));
}

void CachedResultVault::freezeAll() {
  if (_allFrozen) {
    throw exception("unfreezeAll should be called after one freezeAll");
  }
  for (CachedResultBase *p : _cachedResults) {
    p->freeze();
  }
  _allFrozen = true;
}

void CachedResultVault::unfreezeAll() {
  if (!_allFrozen) {
    throw exception("unfreezeAll should be called after one freezeAll");
  }
  for (CachedResultBase *p : _cachedResults) {
    p->unfreeze();
  }
  _allFrozen = false;
}
}  // namespace cct