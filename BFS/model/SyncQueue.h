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
#include <atomic>

using namespace std;

namespace FUSESwift {

class SyncQueue{
protected:
  //atomic stop/start condition
  atomic<bool> running;
  vector<SyncEvent*> list;
  //Mutex to protect queue
  std::mutex queueMutex;
  //Thread to run syncLoop
  std::thread *syncThread;
  //Private constructor
  SyncQueue();
  //Protected virtual methods
  virtual void processEvent(const SyncEvent* _event) = 0;
  virtual void syncLoop() = 0;
public:
  virtual ~SyncQueue();
  bool push(SyncEvent* _node);
  SyncEvent* pop();
  long size();
  size_t workloadSize();
  virtual void startSynchronization() = 0;
  virtual void stopSynchronization() = 0;
  inline bool containsEvent(const SyncEvent* _event);
};

} /* namespace FUSESwift */

#endif /* SYNCQUEUE_H_ */
