/*
 * filesystem.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "filesystem.h"
#include "filenode.h"
#include <vector>
#include <iostream>
#include <Poco/StringTokenizer.h>
#include "../log.h"
#include "UploadQueue.h"

using namespace std;
using namespace Poco;

namespace FUSESwift {

//initialize static variables
FileSystem* FileSystem::mInstance = new FileSystem(nullptr);

FileSystem::FileSystem(FileNode* _root) :
    Tree(_root), root(_root) {
}

FileSystem::~FileSystem() {
  //recursively delete
  destroy();
}

void FileSystem::initialize(FileNode* _root) {
  root = _root;
	int err = pthread_mutex_init(&mutex, NULL);
	if (err)
		log_msg("\npthread_mutex_init failed.\n");
}

FileSystem* FileSystem::getInstance() {
  return mInstance;
}

FileNode* FileSystem::mkFile(FileNode* _parent, std::string _name) {
  if (_parent == nullptr || _name.length() == 0)
    return nullptr;
  FileNode *dir = new FileNode(_name, false, _parent);
  auto res = _parent->childAdd(dir);
  if (res.second)
    return (FileNode*) (res.first->second);
  else
    return nullptr;
}

FileNode* FileSystem::mkDirectory(FileNode* _parent, std::string _name) {
  if (_parent == nullptr || _name.length() == 0)
    return nullptr;
  FileNode *dir = new FileNode(_name, true, _parent);
  auto res = _parent->childAdd(dir);
  if (res.second)
    return (FileNode*) (res.first->second);
  else
    return nullptr;
}

FileNode* FileSystem::searchNode(FileNode* _parent, std::string _name,
    bool _isDir) {
  //Recursive search is dangerous, we might cause stack overflow
  vector<FileNode*> childrenQueue;

  while (_parent != nullptr) {
    if (_parent->getName() == _name && _parent->isDirectory() == _isDir)
      return _parent;
    //add children to queue
    auto childIterator = _parent->childrendBegin();
    for (; childIterator != _parent->childrenEnd(); childIterator++)
      childrenQueue.push_back((FileNode*) childIterator->second);

    if (childrenQueue.size() == 0)
      return nullptr;
    else {
      //Now assign start to a new node in queue
      auto frontIt = childrenQueue.begin();
      _parent = *frontIt;
      childrenQueue.erase(frontIt);
    }
  }
  return nullptr;
}

size_t FileSystem::rmNode(FileNode* &_parent, FileNode* &_node) {
  //Recursive removing is dangerous, because we might run out of memory.
  vector<FileNode*> childrenQueue;
  size_t numOfRemovedNodes = 0;
  //remove from parent
  if(_parent != nullptr) { //removing the node itself
    _parent->childRemove(_node->getName());
    //Now commit to sync queue! the order matters (before delete)
    UploadQueue::push(new SyncEvent(SyncEventType::DELETE,_node,_node->getFullPath()));
  }

  while (_node != nullptr) {
    //add children to queue
    auto childIterator = _node->childrendBegin();
    for (; childIterator != _node->childrenEnd(); childIterator++)
      childrenQueue.push_back((FileNode*) childIterator->second);
    //Now we can release start node
    delete _node;
    numOfRemovedNodes++;
    if (childrenQueue.size() == 0)
      _node = nullptr;
    else {
      //Now assign start to a new node in queue
      auto frontIt = childrenQueue.begin();
      _node = *frontIt;
      childrenQueue.erase(frontIt);
      //Now commit to sync queue! the order matters (before delete)
      UploadQueue::push(new SyncEvent(SyncEventType::DELETE,_node,_node->getFullPath()));
    }
  }

  return numOfRemovedNodes;
}

FileNode* FileSystem::searchFile(FileNode* _parent, std::string _name) {
  return searchNode(_parent, _name, false);
}

FileNode* FileSystem::searchDir(FileNode* _parent, std::string _name) {
  return searchNode(_parent, _name, true);
}

std::string FileSystem::getDelimiter() {
  return delimiter;
}



FileNode* FileSystem::traversePathToParent(string _path) {
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, "/");
  string name = tokenizer[tokenizer.count() - 1];
  FileNode* start = root;
  for (uint i = 0; i < tokenizer.count() - 1; i++) {
    if (tokenizer[i].length() == 0)
      continue;
    Node* node = start->childFind(tokenizer[i]);
    start = (node == nullptr) ? nullptr : (FileNode*) node;
  }
  return start;
}

string getNameFromPath(string _path) {
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, "/");
  return tokenizer[tokenizer.count() - 1];
}

FileNode* FileSystem::mkFile(string _path) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  return mkFile(parent, name);
}

FileNode* FileSystem::mkDirectory(std::string _path) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  return mkDirectory(parent, name);
}

FileNode* FileSystem::getNode(std::string _path) {
  //Root
  if (_path == "/")
    return root;
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, "/");
  auto it = tokenizer.begin();
  FileNode* start = root;
  for (; it != tokenizer.end(); it++) {
    if (it->length() == 0)
      continue;
    //log_msg("inja: %s\tstart: %s\n",(*it).c_str(),start->getName().c_str());
    Node* node = start->childFind(*it);
    start = (node == nullptr) ? nullptr : (FileNode*) node;
  }
  return start;
}

FileNode* FileSystem::findParent(const string &_path) {
  vector<FileNode*> childrenQueue;
  //Root
  if (_path == "/")
    return root;
  string parentPath = "/";
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, "/");
  for (uint i=0; i < tokenizer.count()-1; i++) {
    if (tokenizer[i].length() == 0)
      continue;
    if(i == tokenizer.count()-2)
      parentPath += tokenizer[i];
    else
      parentPath += tokenizer[i]+"/";
  }
  log_msg("parent path: %s\n",parentPath.c_str());
  return getNode(parentPath);
}

void FileSystem::destroy() {
  FileNode* nullNode = nullptr;
  rmNode(nullNode, root);
}

bool FileSystem::tryRename(const string &_from,const string &_to) {
  FileNode* node = getNode(_from);
  if(node == nullptr)
    return false;

  if(_to.length() == 0)
    return false;

  //Get last token (name) from destination path
  StringTokenizer tokenizer(_to, "/");
  string newName = tokenizer[tokenizer.count()-1];

  //First find parent node
  FileNode* parentNode = findParent(_from);
  if(parentNode == nullptr)
    return false;

  //Ask  parent to delete this node
  bool result = parentNode->renameChild(node,newName);
  //Add syncQueue event
  if(result)
    UploadQueue::push(new SyncEvent(SyncEventType::RENAME,node,_from));
  return result;
}

std::string FileSystem::printFileSystem() {
  string output = "";
  //Recursive removing is dangerous, because we might run out of memory.
  vector<FileNode*> childrenQueue;
  FileNode* start = root;
  int newLineCounter = 1;
  log_msg("FileSystem:\n\n");
  while (start != nullptr) {
    //add children to queue
    auto childIterator = start->childrendBegin();
    for (; childIterator != start->childrenEnd(); childIterator++)
      childrenQueue.push_back((FileNode*) childIterator->second);
    //Now we can release start node
    output += start->getName()+" ";
    newLineCounter--;

    if(!newLineCounter) {
      output += "\n";
      newLineCounter = start->childrenSize();
    }

    if (childrenQueue.size() == 0)
      start = nullptr;
    else {
      //Now assign start to a new node in queue
      auto frontIt = childrenQueue.begin();
      start = *frontIt;
      childrenQueue.erase(frontIt);
    }
  }
  log_msg("%s\n\n",output.c_str());
  return output;
}

void FileSystem::lock() {
	pthread_mutex_lock(&mutex);
}

void FileSystem::unlock() {
	pthread_mutex_unlock(&mutex);
}

} // namespace

