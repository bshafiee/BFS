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
#include <Poco/RegularExpression.h>
#include "../log.h"
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "zoo/ZooHandler.h"

using namespace std;
using namespace Poco;

namespace FUSESwift {

//initialize static variables
std::string FileSystem::delimiter = "/";

FileSystem::FileSystem(FileNode* _root) :
    Tree(_root), root(_root) {
}

FileSystem::~FileSystem() {
  //recursively delete
  destroy();
}

void FileSystem::initialize(FileNode* _root) {
  root = _root;
}

FileSystem& FileSystem::getInstance() {
  static FileSystem mInstance(nullptr);
  return mInstance;
}

FileNode* FileSystem::mkFile(FileNode* _parent, const std::string &_name) {
  if (_parent == nullptr || _name.length() == 0)
    return nullptr;
  FileNode *dir = new FileNode(_name, false, _parent);
  auto res = _parent->childAdd(dir);
  if (res.second) {
  	//Inform ZooHandler about new file
		ZooHandler::getInstance().publishListOfFiles();
    return (FileNode*) (res.first->second);
  }
  else
    return nullptr;
}

FileNode* FileSystem::mkDirectory(FileNode* _parent, const std::string &_name) {
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
  //First traverse and store list of all files to be deleted by backend(bottom up!)
  //we need stack because we traverse top-down
  vector<string> fullPathStack;
  //Recursive removing is dangerous, because we might run out of memory.
  vector<FileNode*> childrenQueue;
  FileNode* temp = _node;
  while (temp != nullptr) {
    fullPathStack.push_back(temp->getFullPath());
    //add children to queue
    auto childIterator = temp->childrendBegin();
    for (; childIterator != temp->childrenEnd(); childIterator++)
      childrenQueue.push_back((FileNode*) childIterator->second);
    if (childrenQueue.size() == 0)
      temp = nullptr;
    else {
      //Now assign start to a new node in queue
      temp = childrenQueue.front();
      childrenQueue.erase(childrenQueue.begin());
    }
  }

  /**
   * Inform Download queue that these files are going to be deleted!
   * So no need to download them.
   */
  DownloadQueue::getInstance().informDeletedFiles(fullPathStack);

  //Now commit to sync queue! the order matters (before delete)
  for(int i=fullPathStack.size()-1;i>=0;i--)
    UploadQueue::getInstance().push(new SyncEvent(SyncEventType::DELETE,nullptr,fullPathStack[i]));

  //Do the actual removing on local file system
  //remove from parent
  if(_parent != nullptr) //removing the node itself
    _parent->childRemove(_node->getName());

  delete _node;//this will recursively call destructor of all kids
  _node = nullptr;

  //Inform ZooHandler about new file
	ZooHandler::getInstance().publishListOfFiles();
  return fullPathStack.size();
}

FileNode* FileSystem::searchFile(FileNode* _parent, const std::string &_name) {
  return searchNode(_parent, _name, false);
}

FileNode* FileSystem::searchDir(FileNode* _parent, const std::string &_name) {
  return searchNode(_parent, _name, true);
}

FileNode* FileSystem::traversePathToParent(const string &_path) {
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
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

std::string FileSystem::getFileNameFromPath(const std::string &_path) {
  if(_path.length() == 0)
    return "";
  if(_path.find(delimiter) == string::npos)
    return _path;
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
  return tokenizer[tokenizer.count() - 1];
}

FileNode* FileSystem::createHierarchy(const std::string &_path) {
  if(root == nullptr)
    return nullptr;
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
  string name = tokenizer[tokenizer.count() - 1];
  FileNode* start = root;
  for (uint i = 0; i < tokenizer.count() - 1; i++) {
    if (tokenizer[i].length() == 0)
      continue;
    Node* node = start->childFind(tokenizer[i]);
    if(node == nullptr)
      start = mkDirectory(start,tokenizer[i]);
    else
      start = (FileNode*) node;
  }
  return start;
}

string getNameFromPath(const string &_path) {
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
  return tokenizer[tokenizer.count() - 1];
}

FileNode* FileSystem::mkFile(const string &_path) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  return mkFile(parent, name);
}

FileNode* FileSystem::mkDirectory(const std::string &_path) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  return mkDirectory(parent, name);
}

FileNode* FileSystem::getNode(const std::string &_path) {
  //Root
  if (_path == FileSystem::delimiter)
    return root;
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
  auto it = tokenizer.begin();
  FileNode* start = root;
  for (; it != tokenizer.end(); it++) {
    if(start == nullptr)
      return nullptr;
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
  if (_path == FileSystem::delimiter)
    return root;
  string parentPath = FileSystem::delimiter;
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
  for (uint i=0; i < tokenizer.count()-1; i++) {
    if (tokenizer[i].length() == 0)
      continue;
    if(i == tokenizer.count()-2)
      parentPath += tokenizer[i];
    else
      parentPath += tokenizer[i]+FileSystem::delimiter;
  }
  //log_msg("parent path: %s\n",parentPath.c_str());
  return getNode(parentPath);
}

void FileSystem::destroy() {
  //Important, first kill download and upload thread
  DownloadQueue::getInstance().stopSynchronization();
  UploadQueue::getInstance().stopSynchronization();
  delete root;
}

bool FileSystem::tryRename(const string &_from,const string &_to) {
  FileNode* node = getNode(_from);
  if(node == nullptr)
    return false;

  if(_to.length() == 0)
    return false;

  //Get last token (name) from destination path
  StringTokenizer tokenizer(_to, FileSystem::delimiter);
  string newName = tokenizer[tokenizer.count()-1];

  //First find parent node
  FileNode* parentNode = findParent(_from);
  if(parentNode == nullptr)
    return false;

  //Ask  parent to delete this node
  bool result = parentNode->renameChild(node,newName);
  //Add syncQueue event
  if(result)
    UploadQueue::getInstance().push(new SyncEvent(SyncEventType::RENAME,node,_from));
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

std::vector<std::string>* FileSystem::listFileSystem() {
	vector<string> *output = new vector<string>();
  //Recursive removing is dangerous, because we might run out of memory.
  vector<FileNode*> childrenQueue;
  FileNode* start = root;
  while (start != nullptr) {
    //add children to queue
    auto childIterator = start->childrendBegin();
    for (; childIterator != start->childrenEnd(); childIterator++)
      childrenQueue.push_back((FileNode*) childIterator->second);
    //Now we can release start node
    if(start->getName()!="/")
    	output->push_back(start->getFullPath());

    if (childrenQueue.size() == 0)
      start = nullptr;
    else {
      //Now assign start to a new node in queue
      auto frontIt = childrenQueue.begin();
      start = *frontIt;
      childrenQueue.erase(frontIt);
    }
  }

  return output;
}

bool FileSystem::nameValidator(const std::string& _name) {
  return RegularExpression::match(_name,"^[a-zA-Z0-9äöüÄÖÜ\\.\\-_@!#\\$%\\^\\&\\*\\)\\(\\+\\|\\?<>\\[\\]\\{\\}:;'\",~]*$");
}

} // namespace

