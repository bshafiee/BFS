/*
 * SyncQueue.cpp
 *
 *  Created on: Jul 12, 2014
 *      Author: behrooz
 */

#include "SyncQueue.h"
#include "../log.h"
#include <cstdio>

namespace FUSESwift {

//static members
vector<SyncEvent*> SyncQueue::list;
mutex SyncQueue::mutex;
thread *SyncQueue::syncThread = nullptr;

SyncQueue::SyncQueue() {}

SyncQueue::~SyncQueue() {
  list.clear();
}

bool SyncQueue::push(SyncEvent* _event) {
  if(_event == nullptr)
    return false;
  if(_event->node == nullptr)
    return false;


  bool exist = false;
  for(auto it = list.begin(); it != list.end();++it)
    if(*(*it) == *_event) {
      exist = true;
      break;
    }
  if(!exist) {
    mutex.lock();
    list.push_back(_event);
    mutex.unlock();
  }
  return true;
}

SyncEvent* SyncQueue::pop() {
  if(list.size() == 0)
    return nullptr;
  mutex.lock();
  //First element
  SyncEvent* firstElem = list.front();
  //Now we can remove last element
  list.erase(list.begin());
  mutex.unlock();
  return firstElem;
}

long SyncQueue::size() {
  return list.size();
}

size_t SyncQueue::workloadSize() {
  size_t total = 0;
  for(auto it = list.begin(); it != list.end();++it)
    total += (*it)->node->getSize();
  return total;
}

void SyncQueue::processEvent(SyncEvent* _event) {
  switch (_event->type) {
    case SyncEventType::DELETE:
      log_msg("Event:DELETE fullpath:%s\n",_event->fullPathBuffer.c_str());
      break;
    case SyncEventType::RENAME:
      log_msg("Event:RENAME from:%s to:%s\n",_event->fullPathBuffer.c_str(),_event->node->getFullPath().c_str());
          break;
    case SyncEventType::UPDATE_CONTENT:
      log_msg("Event:UPDATE_CONTENT file:%s\n",_event->node->getFullPath().c_str());
          break;
    case SyncEventType::UPDATE_METADATA:
      log_msg("Event:UPDATE_METADATA file:%s\n",_event->node->getFullPath().c_str());
          break;
    default:
      log_msg("Event:UNKNOWN file:%s\n",_event->node->getFullPath().c_str());
      ;
  }
}

void SyncQueue::syncLoop() {
  const long maxDelay = 1000;//Milliseconds
  const long minDelay = 10;//Milliseconds
  long delay = 10;//Milliseconds

  while(true) {
    //Empty list
    if(!list.size()) {
      log_msg("I will sleep for %zu milliseconds\n",delay);
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

void SyncQueue::startSyncThread() {
  syncThread = new thread(syncLoop);
}

} /* namespace FUSESwift */
