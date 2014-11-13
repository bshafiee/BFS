/*
 * tree.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "tree.h"
#include <vector>

using namespace std;

namespace FUSESwift {

Tree::Tree(Node *_root):root(_root) {
}

Tree::~Tree() {
  if(root == nullptr)
    return;
  destroy(root);
}

size_t Tree::destroy(Node*& start) {
  //Recursive removing is dangerous, because we might run out of memory.
  vector<Node*> childrenQueue;
  size_t numOfRemovedNodes = 0;

  while(start != nullptr) {
    //add children to queue
    auto childIterator = start->childrendBegin();
    for(;childIterator != start->childrenEnd();childIterator++)
      childrenQueue.push_back(childIterator->second);
    //Now we can release start node
    delete start;
    numOfRemovedNodes++;
    if(childrenQueue.size() == 0)
      start = nullptr;
    else {
      //Now assign start to a new node in queue
      auto frontIt = childrenQueue.begin();
      start = *frontIt;
      childrenQueue.erase(frontIt);
    }
  }
  return numOfRemovedNodes;
}

}
