/*
 * Queue.h
 *
 *  Created on: Jan 20, 2015
 *      Author: behrooz
 */

#ifndef SRC_QUEUE_H_
#define SRC_QUEUE_H_

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif

namespace BFS {

template <typename T>
class Queue {
private:
  std::vector<T> queue;
  std::mutex m;
  std::condition_variable cond;
  std::atomic<bool>isRunning;
 public:
  Queue ():isRunning(true) {}
  T pop() {
    std::unique_lock<std::mutex> mlock(m);
    while (queue.empty()) {
      cond.wait(mlock);
      if(unlikely(!isRunning))
        return nullptr;
    }
    auto item = queue.front();
    queue.erase(queue.begin());
    return item;
  }

  T front() {
    std::unique_lock<std::mutex> mlock(m);
    while (queue.empty()) {
      cond.wait(mlock);
      if(unlikely(!isRunning))
        return nullptr;
    }
    auto item = queue.front();
    return item;
  }

  template <class... Args>
  void emplace(Args&&... args) {
    std::unique_lock<std::mutex> mlock(m);
    queue.empalce_back(std::forward<Args>(args)...);
    mlock.unlock();
    cond.notify_one();
  }

  void push(const T& item) {
    std::unique_lock<std::mutex> mlock(m);
    queue.push_back(item);
    mlock.unlock();
    cond.notify_one();
  }

  void push(T&& item) {
    std::unique_lock<std::mutex> mlock(m);
    queue.push_back(std::move(item));
    mlock.unlock();
    cond.notify_one();
  }

  inline auto at(uint64_t index) {
    return queue[index];
  }

  auto begin() const{
    return queue.begin();
  }

  auto end() const{
    return queue.end();
  }

  const auto size() {
    std::lock_guard<std::mutex> lock(m);
    return queue.size();
  }

  void stop() {
    isRunning = false;
    cond.notify_all();
  }
};


}
#endif /* SRC_QUEUE_H_ */
