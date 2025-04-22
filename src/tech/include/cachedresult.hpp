#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "cachedresultvault.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_hash.hpp"
#include "timedef.hpp"

namespace cct {

namespace details {
template <class DurationT>
class CachedResultOptionsT {
 public:
  explicit CachedResultOptionsT(DurationT refreshPeriod) : _refreshPeriod(refreshPeriod) {}

  CachedResultOptionsT(DurationT refreshPeriod, CachedResultVaultT<DurationT> &cacheResultVault)
      : _refreshPeriod(refreshPeriod), _pCacheResultVault(std::addressof(cacheResultVault)) {}

 private:
  template <class, class, class...>
  friend class CachedResultWithArgs;

  template <class, class>
  friend class CachedResultWithoutArgs;

  DurationT _refreshPeriod;
  CachedResultVaultT<DurationT> *_pCacheResultVault = nullptr;
};
}  // namespace details

using CachedResultOptions = details::CachedResultOptionsT<Duration>;

namespace details {

template <class ClockT, class T, class... FuncTArgs>
class CachedResultWithArgs : public CachedResultBase<typename ClockT::duration> {
 public:
  using ResultType = std::remove_cvref_t<decltype(std::declval<T>()(std::declval<FuncTArgs>()...))>;
  using TimePoint = ClockT::time_point;
  using Duration = ClockT::duration;
  using State = CachedResultBase<Duration>::State;

 private:
  using TKey = std::tuple<std::remove_cvref_t<FuncTArgs>...>;

  struct Value {
    template <class R>
    Value(R &&result, TimePoint lastUpdatedTs) : _result(std::forward<R>(result)), _lastUpdatedTs(lastUpdatedTs) {}

    template <class F, class K>
    Value(F &func, K &&key, TimePoint lastUpdatedTs)
        : _result(std::apply(func, std::forward<K>(key))), _lastUpdatedTs(lastUpdatedTs) {}

    ResultType _result;
    TimePoint _lastUpdatedTs;
  };

 public:
  template <class... TArgs>
  explicit CachedResultWithArgs(CachedResultOptionsT<Duration> opts, TArgs &&...args)
      : CachedResultBase<Duration>(opts._refreshPeriod), _func(std::forward<TArgs &&>(args)...) {
    if (opts._pCacheResultVault) {
      opts._pCacheResultVault->registerCachedResult(*this);
    }
  }

  CachedResultWithArgs(const CachedResultWithArgs &) = delete;
  CachedResultWithArgs(CachedResultWithArgs &&) = delete;
  CachedResultWithArgs &operator=(const CachedResultWithArgs &) = delete;
  CachedResultWithArgs &operator=(CachedResultWithArgs &&) = delete;

  ~CachedResultWithArgs() = default;

  /// Sets given value associated to the key built with given parameters,
  /// if given timestamp is more recent than the one associated to the value already present at this key (if any)
  /// refresh period is not checked, if given timestamp is more recent than the one associated to given value, cache
  /// will be updated.
  template <class ResultTypeT, class... Args>
  void set(ResultTypeT &&val, TimePoint timePoint, Args &&...funcArgs) {
    checkPeriodicRehash();

    auto [it, isInserted] =
        _data.try_emplace(TKey(std::forward<Args &&>(funcArgs)...), std::forward<ResultTypeT>(val), timePoint);
    if (!isInserted && it->second._lastUpdatedTs < timePoint) {
      it->second = Value(std::forward<ResultTypeT>(val), timePoint);
    }
  }

  /// Get the latest value associated to the key built with given parameters.
  /// If the value is too old according to refresh period, it will be recomputed automatically.
  template <class... Args>
  const ResultType &get(Args &&...funcArgs) {
    const auto nowTime = ClockT::now();

    if (this->_state == State::kForceUniqueRefresh) {
      _data.clear();

      this->_state = State::kForceCache;
    } else {
      checkPeriodicRehash();
    }

    const auto flattenTuple = [this](auto &&...values) { return _func(std::forward<decltype(values) &&>(values)...); };
    TKey key(std::forward<Args &&>(funcArgs)...);
    auto [it, isInserted] = _data.try_emplace(key, flattenTuple, key, nowTime);
    if (!isInserted && this->_state != State::kForceCache &&
        // less or equal to make sure value is always refreshed for a zero refresh period
        this->_refreshPeriod <= nowTime - it->second._lastUpdatedTs) {
      it->second = Value(flattenTuple, std::move(key), nowTime);
    }
    return it->second._result;
  }

  /// Retrieve a {pointer, lastUpdateTime} to latest value associated to the key built with given parameters.
  /// If no value has been computed for this key, returns a nullptr.
  template <class... Args>
  std::pair<const ResultType *, TimePoint> retrieve(Args &&...funcArgs) const {
    auto it = _data.find(TKey(std::forward<Args &&>(funcArgs)...));
    if (it == _data.end()) {
      return {};
    }
    return {std::addressof(it->second._result), it->second._lastUpdatedTs};
  }

 private:
  void checkPeriodicRehash() {
    static constexpr decltype(this->_flushCounter) kFlushCheckCounter = 20000;
    if (++this->_flushCounter < kFlushCheckCounter) {
      return;
    }
    this->_flushCounter = 0;

    const auto nowTime = ClockT::now();

    for (auto it = _data.begin(); it != _data.end();) {
      if (this->_refreshPeriod < nowTime - it->second._lastUpdatedTs) {
        // Data has expired, remove it
        it = _data.erase(it);
      } else {
        ++it;
      }
    }

    _data.rehash(_data.size());
  }

  T _func;
  std::unordered_map<TKey, Value, HashTuple> _data;
};

/// Optimization when there is no key.
/// Data is stored inlined in the CachedResult object in this case.
template <class ClockT, class T>
class CachedResultWithoutArgs : public CachedResultBase<typename ClockT::duration> {
 public:
  using ResultType = std::remove_cvref_t<decltype(std::declval<T>()())>;
  using TimePoint = ClockT::time_point;
  using Duration = ClockT::duration;
  using State = CachedResultBase<Duration>::State;

  template <class... TArgs>
  explicit CachedResultWithoutArgs(CachedResultOptionsT<Duration> opts, TArgs &&...args)
      : CachedResultBase<Duration>(opts._refreshPeriod), _func(std::forward<TArgs &&>(args)...) {
    if (opts._pCacheResultVault) {
      opts._pCacheResultVault->registerCachedResult(*this);
    }
  }

  CachedResultWithoutArgs(const CachedResultWithoutArgs &) = delete;
  CachedResultWithoutArgs(CachedResultWithoutArgs &&) = delete;
  CachedResultWithoutArgs &operator=(const CachedResultWithoutArgs &) = delete;
  CachedResultWithoutArgs &operator=(CachedResultWithoutArgs &&) = delete;

  ~CachedResultWithoutArgs() = default;

  /// Sets given value for given time stamp, if time stamp currently associated to last value is older.
  template <class ResultTypeT>
  void set(ResultTypeT &&val, TimePoint timePoint) {
    if (_lastUpdatedTs < timePoint) {
      if (isResultConstructed()) {
        _resultStorage.front() = std::forward<ResultTypeT>(val);
      } else {
        _resultStorage.push_back(std::forward<ResultTypeT>(val));
      }

      _lastUpdatedTs = timePoint;
    }
  }

  /// Get the latest value.
  /// If the value is too old according to refresh period, it will be recomputed automatically.
  const ResultType &get() {
    const auto nowTime = ClockT::now();

    if (this->_state == State::kForceUniqueRefresh) {
      _lastUpdatedTs = TimePoint{};
      this->_state = State::kForceCache;
    }

    if (_resultStorage.empty() || (this->_refreshPeriod < nowTime - _lastUpdatedTs &&
                                   (this->_state != State::kForceCache || _lastUpdatedTs == TimePoint{}))) {
      const auto flattenTuple = [this](auto &&...values) {
        return _func(std::forward<decltype(values) &&>(values)...);
      };

      static constexpr auto kEmptyTuple = std::make_tuple();
      _resultStorage.assign(static_cast<decltype(_resultStorage)::size_type>(1), std::apply(flattenTuple, kEmptyTuple));
      _lastUpdatedTs = nowTime;
    }

    return _resultStorage.front();
  }

  /// Retrieve a {pointer, lastUpdateTime} to latest value stored in this cache.
  /// If no value has been computed, returns a nullptr.
  std::pair<const ResultType *, TimePoint> retrieve() const {
    return {isResultConstructed() ? _resultStorage.data() : nullptr, _lastUpdatedTs};
  }

 private:
  using ResultStorage = FixedCapacityVector<ResultType, 1>;

  [[nodiscard]] bool isResultConstructed() const noexcept { return !_resultStorage.empty(); }

  T _func;
  ResultStorage _resultStorage;
  TimePoint _lastUpdatedTs;
};

template <class ClockT, class T, class... FuncTArgs>
using CachedResultImpl = std::conditional_t<sizeof...(FuncTArgs) == 0, CachedResultWithoutArgs<ClockT, T>,
                                            CachedResultWithArgs<ClockT, T, FuncTArgs...>>;
}  // namespace details

/// Wrapper of a functor F for which the underlying method is called at most once per
/// given period of time, provided at construction time.
/// May be useful to automatically cache some API calls in an easy and efficient way.
/// The underlying implementation differs according to FuncTArgs:
///  - if number of FuncTArgs is zero: data is stored inline. Returned pointers / references from get() / retrieve()
///    methods are never invalidated.
///  - otherwise, data is stored in an unordered_map. Returned pointers / references from get() / retrieve() are
///    invalidated by get() calls.
/// In all cases, CachedResult is not moveable nor copyable, because it would require complex logic for
/// CachedResultVault registers based on addresses of objects.
template <class F, class... FuncTArgs>
using CachedResult = details::CachedResultImpl<Clock, F, FuncTArgs...>;

}  // namespace cct
