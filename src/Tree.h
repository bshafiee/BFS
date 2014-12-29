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

#ifndef TREE_H_
#define TREE_H_
#include "Global.h"
#include "Node.h"

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
