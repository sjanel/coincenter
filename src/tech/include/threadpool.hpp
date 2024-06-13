#pragma once

#include <concepts>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"

namespace cct {

/// @brief C++ ThreadPool implementation. Number of threads is to be specified at creation of the object.
/// @note original code taken from https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h, with modifications:
///         - Rule of 5: delete all special members.
///         - Utility function parallelTransform added.
///         - C++20 version with std::invoke_result instead of std::result_of and std::jthread that calls join
///         automatically
class ThreadPool {
 public:
  explicit ThreadPool(int nbThreads = 1);

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  ~ThreadPool();

  auto nbWorkers() const noexcept { return _workers.size(); }

  // Add new work item to the pool
  // By default, arguments will be copied for safety. If you want to pass arguments by reference,
  // make sure that the reference lifetime is valid through the whole execution time of the future,
  // and wrap the argument you want to pass by reference with 'std::ref'.
  template <class Func, class... Args>
  std::future<std::invoke_result_t<Func, Args...>> enqueue(Func&& func, Args&&... args);

  // Parallel version of std::transform with unary operation.
  // This function will first enqueue all the tasks at one, using waiting threads of the thread pool,
  // and then retrieves and moves the results to 'out', as for std::transform.
  // Note: the objects passed in argument from input range are not copied and passed by reference (through
  // std::reference_wrapper)
  template <std::ranges::input_range InputRange, std::weakly_incrementable OutputIt,
            std::copy_constructible UnaryOperation>
    requires std::indirectly_writable<OutputIt,
                                      std::invoke_result_t<UnaryOperation, std::ranges::range_reference_t<InputRange>>>
  OutputIt parallelTransform(InputRange&& r, OutputIt result, UnaryOperation op);

  // Parallel version of std::transform with binary operation.
  // Note: the objects passed in argument from input ranges are not copied and passed by reference (through
  // std::reference_wrapper)
  template <std::ranges::input_range InputRange1, std::ranges::input_range InputRange2,
            std::weakly_incrementable OutputIt, std::copy_constructible BinaryOperation>
    requires std::indirectly_writable<OutputIt,
                                      std::invoke_result_t<BinaryOperation, std::ranges::range_reference_t<InputRange1>,
                                                           std::ranges::range_reference_t<InputRange2>>>
  OutputIt parallelTransform(InputRange1&& r1, InputRange2&& r2, OutputIt result, BinaryOperation op);

 private:
  using TasksQueue = std::queue<std::function<void()>>;

  template <class Futures, class OutputIt>
  OutputIt retrieveAllResults(Futures& futures, OutputIt out);

  void stopRequested();

  // the task queue
  TasksQueue _tasks;

  // synchronization
  std::mutex _queueMutex;
  std::condition_variable _condition;
  bool _stop = false;

  // join is automatically called at the destruction of the std::jthread.
  // '_workers' should be destroyed first at ThreadPool destruction so it must be placed as last member.
  vector<std::jthread> _workers;
};

inline ThreadPool::ThreadPool(int nbThreads) {
  if (nbThreads < 1) {
    throw invalid_argument("number of threads should be strictly positive");
  }
  _workers.reserve(static_cast<decltype(_workers)::size_type>(nbThreads));
  for (decltype(nbThreads) threadPos = 0; threadPos < nbThreads; ++threadPos) {
    _workers.emplace_back([this] {
      while (true) {
        TasksQueue::value_type task;

        {
          std::unique_lock<std::mutex> lock(this->_queueMutex);
          this->_condition.wait(lock, [this] { return this->_stop || !this->_tasks.empty(); });
          if (this->_stop && this->_tasks.empty()) {
            break;
          }
          task = std::move(this->_tasks.front());
          this->_tasks.pop();
        }
        task();
      }
    });
  }
}

inline ThreadPool::~ThreadPool() {
  stopRequested();
  _condition.notify_all();
}

template <class Func, class... Args>
inline std::future<std::invoke_result_t<Func, Args...>> ThreadPool::enqueue(Func&& func, Args&&... args) {
  // std::bind copies the arguments. To avoid copies, you can use std::ref to copy reference instead.
  using return_type = std::invoke_result_t<Func, Args...>;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(_queueMutex);

    // don't allow enqueueing after stopping the pool
    if (_stop) {
      throw std::runtime_error("attempt to enqueue on ThreadPool being destroyed");
    }

    _tasks.emplace([task]() { (*task)(); });
  }
  _condition.notify_one();
  return res;
}

template <std::ranges::input_range InputRange, std::weakly_incrementable OutputIt,
          std::copy_constructible UnaryOperation>
  requires std::indirectly_writable<OutputIt,
                                    std::invoke_result_t<UnaryOperation, std::ranges::range_reference_t<InputRange>>>
inline OutputIt ThreadPool::parallelTransform(InputRange&& r, OutputIt result, UnaryOperation op) {
  using FutureT = std::future<std::invoke_result_t<UnaryOperation, std::ranges::range_reference_t<InputRange>>>;
  SmallVector<FutureT, kTypicalNbPrivateAccounts> futures;
  if constexpr (std::ranges::sized_range<InputRange>) {
    futures.reserve(std::ranges::size(r));
  }
  for (auto& v : r) {
    futures.emplace_back(enqueue(op, std::ref(v)));
  }
  return retrieveAllResults(futures, result);
}

template <std::ranges::input_range InputRange1, std::ranges::input_range InputRange2,
          std::weakly_incrementable OutputIt, std::copy_constructible BinaryOperation>
  requires std::indirectly_writable<OutputIt,
                                    std::invoke_result_t<BinaryOperation, std::ranges::range_reference_t<InputRange1>,
                                                         std::ranges::range_reference_t<InputRange2>>>
inline OutputIt ThreadPool::parallelTransform(InputRange1&& r1, InputRange2&& r2, OutputIt result, BinaryOperation op) {
  using FutureT = std::future<std::invoke_result_t<BinaryOperation, std::ranges::range_reference_t<InputRange1>,
                                                   std::ranges::range_reference_t<InputRange2>>>;
  SmallVector<FutureT, kTypicalNbPrivateAccounts> futures;
  if constexpr (std::ranges::sized_range<InputRange1>) {
    futures.reserve(std::ranges::size(r1));
  }

  auto it2 = std::ranges::begin(r2);
  for (auto it1 = std::ranges::begin(r1), endIt1 = std::ranges::end(r1); it1 != endIt1; ++it1, ++it2) {
    futures.emplace_back(enqueue(op, std::ref(*it1), std::ref(*it2)));
  }
  return retrieveAllResults(futures, result);
}

template <class Futures, class OutputIt>
inline OutputIt ThreadPool::retrieveAllResults(Futures& futures, OutputIt out) {
  auto nbFutures = futures.size();
  int nbExceptionsThrown = 0;
  for (decltype(nbFutures) runPos = 0; runPos < nbFutures; ++runPos, ++out) {
    try {
      *out = futures[runPos].get();
    } catch (const std::exception& e) {
      // When a future throws an exception, it will be rethrown at the get() method call.
      // We need to catch it and finish getting all the results before we can rethrow it.
      using OutputType = std::remove_cvref_t<decltype(*out)>;
      // value initialize the result for this thread. Probably not needed, but safer.
      *out = OutputType();
      log::critical("exception caught in thread pool: {}", e.what());
      ++nbExceptionsThrown;
    }
  }
  if (nbExceptionsThrown != 0) {
    // In this command line implementation of coincenter, I choose to rethrow any exception thrown by threads.
    // In a server implementation, we could maybe only log the error and not rethrow the exception
    throw exception("{} exception(s) thrown in thread pool", nbExceptionsThrown);
  }

  return out;
}

inline void ThreadPool::stopRequested() {
  std::unique_lock<std::mutex> lock(_queueMutex);
  _stop = true;
}

}  // namespace cct