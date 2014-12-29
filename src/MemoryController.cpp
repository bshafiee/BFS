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

#include "MemoryController.h"
#include "SettingManager.h"
#include "LoggerInclude.h"

namespace FUSESwift {

MemoryContorller& MemoryContorller::getInstance() {
  //Static members
  static MemoryContorller instance;
  return instance;
}

//////////////////////////////////////////////////////////////////////////////
//
// process_mem_usage(double &, double &) - takes two doubles by reference,
// attempts to read the system-dependent data for a process' virtual memory
// size and resident set size, and return the results in KB.
//
// On failure, returns 0.0, 0.0
// Source: taken from stackoverflow:
// http://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-run-time-in-c
void MemoryContorller::processMemUsage(double& vm_usage,
    double& resident_set) {
  using std::ios_base;
  using std::ifstream;
  using std::string;

  vm_usage = 0.0;
  resident_set = 0.0;

  // 'file' stat seems to give the most reliable results
  //
  ifstream stat_stream("/proc/self/stat", ios_base::in);

  // dummy vars for leading entries in stat that we don't care about
  //
  string pid, comm, state, ppid, pgrp, session, tty_nr;
  string tpgid, flags, minflt, cminflt, majflt, cmajflt;
  string utime, stime, cutime, cstime, priority, nice;
  string O, itrealvalue, starttime;

  // the two fields we want
  //
  unsigned long vsize;
  long rss;

  stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
      >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime
      >> stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue
      >> starttime >> vsize >> rss; // don't care about the rest

  stat_stream.close();

  long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
  vm_usage = vsize / 1024.0;
  resident_set = rss * page_size_kb;
}

MemoryContorller::MemoryContorller():total(0),max_allowed(0) {
  double coef = SettingManager::getDouble(CONFIG_KEY_MAX_MEM_COEF);
  if(coef > 0 && coef <=1)
  	MAX_MEM_COEF = coef;
  else
  	LOG(ERROR)<<"Invalid MAX_MEM_COEF:"<<coef;

	max_allowed = getTotalSystemMemory() * MAX_MEM_COEF;
  //max_allowed = 512l*1024l*1024l;
}

bool MemoryContorller::requestMemory(int64_t _size) {
  if (_size + total > max_allowed)
    return false;
  total += _size;
  return true;
}

bool MemoryContorller::checkPossibility(int64_t _size) {
  if (_size + total > max_allowed)
    return false;
  return true;
}

void MemoryContorller::releaseMemory(int64_t _size) {
  total -= _size;
}

int64_t MemoryContorller::getMaxAllowed() const {
  return max_allowed;
}

int64_t MemoryContorller::getTotal() const {
  return total;
}

int64_t MemoryContorller::getTotalSystemMemory() {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  return pages * page_size;
}

int64_t MemoryContorller::getAvailableMemory() {
  return max_allowed - total;
}

double MemoryContorller::getMemoryUtilization() {
  double used = getMaxAllowed() - getAvailableMemory();
  return used/(double)getMaxAllowed();
}

} //end of namespace FUSESWIFT

