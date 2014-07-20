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
  //Singleton instance
  static UploadQueue *mInstance;
  //Process Events
  void processEvent(SyncEvent* &_event);
  static void syncLoopWrapper();
  void syncLoop();
  //Private Constructor
  UploadQueue();
public:
  static UploadQueue* getInstance();
  virtual ~UploadQueue();
  //Start Upload Thread
  void startSynchronization();
};

} /* namespace FUSESwift */
#endif /* UPLOADQUEUE_H_ */
