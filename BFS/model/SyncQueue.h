/*
 * SyncQueue.h
 *
 *  Created on: Jul 12, 2014
 *      Author: behrooz
 */

#ifndef SYNCQUEUE_H_
#define SYNCQUEUE_H_

#include <vector>
#include "filenode.h"
#include "syncEvent.h"
#include <mutex>
#include <thread>

using namespace std;

namespace FUSESwift {

class SyncQueue {
protected:
  static vector<SyncEvent*> list;
  //Mutex to protect queue
  static std::mutex mutex;
  //Thread to run syncLoop
  static std::thread *syncThread;
  //Private constructor
  SyncQueue();
public:
  virtual ~SyncQueue();
  static bool push(SyncEvent* _node);
  static SyncEvent* pop();
  static long size();
  static size_t workloadSize();
};

} /* namespace FUSESwift */

#endif /* SYNCQUEUE_H_ */
