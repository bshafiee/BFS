/*
 * UploadQueue.h
 *
 *  Created on: 2014-07-17
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef UPLOADQUEUE_H_
#define UPLOADQUEUE_H_
#include "SyncQueue.h"

namespace FUSESwift {

class UploadQueue: public SyncQueue{
  //Process Events
  static void processEvent(SyncEvent* _event);
  static void syncLoop();
public:
  UploadQueue();
  virtual ~UploadQueue();
  //Start Upload Thread
  static void startSynchronization();
};

} /* namespace FUSESwift */
#endif /* UPLOADQUEUE_H_ */
