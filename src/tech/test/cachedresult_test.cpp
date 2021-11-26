#include "cachedresult.hpp"

#include <gtest/gtest.h>

#include <thread>

namespace cct {
namespace {
struct Incr {
  int operator()() { return ++nbCalls; }

  int operator()(int a) {
    nbCalls += a;
    return nbCalls;
  }

  int operator()(int a, int b) {
    nbCalls += a;
    nbCalls += b;
    return nbCalls;
  }

  int nbCalls{};
};

}  // namespace

class CachedResultTestBasic : public ::testing::Test {
 protected:
  CachedResultTestBasic() : vault(), cachedResult(CachedResultOptions(std::chrono::milliseconds(1), vault)) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CachedResultVault vault;
  CachedResult<Incr> cachedResult;
};

TEST_F(CachedResultTestBasic, GetCache) {
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
}

TEST_F(CachedResultTestBasic, Freeze) {
  EXPECT_EQ(cachedResult.get(), 1);
  vault.freezeAll();
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  EXPECT_EQ(cachedResult.get(), 2);
  vault.unfreezeAll();
  EXPECT_EQ(cachedResult.get(), 3);
}

class CachedResultTest : public ::testing::Test {
 protected:
  using CachedResType = CachedResult<Incr, int, int>;

  CachedResultTest() : cachedResult(CachedResultOptions(std::chrono::milliseconds(1))) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CachedResType cachedResult;
};

TEST_F(CachedResultTest, GetCache) {
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  EXPECT_EQ(cachedResult.get(3, 4), 14);
}

TEST_F(CachedResultTest, SetInCache) {
  TimePoint t = Clock::now();
  cachedResult.set(42, t, 3, 4);
  EXPECT_EQ(cachedResult.get(3, 4), 42);
  EXPECT_EQ(cachedResult.get(3, 4), 42);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  EXPECT_EQ(cachedResult.get(3, 4), 7);
  cachedResult.set(42, t, 3, 4);  // timestamp too old, should not be set
  EXPECT_EQ(cachedResult.get(3, 4), 7);
}

TEST_F(CachedResultTest, RetrieveFromCache) {
  using RetrieveRetType = CachedResType::ResPtrTimePair;

  EXPECT_EQ(cachedResult.retrieve(-5, 3), RetrieveRetType());
  EXPECT_EQ(cachedResult.get(-5, 3), -2);
  RetrieveRetType ret = cachedResult.retrieve(-5, 3);
  ASSERT_NE(ret.first, nullptr);
  EXPECT_EQ(*ret.first, -2);
  EXPECT_GT(ret.second, TimePoint());
  EXPECT_EQ(cachedResult.retrieve(-4, 3), RetrieveRetType());
}
}  // namespace cct