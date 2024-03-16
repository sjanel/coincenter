#include "threadpool.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <forward_list>
#include <functional>
#include <future>
#include <numeric>
#include <stdexcept>
#include <thread>

#include "cct_vector.hpp"

namespace cct {

namespace {
using namespace std::chrono_literals;

int SlowDouble(const int &val) {
  if (val == 42) {
    throw std::invalid_argument("42 is not the answer to the ultimate question of life");
  }
  std::this_thread::sleep_for(10ms);
  return val * 2;
}

int SlowAdd(const int &lhs, const int &rhs) {
  std::this_thread::sleep_for(10ms);
  return lhs + rhs;
}

struct NonCopyable {
  NonCopyable(int val = 0) : val(val) {}

  NonCopyable(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&) = default;
  NonCopyable &operator=(const NonCopyable &) = delete;
  NonCopyable &operator=(NonCopyable &&) = default;

  ~NonCopyable() = default;

  int val;
};

int SlowDoubleNonCopyable(const NonCopyable &val) {
  std::this_thread::sleep_for(10ms);
  return val.val * 2;
}
}  // namespace

TEST(ThreadPoolTest, Enqueue) {
  ThreadPool threadPool(2);
  vector<std::future<int>> results;

  constexpr int kNbElems = 4;
  for (int elem = 0; elem < kNbElems; ++elem) {
    results.push_back(threadPool.enqueue(SlowDouble, elem));
  }

  for (int elem = 0; elem < kNbElems; ++elem) {
    EXPECT_EQ(results[elem].get(), elem * 2);
  }
}

TEST(ThreadPoolTest, EnqueueNonCopyable) {
  ThreadPool threadPool(2);
  vector<std::future<int>> results;

  constexpr int kNbElems = 4;
  vector<NonCopyable> inputData(kNbElems);
  for (int elem = 0; elem < kNbElems; ++elem) {
    inputData[elem] = NonCopyable(elem);
    results.push_back(threadPool.enqueue(SlowDoubleNonCopyable, std::ref(inputData[elem])));
  }

  for (int elem = 0; elem < kNbElems; ++elem) {
    EXPECT_EQ(results[elem].get(), elem * 2);
  }
}

TEST(ThreadPoolTest, ParallelTransformRandomInputIt) {
  ThreadPool threadPool(4);
  constexpr int kNbElems = 22;
  vector<int> data(kNbElems);
  std::iota(data.begin(), data.end(), 0);
  vector<int> res(data.size());

  threadPool.parallelTransform(data.begin(), data.end(), res.begin(), SlowDouble);

  for (int elem = 0; elem < kNbElems; ++elem) {
    EXPECT_EQ(2 * data[elem], res[elem]);
  }
}

TEST(ThreadPoolTest, ParallelTransformForwardInputIt) {
  ThreadPool threadPool(3);
  constexpr int kNbElems = 13;
  std::forward_list<int> data(kNbElems);
  std::iota(data.begin(), data.end(), 0);

  std::forward_list<int> res(kNbElems);
  threadPool.parallelTransform(data.begin(), data.end(), res.begin(), SlowDouble);

  for (auto dataIt = data.begin(), resIt = res.begin(); dataIt != data.end(); ++dataIt, ++resIt) {
    EXPECT_EQ(2 * *dataIt, *resIt);
  }
}

TEST(ThreadPoolTest, ParallelTransformException) {
  ThreadPool threadPool(3);
  constexpr int kNbElems = 5;
  vector<int> data(kNbElems);
  std::iota(data.begin(), data.end(), 40);
  vector<int> res(data.size(), 40);

  bool exceptionThrown = false;

  try {
    threadPool.parallelTransform(data.begin(), data.end(), res.begin(), SlowDouble);
  } catch (...) {
    exceptionThrown = true;
  }

  EXPECT_TRUE(exceptionThrown);
  EXPECT_EQ(res, (vector<int>{80, 82, 0, 86, 88}));
}

TEST(ThreadPoolTest, ParallelTransformBinaryOperation) {
  ThreadPool threadPool(2);
  constexpr int kNbElems = 11;

  std::forward_list<int> data1(kNbElems);
  std::iota(data1.begin(), data1.end(), 0);

  vector<int> data2(kNbElems);
  std::iota(data2.begin(), data2.end(), 3);

  vector<int> res(kNbElems);
  threadPool.parallelTransform(data1.begin(), data1.end(), data2.begin(), res.begin(), SlowAdd);

  auto resIt = res.begin();
  auto data1It = data1.begin();
  auto data2It = data2.begin();
  for (; data1It != data1.end(); ++data1It, ++data2It, ++resIt) {
    EXPECT_EQ(*data1It + *data2It, *resIt);
  }
}

TEST(ThreadPoolTest, LongTaskToBeFinishedBeforeThreadPoolDestroyed) {
  ThreadPool threadPool(1);

  constexpr int kNbElems = 5;
  for (int elem = 0; elem < kNbElems; ++elem) {
    threadPool.enqueue(SlowDouble, elem);
  }
}
}  // namespace cct
