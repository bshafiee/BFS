/*
 * tree.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef TREE_H_
#define TREE_H_

#include "node.h"

namespace FUSESwift {

class Tree {
  Node *root;
public:
  Tree(Node* _root);
  virtual ~Tree();
  /**
   * clears Tree from specified start node
   * returns number of removed nodes.
   */
  size_t destroy(Node* &start);
};

} /* namespace FUSESwift */

#endif /* TREE_H_ */
