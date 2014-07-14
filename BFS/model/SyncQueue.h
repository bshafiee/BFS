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

using namespace std;

namespace FUSESwift {

class SyncQueue {
  vector<SyncEvent*> list;
  //singleton instance
  static SyncQueue *mInstance;
  //Private constructor
  SyncQueue();
public:
  static SyncQueue* getInstance();
  virtual ~SyncQueue();
  bool push(SyncEvent* _node);
  SyncEvent* pop();
  long size();
  size_t workloadSize();
};

} /* namespace FUSESwift */

#endif /* SYNCQUEUE_H_ */
