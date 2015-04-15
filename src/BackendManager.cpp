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

#include "BackendManager.h"

using namespace std;

namespace BFS {

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

} /* namespace BFS */
