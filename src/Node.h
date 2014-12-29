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

#ifndef NODE_H_
#define NODE_H_
#include "Global.h"
#include <unordered_map>
#include <string>
#include <mutex>

namespace FUSESwift {

class Node;
//Types
typedef std::unordered_map<std::string,Node*> childDictionary;

class Node {
protected:
  childDictionary children;
  std::string key;
  std::mutex mapMutex;
  std::string fullPath;
  void setFullPath(std::string _fullPath);
public:

  /** Functions **/
  Node(std::string _key,std::string _fullPath);

  virtual ~Node();

  std::string getFullPath();

  /**
   * Returns a pair consisting of an iterator to the inserted element
   * (or to the element that prevented the insertion) and a bool denoting
   * whether the insertion took place.
   */
  std::pair<childDictionary::iterator, bool> childAdd(Node* _node);

  /**
   * removes the specified element and returns Number of elements removed.
   */
  long childRemove(const std::string &_key);

  /**
   *  Finds an element with key equivalent to key.
   *  returns specified element or nullptr.
   */
  Node* childFind(const std::string &_key);

  /**
   * Removes all elements from the container.
   */
  void childrenClear();

  /**
   * Returns the number of elements with key key.
   */
  size_t childrenSize();

  /**
   * Returns the maximum number of elements the container is able
   * to hold due to system or library implementation limitations.
   */
  size_t childrenMaxSize();

  /**
   * Returns an iterator to the first element of the container.
   * If the container is empty, the returned iterator will be equal to end().
   */
  childDictionary::iterator childrendBegin2();

  /**
   * Returns an iterator to the element following the last element of
   * the container. This element acts as a placeholder; attempting to
   * access it results in undefined behavior.
   */
  childDictionary::iterator childrenEnd2();

  void childrenLock();
  void childrenUnlock();
};

} /* namespace FUSESwift */
#endif /* NODE_H_ */
