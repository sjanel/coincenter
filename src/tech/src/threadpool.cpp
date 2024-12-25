#include "threadpool.hpp"

#include <mutex>
#include <utility>

#include "cct_invalid_argument_exception.hpp"

namespace cct {

ThreadPool::ThreadPool(int nbThreads) {
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

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(_queueMutex);
    _stop = true;
  }
  _condition.notify_all();
}

}  // namespace cct