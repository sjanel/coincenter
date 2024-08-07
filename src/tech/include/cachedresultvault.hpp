#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>

#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "timedef.hpp"

namespace cct {
template <class DurationT>
class CachedResultBase {
 protected:
  template <class>
  friend class CachedResultVaultT;

  enum class State : int8_t { kStandardRefresh, kForceUniqueRefresh, kForceCache };

  explicit CachedResultBase(DurationT refreshPeriod) : _refreshPeriod(refreshPeriod) {}

  void freeze() noexcept { _state = State::kForceUniqueRefresh; }

  void unfreeze() noexcept { _state = State::kStandardRefresh; }

  DurationT _refreshPeriod;
  uint32_t _flushCounter{};
  State _state = State::kStandardRefresh;
};

/// Represents an Observer of CachedResults.
/// It can be used to launch queries on all objects listening to this observer.
template <class DurationT>
class CachedResultVaultT {
 public:
  void registerCachedResult(CachedResultBase<DurationT> &cacheResult) {
    _cachedResults.push_back(std::addressof(cacheResult));
  }

  void freezeAll() {
    if (!_allFrozen) {
      std::ranges::for_each(_cachedResults, [](CachedResultBase<DurationT> *p) { p->freeze(); });
      _allFrozen = true;
    }
  }

  void unfreezeAll() noexcept {
    if (_allFrozen) {
      std::ranges::for_each(_cachedResults, [](CachedResultBase<DurationT> *p) { p->unfreeze(); });
      _allFrozen = false;
    }
  }

 private:
  using CachedResultPtrs = vector<CachedResultBase<DurationT> *>;

 public:
  using trivially_relocatable = is_trivially_relocatable<CachedResultPtrs>::type;

 private:
  CachedResultPtrs _cachedResults;
  bool _allFrozen = false;
};

using CachedResultVault = CachedResultVaultT<Duration>;

}  // namespace cct