/*
 * MemoryController.h
 *
 *  Created on: 2014-08-19
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef MEMORYCONTROLLER_H_
#define MEMORYCONTROLLER_H_

#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>

namespace FUSESwift {

class MemoryContorller {
  double MAX_MEM_COEF = 0;
  int64_t total;
  int64_t max_allowed;
  MemoryContorller();
public:
  static MemoryContorller& getInstance();
  bool requestMemory(int64_t _size);
  bool checkPossibility(int64_t _size);
  void releaseMemory(int64_t _size);
  void processMemUsage(double& _vmUsage, double& _residentSet);
  int64_t getTotalSystemMemory();
  int64_t getMaxAllowed() const;
  int64_t getTotal() const;
  int64_t getAvailableMemory();
  /**
   * @return [0..1] % of utilization
   */
  double getMemoryUtilization();
};

} //end of namespace FUSESWIFT

#endif /* MEMORYCONTROLLER_H_ */
