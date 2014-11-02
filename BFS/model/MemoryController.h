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
  long long total;
  unsigned long long max_allowed;
  MemoryContorller();
public:
  static MemoryContorller& getInstance();
  bool requestMemory(ulong _size);
  bool checkPossibility(ulong _size);
  void releaseMemory(ulong _size);
  void processMemUsage(double& _vmUsage, double& _residentSet);
  size_t getTotalSystemMemory();
  long long getMaxAllowed() const;
  long long getTotal() const;
  size_t getAvailableMemory();
};

} //end of namespace FUSESWIFT

#endif /* MEMORYCONTROLLER_H_ */
