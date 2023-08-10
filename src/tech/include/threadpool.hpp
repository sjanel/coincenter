#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"

namespace cct {

/// @brief C++ ThreadPool implementation. Number of threads is to be specified at creation of the object.
/// @note original code taken from https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h, with modifications:
///         - Rule of 5: delete all special members.
///         - Utility function parallel_transform added.
///         - C++20 version with std::invoke_result instead of std::result_of and std::jthread that calls join
///         automatically
class ThreadPool {
 public:
  explicit ThreadPool(int nbThreads = 1) {
    if (nbThreads < 1) {
      throw std::invalid_argument("number of threads should be strictly positive");
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

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  auto nbWorkers() const noexcept { return _workers.size(); }

  // add new work item to the pool
  template <class Func, class... Args>
  std::future<typename std::invoke_result<Func, Args...>::type> enqueue(Func&& f, Args&&... args);

  // Parallel version of std::transform with unary operation.
  // This function will first enqueue all the tasks at one, using waiting threads of the thread pool,
  // and then retrieves and moves the results to 'out', as for std::transform.
  template <class InputIt, class OutputIt, class UnaryOperation>
  OutputIt parallel_transform(InputIt beg, InputIt end, OutputIt out, UnaryOperation op);

  // Parallel version of std::transform with binary operation.
  template <class InputIt1, class InputIt2, class OutputIt, class BinaryOperation>
  OutputIt parallel_transform(InputIt1 first1, InputIt1 last1, InputIt2 first2, OutputIt d_first,
                              BinaryOperation binary_op);

  ~ThreadPool() {
    stopRequested();
    _condition.notify_all();
  }

 private:
  using TasksQueue = std::queue<std::function<void()>>;

  void stopRequested() {
    std::unique_lock<std::mutex> lock(_queueMutex);
    _stop = true;
  }

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

template <class Func, class... Args>
inline std::future<typename std::invoke_result<Func, Args...>::type> ThreadPool::enqueue(Func&& f, Args&&... args) {
  using return_type = typename std::invoke_result<Func, Args...>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<Func>(f), std::forward<Args>(args)...));

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

template <class InputIt, class OutputIt, class UnaryOperation>
inline OutputIt ThreadPool::parallel_transform(InputIt beg, InputIt end, OutputIt out, UnaryOperation op) {
  using FutureT = std::future<std::invoke_result_t<UnaryOperation, decltype(*beg)>>;
  SmallVector<FutureT, kTypicalNbPrivateAccounts> futures;
  for (; beg != end; ++beg) {
    futures.emplace_back(enqueue(op, *beg));
  }
  auto nbFutures = futures.size();
  for (decltype(nbFutures) runPos = 0; runPos < nbFutures; ++runPos, ++out) {
    *out = futures[runPos].get();
  }
  return out;
}

template <class InputIt1, class InputIt2, class OutputIt, class BinaryOperation>
inline OutputIt ThreadPool::parallel_transform(InputIt1 first1, InputIt1 last1, InputIt2 first2, OutputIt out,
                                               BinaryOperation binary_op) {
  using FutureT = std::future<std::invoke_result_t<BinaryOperation, decltype(*first1), decltype(*first2)>>;
  SmallVector<FutureT, kTypicalNbPrivateAccounts> futures;
  for (; first1 != last1; ++first1, ++first2) {
    futures.emplace_back(enqueue(binary_op, *first1, *first2));
  }
  auto nbFutures = futures.size();
  for (decltype(nbFutures) runPos = 0; runPos < nbFutures; ++runPos, ++out) {
    *out = futures[runPos].get();
  }
  return out;
}

}  // namespace cct