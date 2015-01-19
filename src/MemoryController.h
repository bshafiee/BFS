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

#ifndef MEMORYCONTROLLER_H_
#define MEMORYCONTROLLER_H_
#include "Global.h"
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#include <atomic>

namespace FUSESwift {

class MemoryContorller {
  double MAX_MEM_COEF = 0;
  std::atomic<int64_t> total;
  std::atomic<int64_t> max_allowed;
  std::atomic<int64_t> claimed;
  std::atomic<int64_t> lastAvailMemory;
  MemoryContorller();
public:
  static MemoryContorller& getInstance();
  bool requestMemory(int64_t _size);
  bool claimMemory(int64_t _size);
  void releaseClaimedMemory(int64_t _size);
  bool checkPossibility(int64_t _size);
  void releaseMemory(int64_t _size);
  void processMemUsage(double& _vmUsage, double& _residentSet);
  int64_t getTotalSystemMemory();
  int64_t getMaxAllowed() const;
  int64_t getTotal() const;
  int64_t getAvailableMemory();
  int64_t getClaimedMemory();
  /**
   * @return [0..1] % of utilization
   */
  double getMemoryUtilization();
  void informMemoryUsage();
};

} //end of namespace FUSESWIFT

#endif /* MEMORYCONTROLLER_H_ */
