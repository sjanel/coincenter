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

class CachedResultOptions {
 public:
  explicit CachedResultOptions(Duration refreshPeriod) : _refreshPeriod(refreshPeriod) {}

  CachedResultOptions(Duration refreshPeriod, CachedResultVault &cacheResultVault)
      : _refreshPeriod(refreshPeriod), _pCacheResultVault(std::addressof(cacheResultVault)) {}

 private:
  template <class T, class... FuncTArgs>
  friend class CachedResult;

  Duration _refreshPeriod;
  CachedResultVault *_pCacheResultVault = nullptr;
};

/// Wrapper of an object of type T (should be a functor) for which the underlying method is called at most once per
/// given period of time. May be useful to automatically cache some API calls in an easy and efficient way.
template <class T, class... FuncTArgs>
class CachedResult : public CachedResultBase {
 public:
  using ResultType = std::remove_cvref_t<decltype(std::declval<T>()(std::declval<FuncTArgs>()...))>;
  using ResPtrTimePair = std::pair<const ResultType *, TimePoint>;

 private:
  using TKey = std::tuple<std::remove_cvref_t<FuncTArgs>...>;
  using TValue = std::pair<ResultType, TimePoint>;
  using MapType = std::unordered_map<TKey, TValue, HashTuple>;

 public:
  template <class... TArgs>
  explicit CachedResult(CachedResultOptions opts, TArgs &&...args)
      : CachedResultBase(opts._refreshPeriod), _func(std::forward<TArgs &&>(args)...) {
    if (opts._pCacheResultVault) {
      opts._pCacheResultVault->registerCachedResult(*this);
    }
  }

  CachedResult(const CachedResult &) = delete;
  CachedResult &operator=(const CachedResult &) = delete;

  CachedResult(CachedResult &&) noexcept = default;
  CachedResult &operator=(CachedResult &&) noexcept = default;

  /// Sets given value associated to the key built with given parameters,
  /// if given timestamp is more recent than the one associated to the value already present at this key (if any)
  template <class ResultTypeT, class... Args>
  void set(ResultTypeT &&val, TimePoint t, Args &&...funcArgs) {
    auto [it, inserted] =
        _cachedResultsMap.try_emplace(TKey(std::forward<Args &&>(funcArgs)...), std::forward<ResultTypeT>(val), t);
    if (!inserted && it->second.second < t) {
      it->second = TValue(std::forward<ResultTypeT>(val), t);
    }
  }

  /// Get the latest value associated to the key built with given parameters.
  /// If the value is too old according to refresh period, it will be recomputed automatically.
  template <class... Args>
  const ResultType &get(Args &&...funcArgs) {
    TKey key(std::forward<Args &&>(funcArgs)...);
    TimePoint t = Clock::now();
    auto flattenTuple = [this](auto &&...values) { return _func(std::forward<decltype(values) &&>(values)...); };
    if (_state == State::kForceUniqueRefresh) {
      _cachedResultsMap.clear();
      _state = State::kForceCache;
    }
    auto it = _cachedResultsMap.find(key);
    if (it == _cachedResultsMap.end()) {
      TValue val(std::apply(flattenTuple, key), t);
      it = _cachedResultsMap.insert_or_assign(std::move(key), std::move(val)).first;
    } else if (_state != State::kForceCache && _refreshPeriod != Duration::max() &&
               it->second.second + _refreshPeriod < t) {
      it->second = TValue(std::apply(flattenTuple, std::move(key)), t);
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

}  // namespace cct