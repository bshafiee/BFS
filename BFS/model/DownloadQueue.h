/*
 * DownlaodQueue.h
 *
 *  Created on: 2014-07-17
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef DOWNLOADQUEUE_H_
#define DOWNLOADQUEUE_H_
#include "SyncQueue.h"

namespace FUSESwift {

class DownloadQueue: public SyncQueue{
  //Process Events
  static void processEvent(SyncEvent* &_event);
  static void syncLoop();
  static void updateFromBackend();
  static void processDownloadContent(SyncEvent* _event);
  static void processDownloadMetadata(SyncEvent* _event);
public:
  DownloadQueue();
  virtual ~DownloadQueue();
  //Start Downlaod Thread
  static void startSynchronization();
};

} /* namespace FUSESwift */
#endif /* DOWNLOADQUEUE_H_ */
