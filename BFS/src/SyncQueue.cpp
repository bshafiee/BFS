/*
 * SyncQueue.cpp
 *
 *  Created on: Jul 12, 2014
 *      Author: behrooz
 */

#include "SyncQueue.h"
#include "log.h"
#include <cstdio>
#include <mutex>
#include "BackendManager.h"

namespace FUSESwift {

SyncQueue::SyncQueue():syncThread(nullptr) {}

SyncQueue::~SyncQueue() {
  queueMutex.lock();
  list.clear();
  queueMutex.unlock();
}

bool SyncQueue::push(SyncEvent* _event) {
	lock_guard<std::mutex> lock(queueMutex);

  if(_event == nullptr)
    return false;

  bool exist = false;
  for(auto it = list.begin(); it != list.end();++it)
    if(*(*it) == *_event) {
      exist = true;
      break;
    }
  if(!exist)
    list.push_back(_event);
  return true;
}

SyncEvent* SyncQueue::pop() {
  if(list.size() == 0)
    return nullptr;
  queueMutex.lock();
  //First element
  SyncEvent* firstElem = list.front();
  //Now we can remove front element
  list.erase(list.begin());
  queueMutex.unlock();
  return firstElem;
}

bool SyncQueue::containsEvent(const SyncEvent* _event) {
  if(_event == nullptr || list.size() == 0)
    return false;
  for(auto it = list.begin(); it != list.end();++it)
      if (*(*it) == *_event)
        return true;
  return false;
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

} /* namespace FUSESwift */
