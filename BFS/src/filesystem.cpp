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
#include "log.h"
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "ZooHandler.h"

using namespace std;
using namespace Poco;

namespace FUSESwift {

//initialize static variables
std::string FileSystem::delimiter = "/";

FileSystem::FileSystem(FileNode* _root) :
    Tree(_root), root(_root), inodeCounter(1) {
}

FileSystem::~FileSystem() {}

void FileSystem::initialize(FileNode* _root) {
  root = _root;
}

FileSystem& FileSystem::getInstance() {
  static FileSystem mInstance(nullptr);
  return mInstance;
}

FileNode* FileSystem::mkFile(FileNode* _parent, const std::string &_name,bool _isRemote) {
  if (_parent == nullptr || _name.length() == 0)
    return nullptr;
  FileNode *file;
  if(_parent == root)
    file = new FileNode(_name,_parent->getFullPath()+_name, false, _isRemote);
  else
    file = new FileNode(_name,_parent->getFullPath()+delimiter+_name, false, _isRemote);
  auto res = _parent->childAdd(file);
  if (res.second) {
  	//Inform ZooHandler about new file if not remote
  	if(!_isRemote)
  		ZooHandler::getInstance().publishListOfFiles();
    return (FileNode*) (res.first->second);
  }
  else
    return nullptr;
}

FileNode* FileSystem::mkDirectory(FileNode* _parent, const std::string &_name,bool _isRemote) {
  if (_parent == nullptr || _name.length() == 0)
    return nullptr;
  FileNode *dir;
  if(_parent == root)
    dir = new FileNode(_name,_parent->getFullPath()+_name ,true, _isRemote);
  else
    dir = new FileNode(_name,_parent->getFullPath()+delimiter+_name ,true, _isRemote);
  auto res = _parent->childAdd(dir);
  if (res.second)
    return (FileNode*) (res.first->second);
  else
    return nullptr;
}

FileNode* FileSystem::traversePathToParent(const string &_path) {
  //Root
  if (_path == FileSystem::delimiter)
    return nullptr;
  //Traverse FileSystem Hierarchies
  StringTokenizer tokenizer(_path, FileSystem::delimiter);
  string name = tokenizer[tokenizer.count() - 1];
  FileNode* start = root;
  for (uint i = 0; i < tokenizer.count() - 1; i++) {
    if (tokenizer[i].length() == 0)
      continue;
    Node* node = start->childFind(tokenizer[i]);
    start = (node == nullptr) ? nullptr : (FileNode*) node;
    if(start == nullptr)//parent already removed
      return nullptr;
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
      start = mkDirectory(start,tokenizer[i],false);
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

FileNode* FileSystem::mkFile(const string &_path,bool _isRemote) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  return mkFile(parent, name, _isRemote);
}

FileNode* FileSystem::mkDirectory(const std::string &_path,bool _isRemote) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  return mkDirectory(parent, name,_isRemote);
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
  return traversePathToParent(_path);
  /*vector<FileNode*> childrenQueue;
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
  return getNode(parentPath);*/
}

void FileSystem::destroy() {
  //Important, first kill download and upload thread
  DownloadQueue::getInstance().stopSynchronization();
  UploadQueue::getInstance().stopSynchronization();

  if(root)
    root->signalDelete(false);
  root = nullptr;
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
    start->childrenLock();
    auto childIterator = start->childrendBegin2();
    for (; childIterator != start->childrenEnd2(); childIterator++){
      if(((FileNode*)(childIterator->second))->mustBeDeleted())
        continue;
      childrenQueue.push_back((FileNode*) childIterator->second);
    }
    start->childrenUnlock();
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

std::vector<std::string> FileSystem::listFileSystem(bool _includeRemotes) {
	vector<string> output;
  //Recursive is dangerous, because we might run out of memory.
  vector<FileNode*> childrenQueue;
  FileNode* start = root;
  while (start != nullptr) {
    //add children to queue
    start->childrenLock();
    auto childIterator = start->childrendBegin2();
    for (; childIterator != start->childrenEnd2(); childIterator++){
    	FileNode* fileNode = (FileNode*) childIterator->second;

    	if(fileNode->mustBeDeleted())
    	  continue;

    	if(!fileNode->isRemote())//If not remote we will claim we have this File
    	  childrenQueue.push_back(fileNode);
    	else if(_includeRemotes && fileNode->isRemote())
        childrenQueue.push_back(fileNode);
    }
    start->childrenUnlock();
    //Now we can release start node
    if(start->getName()!="/" && !start->isDirectory() && !start->mustBeDeleted())//if not a directory
    	output.push_back(start->getFullPath());

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
  return RegularExpression::match(_name,"^[a-zA-Z0-9äöüÄÖÜ\\.\\-_@!#\\$%\\^\\&\\*\\)\\(\\+\\|\\?<>\\[\\]\\{\\}:;'\",~ ]*$");
	//return true;
}

bool FileSystem::createRemoteFile(const std::string& _name) {
  ZooNode mostFreeNode = ZooHandler::getInstance().getMostFreeNode();
  if(mostFreeNode.freeSpace == 0)//GetMostFreeNode could not find a node
    return false;
  bool res = BFSNetwork::createRemoteFile(_name,mostFreeNode.MAC);
  ZooHandler::getInstance().requestUpdateGlobalView();
  return res;
}

bool FileSystem::moveToRemoteNode(FileNode* _localFile) {
  ZooNode mostFreeNode = ZooHandler::getInstance().getMostFreeNode();
  if(mostFreeNode.freeSpace < _localFile->getSize()*2) //Could not find a node with enough space :(
    return false;
  return BFSNetwork::createRemoteFile(_localFile->getFullPath(),mostFreeNode.MAC);
}

intptr_t FileSystem::getNodeByINodeNum(uint64_t _inodeNum) {
  lock_guard<mutex> lock_gurad(inodeMapMutex);
  auto res = inodeMap.find(_inodeNum);
  if(res != inodeMap.end())
    return res->second;
  else
    return 0;
}

uint64_t FileSystem::assignINodeNum(intptr_t _nodePtr) {
  lock_guard<mutex> lock_gurad(inodeMapMutex);
  uint64_t nextInodeNum = ++inodeCounter;
  if(inodeMap.find(nextInodeNum)!=inodeMap.end()) {
    fprintf(stderr, "fileSystem(): inodeCounter overflow :(\n");
    fflush(stderr);
    return 0;
  }

  auto res = inodeMap.insert(pair<uint64_t,intptr_t>(nextInodeNum,_nodePtr));
  if(res.second) {
    //Insert to nodeInodemaps as well!
    auto nodeInodeItr = nodeInodeMap.find(_nodePtr);
    if(nodeInodeItr != nodeInodeMap.end()){
      //Existing, push back
      nodeInodeItr->second.push_back(nextInodeNum);
    } else {
      list<uint64_t> listOfInodes;
      listOfInodes.push_back(nextInodeNum);
      if(!nodeInodeMap.insert(pair<intptr_t,list<uint64_t> >(_nodePtr,listOfInodes)).second)//failed to insert
        return 0;//Failure
    }
    return nextInodeNum;//Success
  }
  else
    return 0;//Failure
}

void FileSystem::replaceAllInodesByNewNode(intptr_t _oldPtr,intptr_t _newPtr) {
  lock_guard<mutex> lock_gurad(inodeMapMutex);

  auto res = nodeInodeMap.find(_oldPtr);
  if(res == nodeInodeMap.end())
  {
    LOG(ERROR)<<"Not a valid _nodePtr!";
    return; //Not such a
  }

  list<uint64_t> inodeValuesOldPtr;
  for(uint64_t inode:res->second){
    inodeMap.erase(inode);
    if(!inodeMap.insert(pair<uint64_t,intptr_t>(inode,_newPtr)).second)
      LOG(ERROR)<<"failed to insert the new node for Inode"<<inode;
    inodeValuesOldPtr.push_back(inode);
  }

  //Insert all values of oldPtr in nodeInodeMap with _newPtr
  if(!nodeInodeMap.insert(pair<intptr_t,list<uint64_t>>(_newPtr,inodeValuesOldPtr)).second)
    LOG(ERROR)<<"Cannot insert _newPtr in the nodeInodeMap";
  //and get rid of _oldPtr
  nodeInodeMap.erase(_oldPtr);
}

void FileSystem::removeINodeEntry(uint64_t _inodeNum) {
  lock_guard<mutex> lock_gurad(inodeMapMutex);
  //Find ptr_t
  auto res = inodeMap.find(_inodeNum);
  if(res != inodeMap.end()){
    intptr_t nodeVal = res->second;
    auto inodesList = nodeInodeMap.find(nodeVal);
    if(inodesList == nodeInodeMap.end())
      LOG(ERROR)<<"Cannot find corresponding inodeList in nodeInodeMap";
    else {
      for(auto itr = inodesList->second.begin();itr != inodesList->second.end();){
        if(*itr == _inodeNum)
          itr = inodesList->second.erase(itr);
        else
          itr++;
      }
      //if inodesList size is zero we should get rid of this entry in nodeInodeMap as well
      if(inodesList->second.size() == 0)
        nodeInodeMap.erase(nodeVal);
    }
  }
  else
    LOG(ERROR)<<"Cannot find corresponding intptr_t in inodeMap";

  //Finally remove _inode
  inodeMap.erase(_inodeNum);
}

} // namespace
