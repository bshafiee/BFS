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

#include <vector>
#include "Tree.h"

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
    start->childrenLock();
    auto childIterator = start->childrendBegin2();
    for(;childIterator != start->childrenEnd2();childIterator++)
      childrenQueue.push_back(childIterator->second);
    start->childrenUnlock();
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
