/*
 * UploadQueue.cpp
 *
 *  Created on: 2014-07-17
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "DownloadQueue.h"
#include <thread>
#include "BackendManager.h"
#include "../log.h"

using namespace std;

namespace FUSESwift {

DownloadQueue::DownloadQueue() {
  // TODO Auto-generated constructor stub

}

DownloadQueue::~DownloadQueue() {
  // TODO Auto-generated destructor stub
}

void DownloadQueue::startSynchronization() {
  syncThread = new thread(syncLoop);
}

void DownloadQueue::processEvent(SyncEvent* _event) {
  Backend *backend = BackendManager::getActiveBackend();
  if(backend == nullptr) {
    log_msg("No active backend\n");
    return;
  }

  switch (_event->type) {
    case SyncEventType::DOWNLOAD_CONTENT:
      log_msg("Event:DOWNLOAD_CONTENT fullpath:%s\n",_event->fullPathBuffer.c_str());
      backend->get(_event);
      break;
    case SyncEventType::DOWNLOAD_METADATA:
      log_msg("Event:DOWNLOAD_METADATA fullpath:%s\n",_event->fullPathBuffer.c_str());
      backend->get_metadata(_event);
      break;
    default:
      log_msg("INVALID Event: file:%s TYPE:%S\n",_event->node->getFullPath().c_str(),
                SyncEvent::getEnumString(_event->type).c_str());
  }
}

void DownloadQueue::syncLoop() {
  const long maxDelay = 1000;//Milliseconds
  const long minDelay = 10;//Milliseconds
  long delay = 10;//Milliseconds

  while(true) {
    //Empty list
    if(!list.size()) {
      log_msg("DOWNLOADQUEUE: I will sleep for %zu milliseconds\n",delay);
      this_thread::sleep_for(chrono::milliseconds(delay));
      delay *= 2;
      if(delay > maxDelay)
        delay = maxDelay;
      continue;
    }
    //pop the first element and process it
    processEvent(pop());
    //reset delay
    delay = minDelay;
  }
}

} /* namespace FUSESwift */
