/*
 * node.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef NODE_H_
#define NODE_H_

#include <unordered_map>
#include <string>
#include <mutex>

namespace FUSESwift {

class Node;
//Types
typedef std::unordered_map<std::string, Node*> childDictionary;

class Node {
protected:
  childDictionary children;
  std::string key;
  Node* parent;
  std::mutex mapMutex;
public:

  /** Functions **/
  Node(std::string _key, Node* _parent);

  virtual ~Node();

  /**
   * Returns a pair consisting of an iterator to the inserted element
   * (or to the element that prevented the insertion) and a bool denoting
   * whether the insertion took place.
   */
  std::pair<childDictionary::iterator, bool> childAdd(Node* _node);

  /**
   * removes the specified element and returns Number of elements removed.
   */
  int childRemove(const std::string &_key);

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

  Node* getParent();

  void childrenLock();
  void childrenUnlock();
};

} /* namespace FUSESwift */
#endif /* NODE_H_ */
