#pragma once

#include "cct_vector.hpp"
#include "timehelpers.hpp"

namespace cct {
class CachedResultBase {
 protected:
  friend class CachedResultVault;

  enum class State { kStandardRefresh, kForceUniqueRefresh, kForceCache };

  explicit CachedResultBase(Duration refreshPeriod) : _refreshPeriod(refreshPeriod) {}

  void freeze() noexcept { _state = State::kForceUniqueRefresh; }

  void unfreeze() noexcept { _state = State::kStandardRefresh; }

  Duration _refreshPeriod;
  State _state = State::kStandardRefresh;
};

/// Represents an Observer of CachedResults.
/// It can be used to launch queries on all objects listening to this observer.
class CachedResultVault {
 public:
  CachedResultVault() noexcept = default;

  void registerCachedResult(CachedResultBase &cacheResult);

  void freezeAll();

  void unfreezeAll() noexcept;

 private:
  using CachedResultPtrs = vector<CachedResultBase *>;

  CachedResultPtrs _cachedResults;
  bool _allFrozen = false;
};
}  // namespace cct