/*
 * BackendManager.cpp
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "BackendManager.h"

using namespace std;

namespace FUSESwift {

//Static members
vector<Backend*> BackendManager::list;
Backend* BackendManager::currentBackend = nullptr;

BackendManager::BackendManager() {}

BackendManager::~BackendManager() {
}

void BackendManager::registerBackend(Backend* _backend) {
  if(_backend == nullptr)
    return;
  list.push_back(_backend);
}

bool BackendManager::selectBackend(Backend* _backend) {
  if(_backend == nullptr)
    return false;
  currentBackend = _backend;
  return true;
}

bool BackendManager::selectBackend(BackendType _type) {
  for(uint i=list.size()-1;i>=0;i--)
    if(list[i]->getType() == _type) {
      currentBackend = list[i];
      return true;
    }
  return false;
}

Backend* BackendManager::getActiveBackend() {
  if(currentBackend != nullptr)
    return currentBackend;
  if(list.size() > 0) {
    selectBackend(list.back());
    return currentBackend;
  }
  else
    return nullptr;
}

} /* namespace FUSESwift */
