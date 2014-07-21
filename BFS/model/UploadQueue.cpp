/*
 * UploadQueue.cpp
 *
 *  Created on: 2014-07-17
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "UploadQueue.h"
#include <thread>
#include "BackendManager.h"
#include "../log.h"

using namespace std;

namespace FUSESwift {

//Static members
UploadQueue* UploadQueue::mInstance = new UploadQueue();

UploadQueue::UploadQueue():SyncQueue() {
  // TODO Auto-generated constructor stub

}

UploadQueue::~UploadQueue() {
  // TODO Auto-generated destructor stub
}

UploadQueue* UploadQueue::getInstance() {
  return mInstance;
}

void UploadQueue::syncLoopWrapper() {
  UploadQueue::getInstance()->syncLoop();
}

void UploadQueue::startSynchronization() {
  syncThread = new thread(syncLoopWrapper);
}

void UploadQueue::processEvent(SyncEvent* &_event) {
  Backend *backend = BackendManager::getActiveBackend();
  if(backend == nullptr) {
    log_msg("No active backend\n");
    return;
  }

  switch (_event->type) {
    case SyncEventType::DELETE:
      log_msg("Event:DELETE fullpath:%s\n",_event->fullPathBuffer.c_str());
      backend->remove(_event);
      break;
    case SyncEventType::RENAME:
      log_msg("Event:RENAME from:%s to:%s\n",_event->fullPathBuffer.c_str(),_event->node->getFullPath().c_str());
      backend->move(_event);
      break;
    case SyncEventType::UPDATE_CONTENT:
      log_msg("Event:UPDATE_CONTENT file:%s\n",_event->node->getFullPath().c_str());
      backend->put(_event);
      break;
    case SyncEventType::UPDATE_METADATA:
      log_msg("Event:UPDATE_METADATA file:%s\n",_event->node->getFullPath().c_str());
      backend->put_metadata(_event);
      break;
    default:
      log_msg("INVALID Event: file:%s TYPE:%S\n",_event->node->getFullPath().c_str(),
          SyncEvent::getEnumString(_event->type).c_str());
  }
  //do cleanup! delete event
  if(_event != nullptr)
		delete _event;
  _event = nullptr;
}

void UploadQueue::syncLoop() {
  const long maxDelay = 1000;//Milliseconds
  const long minDelay = 10;//Milliseconds
  long delay = 10;//Milliseconds

  while(true) {
    //Empty list
    if(!list.size()) {
      //log_msg("UPLOADQUEUE: I will sleep for %zu milliseconds\n",delay);
      this_thread::sleep_for(chrono::milliseconds(delay));
      delay *= 2;
      if(delay > maxDelay)
        delay = maxDelay;
      continue;
    }
    //pop the first element and process it
    SyncEvent* event = pop();
    processEvent(event);
    //reset delay
    delay = minDelay;
  }
}

} /* namespace FUSESwift */
