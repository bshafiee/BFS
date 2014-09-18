/*
 * MemoryController.cpp
 *
 *  Created on: 2014-08-19
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "MemoryController.h"

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
  max_allowed = getTotalSystemMemory() * MAX_MEM_COEF;
  //max_allowed = 2048l*1024l*1024l;
}

bool MemoryContorller::requestMemory(ulong _size) {
  if (_size + total > max_allowed)
    return false;
  total += _size;
  return true;
}

bool MemoryContorller::checkPossibility(ulong _size) {
  if (_size + total > max_allowed)
    return false;
  return true;
}

void MemoryContorller::releaseMemory(ulong _size) {
  total -= _size;
}

long long MemoryContorller::getMaxAllowed() const {
  return max_allowed;
}

long long MemoryContorller::getTotal() const {
  return total;
}

size_t MemoryContorller::getTotalSystemMemory() {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  return pages * page_size;
}

size_t MemoryContorller::getAvailableMemory() {
  return max_allowed - total;
}

} //end of namespace FUSESWIFT
