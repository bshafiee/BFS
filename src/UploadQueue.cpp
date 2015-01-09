/**********************************************************************
Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
This program comes with ABSOLUTELY NO WARRANTY;

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#include "UploadQueue.h"
#include <thread>
#include "BackendManager.h"
#include "MemoryController.h"
#include "LoggerInclude.h"

using namespace std;

namespace FUSESwift {

UploadQueue::UploadQueue():SyncQueue() {
  // TODO Auto-generated constructor stub

}

UploadQueue::~UploadQueue() {
}

UploadQueue& UploadQueue::getInstance() {
  static UploadQueue mInstance;
  return mInstance;
}

void UploadQueue::syncLoopWrapper() {
  UploadQueue::getInstance().syncLoop();
}

void UploadQueue::startSynchronization() {
  running = true;
  syncThread = new thread(syncLoopWrapper);
}

void UploadQueue::stopSynchronization() {
  running = false;
}

void UploadQueue::processEvent(const SyncEvent* _event) {
  Backend *backend = BackendManager::getActiveBackend();
  if(backend == nullptr) {
    LOG(ERROR)<<"No active backend";
    return;
  }
  //accessing _event->node is dangerous it may be deleted from main thread!
  switch (_event->type) {
    case SyncEventType::DELETE:
      if(!checkEventValidity(*_event)) break;
      LOG(DEBUG)<<"Event:DELETE fullpath:"<<_event->fullPathBuffer;
      backend->remove(_event);
      break;
    case SyncEventType::RENAME:
      if(!checkEventValidity(*_event)) break;
      //LOG(DEBUG)<<"Event:RENAME from:"<<_event->fullPathBuffer<<" to:"<<_event->node->getFullPath();
      backend->move(_event);
      break;
    case SyncEventType::UPDATE_CONTENT:
      if(!checkEventValidity(*_event)) break;
      //log_msg("Event:UPDATE_CONTENT file:%s\n",_event->node->getFullPath().c_str());
      backend->put(_event);
      break;
    case SyncEventType::UPDATE_METADATA:
      if(!checkEventValidity(*_event)) break;
      //log_msg("Event:UPDATE_METADATA file:%s\n",_event->node->getFullPath().c_str());
      backend->put_metadata(_event);
      break;
    default:
      LOG(ERROR)<<"INVALID Event: file:"<<_event->fullPathBuffer<<" TYPE:"<<
      SyncEvent::getEnumString(_event->type);
  }
}

void UploadQueue::syncLoop() {
  const long maxDelay = 5000;//Milliseconds
  const long minDelay = 10;//Milliseconds
  long delay = 10;//Milliseconds

  while(running) {
    //Empty list
    if(!list.size()) {
      //log_msg("UPLOADQUEUE: I will sleep for %zu milliseconds\n",delay);
      this_thread::sleep_for(chrono::milliseconds(delay));
      delay *= 2;
      if(delay > maxDelay)
        delay = maxDelay;
      //cout << "TOTAL USED: "<<MemoryContorller::getInstance().getTotal()/1024/1024 << " MB" << endl;
      continue;
    }
    //pop the first element and process it
    SyncEvent* event = pop();
    processEvent(event);
    //do cleanup! delete event
    if(event != nullptr)
      delete event;
    event = nullptr;
    //reset delay
    delay = minDelay;
    //cout << "TOTAL USED: "<<MemoryContorller::getInstance().getTotal()/1024/1024 << " MB" << endl;
  }
}

bool UploadQueue::checkEventValidity(const SyncEvent& _event) {
  lock_guard<std::mutex> lock(queueMutex);
  switch (_event.type) {
    case SyncEventType::DELETE:
      return true;
    case SyncEventType::RENAME:
      //Check if there is another delete or update in the queue coming
      for(uint i=0;i<list.size();i++) {//TODO there is a bug here! (probabaly concurrent push or pop is killing it! :()
        SyncEvent *upcomingEvent = list[i];
        if(upcomingEvent->type == SyncEventType::DELETE)
          if(upcomingEvent->fullPathBuffer == _event.fullPathBuffer) {
            //LOG(INFO)<<("SAVED a RENAME OPERATION FullBuffer:%s\n",_event.fullPathBuffer.c_str());
            return false;
          }

      }
      return true;
    case SyncEventType::UPDATE_CONTENT:
      //Check if there is another delete or update in the queue coming
      for(uint i=0;i<list.size();i++) {
        SyncEvent *upcomingEvent = list[i];
        if(upcomingEvent->type == SyncEventType::DELETE)
          if(upcomingEvent->fullPathBuffer == _event.fullPathBuffer) {
            //printf("SAVED a UPLOAD OPERATION FullBuffer:%s\n",_event.fullPathBuffer.c_str());
            return false;
          }
      }
      return true;
    case SyncEventType::UPDATE_METADATA:
      //Check if there is a delete event coming in the queu
    default:
      return true;
  }
}

} /* namespace FUSESwift */
