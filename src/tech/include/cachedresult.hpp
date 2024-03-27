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
  using ResPtrTimePair = std::pair<const ResultType *, TimePoint>;
  using State = CachedResultBase<Duration>::State;

 private:
  using TKey = std::tuple<std::remove_cvref_t<FuncTArgs>...>;
  using TValue = std::pair<ResultType, TimePoint>;
  using MapType = std::unordered_map<TKey, TValue, HashTuple>;

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
  template <class ResultTypeT, class... Args>
  void set(ResultTypeT &&val, TimePoint timePoint, Args &&...funcArgs) {
    auto [it, inserted] = _cachedResultsMap.try_emplace(TKey(std::forward<Args &&>(funcArgs)...),
                                                        std::forward<ResultTypeT>(val), timePoint);
    if (!inserted && it->second.second < timePoint) {
      it->second = TValue(std::forward<ResultTypeT>(val), timePoint);
    }
  }

  /// Get the latest value associated to the key built with given parameters.
  /// If the value is too old according to refresh period, it will be recomputed automatically.
  template <class... Args>
  const ResultType &get(Args &&...funcArgs) {
    TKey key(std::forward<Args &&>(funcArgs)...);

    auto nowTime = ClockT::now();

    auto flattenTuple = [this](auto &&...values) { return _func(std::forward<decltype(values) &&>(values)...); };

    if (this->_state == State::kForceUniqueRefresh) {
      _cachedResultsMap.clear();
      this->_state = State::kForceCache;
    }

    auto it = _cachedResultsMap.find(key);
    if (it == _cachedResultsMap.end()) {
      TValue val(std::apply(flattenTuple, key), nowTime);
      it = _cachedResultsMap.insert_or_assign(std::move(key), std::move(val)).first;
    } else if (this->_state != State::kForceCache && this->_refreshPeriod < nowTime - it->second.second) {
      it->second = TValue(std::apply(flattenTuple, std::move(key)), nowTime);
    }

    return it->second.first;
  }

  /// Retrieve a {pointer, lastUpdateTime} to latest value associated to the key built with given parameters.
  /// If no value has been computed for this key, returns a nullptr.
  template <class... Args>
  ResPtrTimePair retrieve(Args &&...funcArgs) const {
    auto it = _cachedResultsMap.find(TKey(std::forward<Args &&>(funcArgs)...));
    return it == _cachedResultsMap.end() ? ResPtrTimePair()
                                         : ResPtrTimePair(std::addressof(it->second.first), it->second.second);
  }

 private:
  T _func;
  MapType _cachedResultsMap;
};

template <class T, class... FuncTArgs>
using CachedResult = CachedResultT<Clock, T, FuncTArgs...>;

}  // namespace cct