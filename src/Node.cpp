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

#include "Node.h"

using namespace std;

namespace BFS {

Node::Node(string _key,string _fullPath):key(_key),fullPath(_fullPath) {
}

Node::~Node() {
  lock_guard<mutex> lk(mapMutex);
  //delete children
  for(auto it = children.begin();it != children.end();it++) {
    delete it->second;
    it->second = nullptr;
  }
  children.clear();
}

pair<childDictionary::iterator, bool> Node::childAdd(Node* _node) {
  lock_guard<mutex> lk(mapMutex);
  return children.insert(childDictionary::value_type(_node->key,_node));
}

long Node::childRemove(const string& _key) {
  lock_guard<mutex> lk(mapMutex);
  return children.erase(_key);
}

Node* Node::childFind(const string& _key) {
  lock_guard<mutex> lk(mapMutex);
  auto it = children.find(_key);

  if(it != children.end())
    return it->second;
  else
    return nullptr;
}

void Node::childrenClear() {
  lock_guard<mutex> lk(mapMutex);
  children.clear();
}

size_t Node::childrenSize() {
  lock_guard<mutex> lk(mapMutex);
  return children.size();
}

size_t Node::childrenMaxSize() {
  lock_guard<mutex> lk(mapMutex);
  return children.max_size();
}

childDictionary::iterator Node::childrendBegin2() {
  lock_guard<mutex> lk(mapMutex);
  return children.begin();
}

childDictionary::iterator Node::childrenEnd2() {
  lock_guard<mutex> lk(mapMutex);
  return children.end();
}

void BFS::Node::childrenLock() {
  mapMutex.lock();
}

void BFS::Node::childrenUnlock() {
  mapMutex.unlock();
}

void BFS::Node::setFullPath(std::string _fullPath) {
  fullPath = _fullPath;
}

std::string BFS::Node::getFullPath() {
  return fullPath;
}

}// End of Namespace


