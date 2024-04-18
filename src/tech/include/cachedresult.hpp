#pragma once

#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "cachedresultvault.hpp"
#include "cct_hash.hpp"
#include "timedef.hpp"

namespace cct {
template <class DurationT>
class CachedResultOptionsT {
 public:
  explicit CachedResultOptionsT(DurationT refreshPeriod) : _refreshPeriod(refreshPeriod) {}

  CachedResultOptionsT(DurationT refreshPeriod, CachedResultVaultT<DurationT> &cacheResultVault)
      : _refreshPeriod(refreshPeriod), _pCacheResultVault(std::addressof(cacheResultVault)) {}

 private:
  template <class, class, class...>
  friend class CachedResultT;

  DurationT _refreshPeriod;
  CachedResultVaultT<DurationT> *_pCacheResultVault = nullptr;
};

using CachedResultOptions = CachedResultOptionsT<Duration>;

/// Wrapper of an object of type T (should be a functor) for which the underlying method is called at most once per
/// given period of time. May be useful to automatically cache some API calls in an easy and efficient way.
template <class ClockT, class T, class... FuncTArgs>
class CachedResultT : public CachedResultBase<typename ClockT::duration> {
 public:
  using ResultType = std::remove_cvref_t<decltype(std::declval<T>()(std::declval<FuncTArgs>()...))>;
  using TimePoint = ClockT::time_point;
  using Duration = ClockT::duration;
  using State = CachedResultBase<Duration>::State;

 private:
  using TKey = std::tuple<std::remove_cvref_t<FuncTArgs>...>;

  struct Value {
    template <class R>
    Value(R &&result, TimePoint lastUpdatedTs) : result(std::forward<R>(result)), lastUpdatedTs(lastUpdatedTs) {}

    template <class F, class K>
    Value(F &func, K &&key, TimePoint lastUpdatedTs)
        : result(std::apply(func, std::forward<K>(key))), lastUpdatedTs(lastUpdatedTs) {}

    ResultType result;
    TimePoint lastUpdatedTs;
  };

 public:
  template <class... TArgs>
  explicit CachedResultT(CachedResultOptionsT<Duration> opts, TArgs &&...args)
      : CachedResultBase<Duration>(opts._refreshPeriod), _func(std::forward<TArgs &&>(args)...) {
    if (opts._pCacheResultVault) {
      opts._pCacheResultVault->registerCachedResult(*this);
    }
  }

  /// Sets given value associated to the key built with given parameters,
  /// if given timestamp is more recent than the one associated to the value already present at this key (if any)
  /// refresh period is not checked, if given timestamp is more recent than the one associated to given value, cache
  /// will be updated.
  template <class ResultTypeT, class... Args>
  void set(ResultTypeT &&val, TimePoint timePoint, Args &&...funcArgs) {
    checkPeriodicRehash();

    auto [it, isInserted] = _cachedResultsMap.try_emplace(TKey(std::forward<Args &&>(funcArgs)...),
                                                          std::forward<ResultTypeT>(val), timePoint);
    if (!isInserted && it->second.lastUpdatedTs < timePoint) {
      it->second = Value(std::forward<ResultTypeT>(val), timePoint);
    }
  }

  /// Get the latest value associated to the key built with given parameters.
  /// If the value is too old according to refresh period, it will be recomputed automatically.
  template <class... Args>
  const ResultType &get(Args &&...funcArgs) {
    const auto nowTime = ClockT::now();

    if (this->_state == State::kForceUniqueRefresh) {
      _cachedResultsMap.clear();
      this->_state = State::kForceCache;
    } else {
      checkPeriodicRehash();
    }

    const auto flattenTuple = [this](auto &&...values) { return _func(std::forward<decltype(values) &&>(values)...); };

    TKey key(std::forward<Args &&>(funcArgs)...);
    auto [it, isInserted] = _cachedResultsMap.try_emplace(key, flattenTuple, key, nowTime);
    if (!isInserted && this->_state != State::kForceCache &&
        this->_refreshPeriod < nowTime - it->second.lastUpdatedTs) {
      it->second = Value(flattenTuple, std::move(key), nowTime);
    }
    return it->second.result;
  }

  /// Retrieve a {pointer, lastUpdateTime} to latest value associated to the key built with given parameters.
  /// If no value has been computed for this key, returns a nullptr.
  template <class... Args>
  std::pair<const ResultType *, TimePoint> retrieve(Args &&...funcArgs) const {
    auto it = _cachedResultsMap.find(TKey(std::forward<Args &&>(funcArgs)...));
    if (it == _cachedResultsMap.end()) {
      return {};
    }
    return {std::addressof(it->second.result), it->second.lastUpdatedTs};
  }

 private:
  void checkPeriodicRehash() {
    static constexpr decltype(this->_flushCounter) kFlushCheckCounter = 20000;
    if (++this->_flushCounter < kFlushCheckCounter) {
      return;
    }
    this->_flushCounter = 0;

    const auto nowTime = ClockT::now();

    for (auto it = _cachedResultsMap.begin(); it != _cachedResultsMap.end();) {
      if (this->_refreshPeriod < nowTime - it->second.lastUpdatedTs) {
        // Data has expired, remove it
        it = _cachedResultsMap.erase(it);
      } else {
        ++it;
      }
    }

    _cachedResultsMap.rehash(_cachedResultsMap.size());
  }

  using MapType = std::unordered_map<TKey, Value, HashTuple>;

  T _func;
  MapType _cachedResultsMap;
};

template <class T, class... FuncTArgs>
using CachedResult = CachedResultT<Clock, T, FuncTArgs...>;

}  // namespace cct