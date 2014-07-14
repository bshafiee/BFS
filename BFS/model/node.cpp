/*
 * node.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "node.h"
#include "../log.h"

using namespace std;

namespace FUSESwift {

Node::Node(string _key):key(_key) {
}

Node::~Node() {
  //delete children
  for(auto it = children.begin();it != children.end();it++) {
    delete it->second;
    it->second = nullptr;
  }
}

pair<childDictionary::iterator, bool> Node::childAdd(Node* _node) {
  return children.insert(childDictionary::value_type(_node->key,_node));
}

int Node::childRemove(const string& _key) {
  return children.erase(_key);
}

Node* Node::childFind(const string& _key) {
  auto it = children.find(_key);

  if(it != children.end())
    return it->second;
  else
    return nullptr;
}

void Node::childrenClear() {
  children.clear();
}

size_t Node::childrenSize() {
  return children.size();
}

size_t Node::childrenMaxSize() {
  return children.max_size();
}

childDictionary::iterator Node::childrendBegin() {
  return children.begin();
}

childDictionary::iterator Node::childrenEnd() {
  return children.end();
}

}
