/*
 * Timer.h
 *
 *  Created on: Nov 13, 2014
 *      Author: Behrooz
 */

#include <chrono>

#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

namespace FUSESwift {

class Timer {
  std::chrono::steady_clock::time_point start;
  std::chrono::steady_clock::time_point stop;
public:
  Timer(){
    start = std::chrono::steady_clock::now();
    stop = std::chrono::steady_clock::now();
  }
  virtual ~Timer() {}

  inline void begin() {
    start = std::chrono::steady_clock::now();
  }

  inline void end() {
    stop = std::chrono::steady_clock::now();
  }

  inline double elapsedMillis() {
    auto diff = stop - start;
    return  std::chrono::duration<double, std::milli> (diff).count();
  }

  inline double elapsedMicro() {
    auto diff = stop - start;
    return  std::chrono::duration<double, std::micro> (diff).count();
  }

  inline double elapsedNano() {
    auto diff = stop - start;
    return  std::chrono::duration<double, std::nano> (diff).count();
  }

  inline double elapsedSec() {
    auto diff = stop - start;
    return std::chrono::duration<double> (diff).count();
  }
};

} /* namespace FUSESwift */

#endif /* SRC_TIMER_H_ */
