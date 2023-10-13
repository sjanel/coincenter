#include "cachedresult.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "cachedresultvault.hpp"

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
constexpr std::chrono::steady_clock::duration kCacheTime = std::chrono::milliseconds(10);
constexpr auto kCacheExpireTime = kCacheTime + std::chrono::milliseconds(2);

template <class T, class... FuncTArgs>
using CachedResultSteadyClock = CachedResultT<std::chrono::steady_clock, T, FuncTArgs...>;

using CachedResultOptionsSteadyClock = CachedResultOptionsT<std::chrono::steady_clock::duration>;

using CachedResultVaultSteadyClock = CachedResultVaultT<std::chrono::steady_clock::duration>;

}  // namespace

class CachedResultTestBasic : public ::testing::Test {
 protected:
  CachedResultVaultSteadyClock vault;
  CachedResultSteadyClock<Incr> cachedResult{CachedResultOptionsSteadyClock(kCacheTime, vault)};
};

TEST_F(CachedResultTestBasic, GetCache) {
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
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

  CachedResType cachedResult{CachedResultOptionsSteadyClock(kCacheTime)};
};

TEST_F(CachedResultTest, GetCache) {
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(3, 4), 14);
}

TEST_F(CachedResultTest, SetInCache) {
  auto nowTime = std::chrono::steady_clock::now();
  cachedResult.set(42, nowTime, 3, 4);
  EXPECT_EQ(cachedResult.get(3, 4), 42);
  EXPECT_EQ(cachedResult.get(3, 4), 42);
  std::this_thread::sleep_for(kCacheExpireTime);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  cachedResult.set(42, nowTime, 3, 4);  // timestamp too old, should not be set
  EXPECT_EQ(cachedResult.get(3, 4), 7);
}

TEST_F(CachedResultTest, RetrieveFromCache) {
  using RetrieveRetType = CachedResType::ResPtrTimePair;

  EXPECT_EQ(cachedResult.retrieve(-5, 3), RetrieveRetType());
  EXPECT_EQ(cachedResult.get(-5, 3), -2);
  RetrieveRetType ret = cachedResult.retrieve(-5, 3);
  ASSERT_NE(ret.first, nullptr);
  EXPECT_EQ(*ret.first, -2);
  EXPECT_GT(ret.second, std::chrono::steady_clock::time_point());
  EXPECT_EQ(cachedResult.retrieve(-4, 3), RetrieveRetType());
}
}  // namespace cct