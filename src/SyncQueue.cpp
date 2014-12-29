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

#include "SyncQueue.h"
#include <cstdio>
#include <mutex>
#include "BackendManager.h"
#include "SettingManager.h"

namespace FUSESwift {

SyncQueue::SyncQueue():syncThread(nullptr) {}

SyncQueue::~SyncQueue() {
  queueMutex.lock();
  list.clear();
  queueMutex.unlock();
}

bool SyncQueue::push(SyncEvent* _event) {
  if(SettingManager::runtimeMode() == RUNTIME_MODE::STANDALONE)
    return true;

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

} /* namespace FUSESwift */
