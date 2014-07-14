/*
 * SyncQueue.cpp
 *
 *  Created on: Jul 12, 2014
 *      Author: behrooz
 */

#include "SyncQueue.h"

namespace FUSESwift {

//Static members initialization
SyncQueue* SyncQueue::mInstance = new SyncQueue();

SyncQueue::SyncQueue() {
  // TODO Auto-generated constructor stub

}

SyncQueue::~SyncQueue() {
  // TODO Auto-generated destructor stub
}

bool SyncQueue::push(SyncEvent* _node) {
  if(_node == nullptr)
    return false;
  bool exist = false;
  for(auto it = list.begin(); it != list.end();++it)
    if(*it == _node) {
      exist = true;
      break;
    }
  if(!exist)
    this->list.push_back(_node);
  return true;
}

SyncEvent* SyncQueue::pop() {
  if(list.size() == 0)
    return nullptr;
  //First element
  SyncEvent* firstElem = list.front();
  //Now we can remove last element
  list.erase(list.begin());
  return firstElem;
}

long SyncQueue::size() {
  return list.size();
}

SyncQueue* SyncQueue::getInstance() {
  return mInstance;
}

size_t SyncQueue::workloadSize() {
  size_t total = 0;
  for(auto it = list.begin(); it != list.end();++it)
    total += (*it)->node->getSize();
  return total;
}

} /* namespace FUSESwift */
