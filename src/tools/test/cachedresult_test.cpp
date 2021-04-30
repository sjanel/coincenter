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

  int nbCalls;
};

}  // namespace
TEST(CachedResultTest, Basic) {
  CachedResult<Incr> cachedResult(CachedResultOptions(std::chrono::milliseconds(1)));

  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  EXPECT_EQ(cachedResult.get(), 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_EQ(cachedResult.get(), 2);
  EXPECT_EQ(cachedResult.get(), 2);

  CachedResult<Incr, int, int> cachedResult2(CachedResultOptions(std::chrono::milliseconds(1)));
  EXPECT_EQ(cachedResult2.get(3, 4), 7);
  EXPECT_EQ(cachedResult2.get(3, 4), 7);
  EXPECT_EQ(cachedResult2.get(3, 4), 7);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_EQ(cachedResult2.get(3, 4), 14);
}

TEST(CachedResultTest, Freeze) {
  CachedResultVault vault;
  CachedResult<Incr> cachedResult(CachedResultOptions(std::chrono::milliseconds(1), vault));

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
}  // namespace cct