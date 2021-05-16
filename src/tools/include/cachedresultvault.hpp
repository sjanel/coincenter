#pragma once

#include <chrono>

#include "cct_vector.hpp"

namespace cct {
class CachedResultBase {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

 protected:
  friend class CachedResultVault;

  enum class State { kStandardRefresh, kForceUniqueRefresh, kForceCache };

  explicit CachedResultBase(Clock::duration refreshPeriod)
      : _refreshPeriod(refreshPeriod), _state(State::kStandardRefresh) {}

  void freeze() noexcept { _state = State::kForceUniqueRefresh; }

  void unfreeze() noexcept { _state = State::kStandardRefresh; }

  Clock::duration _refreshPeriod;
  State _state;
};

/// Represents an Observer of CachedResults.
/// It can be used to launch queries on all objects listening to this observer.
class CachedResultVault {
 public:
  CachedResultVault() noexcept : _cachedResults(), _allFrozen(false) {}

  void registerCachedResult(CachedResultBase &cacheResult);

  void freezeAll();

  void unfreezeAll();

 private:
  using CachedResultPtrs = cct::vector<CachedResultBase *>;

  CachedResultPtrs _cachedResults;
  bool _allFrozen;
};
}  // namespace cct