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
  //Start/stop Upload Thread
  void startSynchronization();
  void stopSynchronization();
  //Check if this event is still valid
  /**
   * returns true in case the event should be performed
   */
  bool checkEventValidity(const SyncEvent& _event);
};

} /* namespace FUSESwift */
#endif /* UPLOADQUEUE_H_ */
