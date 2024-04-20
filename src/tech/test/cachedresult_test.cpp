#include "cachedresult.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "cachedresultvault.hpp"
#include "timedef.hpp"

namespace cct {
namespace {
class Incr {
 public:
  int operator()() { return ++_nbCalls; }

  int operator()(int val) {
    _nbCalls += val;
    return _nbCalls;
  }

  int operator()(int lhs, int rhs) {
    _nbCalls += lhs;
    _nbCalls += rhs;
    return _nbCalls;
  }

 private:
  int _nbCalls{};
};

// We use std::chrono::steady_clock for unit test as it is monotonic (system_clock is not)
// Picking a number that is not too small to avoid issues with slow systems
using SteadyClock = std::chrono::steady_clock;

constexpr SteadyClock::duration kCacheTime = milliseconds(10);
constexpr auto kCacheExpireTime = kCacheTime + milliseconds(2);

template <class T, class... FuncTArgs>
using CachedResultSteadyClock = details::CachedResultImpl<SteadyClock, T, FuncTArgs...>;

using CachedResultOptionsSteadyClock = details::CachedResultOptionsT<SteadyClock::duration>;

using CachedResultVaultSteadyClock = CachedResultVaultT<SteadyClock::duration>;

}  // namespace

class CachedResultTestBasic : public ::testing::Test {
 protected:
  CachedResultVaultSteadyClock vault;
  SteadyClock::duration refreshTime{kCacheTime};
  CachedResultSteadyClock<Incr> cachedResult{CachedResultOptionsSteadyClock(refreshTime, vault)};
};

TEST_F(CachedResultTestBasic, GetCache) {
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(), 3);
}

TEST_F(CachedResultTestBasic, Freeze) {
  EXPECT_EQ(cachedResult.get(), 1);
  vault.freezeAll();
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(), 2);
  vault.unfreezeAll();
  EXPECT_EQ(cachedResult.get(), 3);
}

class CachedResultTest : public ::testing::Test {
 protected:
  using CachedResType = CachedResultSteadyClock<Incr, int, int>;

  SteadyClock::duration refreshTime{kCacheTime};
  CachedResType cachedResult{CachedResultOptionsSteadyClock(refreshTime)};
};

TEST_F(CachedResultTest, GetCache) {
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(3, 4), 14);
}

TEST_F(CachedResultTest, SetInCache) {
  auto nowTime = SteadyClock::now();
  cachedResult.set(42, nowTime, 3, 4);

  EXPECT_EQ(cachedResult.get(3, 4), 42);
  EXPECT_EQ(cachedResult.get(3, 4), 42);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  cachedResult.set(42, nowTime, 3, 4);  // timestamp too old, should not be set
  EXPECT_EQ(cachedResult.get(3, 4), 7);

  cachedResult.set(42, nowTime + 2 * kCacheExpireTime, 3, 4);  // should be set
  EXPECT_EQ(cachedResult.get(3, 4), 42);
}

TEST_F(CachedResultTest, RetrieveFromCache) {
  auto [ptr, ts] = cachedResult.retrieve(-5, 3);

  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(ts, SteadyClock::time_point{});

  EXPECT_EQ(cachedResult.get(-5, 3), -2);
  std::tie(ptr, ts) = cachedResult.retrieve(-5, 3);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(*ptr, -2);
  EXPECT_GT(ts, SteadyClock::time_point());

  std::tie(ptr, ts) = cachedResult.retrieve(-4, 3);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(ts, SteadyClock::time_point{});
}

class CachedResultTestZeroRefreshTime : public ::testing::Test {
 protected:
  using CachedResType = CachedResultSteadyClock<Incr, int, int>;

  SteadyClock::duration refreshTime{};
  CachedResType cachedResult{CachedResultOptionsSteadyClock(refreshTime)};
};

TEST_F(CachedResultTestZeroRefreshTime, GetNoCache) {
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 14);
  EXPECT_EQ(cachedResult.get(3, 4), 21);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(3, 2), 26);
}

class CachedResultTestMaxRefreshTime : public ::testing::Test {
 protected:
  using CachedResType = CachedResultSteadyClock<Incr, int, int>;

  SteadyClock::duration refreshTime{SteadyClock::duration::max()};
  CachedResType cachedResult{CachedResultOptionsSteadyClock(refreshTime)};
};

TEST_F(CachedResultTestMaxRefreshTime, GetCache) {
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
}

}  // namespace cct
