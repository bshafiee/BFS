/*
 * node.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "node.h"

using namespace std;

namespace FUSESwift {

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

int Node::childRemove(const string& _key) {
  lock_guard<mutex> lk(mapMutex);
  return children.erase(_key);
}

Node* Node::childFind(const string& _key) {
  lock_guard<mutex> lk(mutex);
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
  return children.begin();
}

childDictionary::iterator Node::childrenEnd2() {
  return children.end();
}

void FUSESwift::Node::childrenLock() {
  mapMutex.lock();
}

void FUSESwift::Node::childrenUnlock() {
  mapMutex.unlock();
}

}

void FUSESwift::Node::setFullPath(std::string _fullPath) {
  fullPath = _fullPath;
}

std::string FUSESwift::Node::getFullPath() {
  return fullPath;
}
