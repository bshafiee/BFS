/**********************************************************************
 Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
 This program comes with ABSOLUTELY NO WARRANTY;

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "Global.h"
#include <chrono>
#include "LoggerInclude.h"
#include <mutex>
#include <condition_variable>
#include <iostream>

#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

namespace BFS {

struct cancelled_error {};
class Later {
public:
  Later(uint _after, std::string _msg) :
      cancel(false), done(false), msg(_msg), after(_after) {
    start();
  }

  ~Later() {
    stop();
    delete thread;
  }

  void stop() {
    if(done)
      return;
    cancel.store(true);
    cv.notify_all();
    thread->join();
  }
private:
  void start() {
    thread = new std::thread(std::bind(&Later::run, this));
  }

  void run() {
    if(cancel){
      done.store(true);
      return;
    }
    std::unique_lock<std::mutex> lk(m);
    cv.wait_for(lk,std::chrono::milliseconds(after));

    if(!cancel)
      LOG(ERROR)<<"LATER HAPPENED:"<<msg;
    done.store(true);
  }
  std::thread* thread;
  std::mutex m;
  std::condition_variable cv;
  std::atomic<bool> cancel;
  std::atomic<bool> done;
  std::string msg;
  uint after;
};

class Timer {
  std::chrono::steady_clock::time_point start;
  std::chrono::steady_clock::time_point stop;
public:
  Timer() {
    start = std::chrono::steady_clock::now();
    stop = std::chrono::steady_clock::now();
  }
  virtual ~Timer() {
  }

  inline void begin() {
    start = std::chrono::steady_clock::now();
  }

  inline void end() {
    stop = std::chrono::steady_clock::now();
  }

  inline double elapsedMillis() {
    auto diff = stop - start;
    return std::chrono::duration<double, std::milli>(diff).count();
  }

  inline double elapsedMicro() {
    auto diff = stop - start;
    return std::chrono::duration<double, std::micro>(diff).count();
  }

  inline double elapsedNano() {
    auto diff = stop - start;
    return std::chrono::duration<double, std::nano>(diff).count();
  }

  inline double elapsedSec() {
    auto diff = stop - start;
    return std::chrono::duration<double>(diff).count();
  }
};

} /* namespace BFS */

#endif /* SRC_TIMER_H_ */
