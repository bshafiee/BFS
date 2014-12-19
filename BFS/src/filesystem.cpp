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
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "ZooHandler.h"
#include "MemoryController.h"
#include "BFSTcpServer.h"
#include "LoggerInclude.h"
#include "Timer.h"

using namespace std;
using namespace Poco;

namespace FUSESwift {

//initialize static variables
const std::string FileSystem::delimiter = "/";

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

FileNode* FileSystem::mkFile(FileNode* _parent, const std::string &_name,bool _isRemote, bool _open) {
  if (_parent == nullptr || _name.length() == 0)
    return nullptr;
  FileNode *file = nullptr;
  if(_parent == root)
    file = new FileNode(_name,_parent->getFullPath()+_name, false, _isRemote);
  else
    file = new FileNode(_name,_parent->getFullPath()+delimiter+_name, false, _isRemote);
  if(_open) {
    if(!file->open()){
      LOG(ERROR)<<"Failed to open newly created file:"<<file->getFullPath();
      return nullptr;
    }
  }
  auto res = _parent->childAdd(file);
  if (res.second) {
  	//Inform ZooHandler about new file if not remote
  	if(!_isRemote)
  		ZooHandler::getInstance().publishListOfFiles();
    return file;
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
  if (res.second){
    //Inform ZooHandler about new file if not remote
    if(!_isRemote)
      ZooHandler::getInstance().publishListOfFiles();
    return (FileNode*) (res.first->second);
  }
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
    const Node* node = start->childFind(tokenizer[i]);
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

FileNode* FileSystem::createHierarchy(const std::string &_path,bool _isRemote) {
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
      start = mkDirectory(start,tokenizer[i],_isRemote);
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

FileNode* FileSystem::mkFile(const string &_path,bool _isRemote,bool _open) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  FileNode* result = mkFile(parent, name, _isRemote,_open);

  return result;
}

FileNode* FileSystem::mkDirectory(const std::string &_path,bool _isRemote) {
  FileNode* parent = traversePathToParent(_path);
  string name = getNameFromPath(_path);
  //Now parent node is start
  FileNode* result = mkDirectory(parent, name,_isRemote);

  return result;
}

FileNode* FileSystem::findAndOpenNode(const std::string &_path) {
  lock_guard<recursive_mutex> lk(deleteQueueMutex);

  if(unlikely(_path == "")){
    LOG(ERROR)<<"Invalid find File Request:"<<_path;
    return nullptr;
  }
  //Root
  if (_path == FileSystem::delimiter){
    if(root->open())
      return root;
    else
      return nullptr;
  }
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

  if(start!=nullptr)
    if(start->open())
      return start;
  return nullptr;
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
  //Empty InodeMap so no body can find anything!
  inodeMapMutex.lock();
  inodeMap.clear();

  LOG(INFO)<<"FILESYSTEM'S DEAD!";
}

bool FileSystem::tryRename(const string &_from,const string &_to) {
  FileNode* node = findAndOpenNode(_from);
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

  //Close it! so it can be removed if needed
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  node->close(inodeNum);

  //Ask  parent to delete this node
  bool result = parentNode->renameChild(node,newName);
  //Add syncQueue event
  if(result)
    UploadQueue::getInstance().push(new SyncEvent(SyncEventType::RENAME,_from));
  return result;
}

std::string FileSystem::printFileSystem() {
  string output = "";
  //Recursive removing is dangerous, because we might run out of memory.
  vector<FileNode*> childrenQueue;
  FileNode* start = root;
  int newLineCounter = 1;
  LOG(DEBUG)<<"FileSystem:";
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
  LOG(DEBUG)<<output<<endl;
  return output;
}

std::vector<std::pair<std::string,bool> > FileSystem::listFileSystem(bool _includeRemotes,bool _includeFolders) {
  vector<pair<string,bool>> output;
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
    if(start->getName()!="/" && !start->mustBeDeleted()){//if not a directory
      if(_includeFolders)
        output.push_back(make_pair(start->getFullPath(),start->isDirectory()));
      else if(!start->isDirectory())
        output.push_back(make_pair(start->getFullPath(),start->isDirectory()));
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
#ifdef BFS_ZERO
  bool res = BFSNetwork::createRemoteFile(_name,mostFreeNode.MAC);
#else
  bool res = BFSTcpServer::createRemoteFile(_name,mostFreeNode.ip,mostFreeNode.port);
#endif
  ZooHandler::getInstance().requestUpdateGlobalView();
  return res;
}

bool FileSystem::moveToRemoteNode(FileNode* _localFile) {
  ZooNode mostFreeNode = ZooHandler::getInstance().getMostFreeNode();
  if(mostFreeNode.freeSpace < _localFile->getSize()*2) //Could not find a node with enough space :(
    return false;
  //First advertise the list of files you have to make sure the dst node will see this file
  ZooHandler::getInstance().publishListOfFiles();
#ifdef BFS_ZERO
  return BFSNetwork::createRemoteFile(_localFile->getFullPath(),mostFreeNode.MAC);
#else
  return BFSTcpServer::moveFileToRemoteNode(_localFile->getFullPath(),mostFreeNode.ip,mostFreeNode.port);
#endif
}

intptr_t FileSystem::getNodeByINodeNum(uint64_t _inodeNum) {
  lock_guard<recursive_mutex> lock_gurad(inodeMapMutex);
  auto res = inodeMap.find(_inodeNum);
  if(res != inodeMap.end())
    return res->second.first;
  else
    return 0;
}

uint64_t FileSystem::assignINodeNum(const intptr_t _nodePtr) {
  lock_guard<recursive_mutex> lock_gurad(inodeMapMutex);
  uint64_t nextInodeNum = ++inodeCounter;
  if(unlikely(nextInodeNum == 0))//Not Zero
    nextInodeNum = ++inodeCounter;
  if(inodeMap.find(nextInodeNum)!=inodeMap.end()) {
    LOG(FATAL)<<"\nfileSystem(): inodeCounter overflow :(\n";
    fprintf(stderr, "fileSystem(): inodeCounter overflow :(\n");
    fflush(stderr);
    return 0;
  }

  auto res = inodeMap.insert(pair<uint64_t,pair<intptr_t,bool>>(nextInodeNum,pair<intptr_t,bool>(_nodePtr,false)));
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
  lock_guard<recursive_mutex> lock_gurad(inodeMapMutex);

  auto res = nodeInodeMap.find(_oldPtr);
  if(res == nodeInodeMap.end())
  {
    LOG(ERROR)<<"Not a valid _nodePtr!";
    return; //Not such a
  }

  list<uint64_t> inodeValuesOldPtr;
  for(uint64_t inode:res->second){
    inodeMap.erase(inode);
    if(!inodeMap.insert(pair<uint64_t,pair<intptr_t,bool>>(inode,pair<intptr_t,bool>(_newPtr,false))).second)
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
  lock_guard<recursive_mutex> lock_gurad(inodeMapMutex);
  //Find ptr_t
  auto res = inodeMap.find(_inodeNum);
  if(res != inodeMap.end()){
    intptr_t nodeVal = res->second.first;
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
  else{
    if(_inodeNum!=0)//used during move operation(exception)
      LOG(ERROR)<<"Cannot find corresponding intptr_t in inodeMap:"<<_inodeNum;
  }

  //Finally remove _inode
  inodeMap.erase(_inodeNum);
}

bool FileSystem::signalDeleteNode(FileNode* _node,bool _informRemoteOwner) {
  //Grab locks
  lock_guard<recursive_mutex> lk(deleteQueueMutex);
  lock_guard<recursive_mutex> lock_gurad(inodeMapMutex);

  //Check if this file is already deleted!
  auto inodeList = nodeInodeMap.find((intptr_t)_node);
  if(inodeList != nodeInodeMap.end()) {
    for(uint64_t inode:inodeList->second) {
      auto inodeIt = inodeMap.find(inode);
      if(inodeIt == inodeMap.end()){
        LOG(FATAL)<<"Inconcsitency between inodeMap and nodeInodeMap!";
        exit(-1);
      }
      else if(inodeIt->second.second){
        LOG(DEBUG)<<"SIGNAL DELETE ALREADY DELETED: Ptr:"<<(FileNode*)_node;
        return true;
      }
    }
  }

  //Check if already Exist
  bool found = false;
  for(FileNode* node:deleteQueue)
    if(node == _node) { //Already has been called!
      found = true;
      //If is still open just return otherwise we gonna go to delete it!
      if(_node->isOpen()){
        LOG(DEBUG)<<"SIGNAL DELETE FORLOOP: Key:"<<_node->key<<" isOpen?"<<_node->concurrentOpen()<<" isRemote():"<<_node->isRemote()<<" Ptr:"<<(FileNode*)_node;
        return true;
      }
      else
        break;//Going to fuck it!
    }

  //0) MustBeDeleted True
  _node->mustDeleted = true;
  _node->mustInformRemoteOwner = _informRemoteOwner;

  //1) Add Node to the delete Queue
  if(!found)
    deleteQueue.push_back(_node);

  //2) Remove parent->thisnode link so it won't show up anymore
  if(!_node->hasInformedDelete){//Only once
    FileNode* parent = findParent(_node->getFullPath());
    if(parent)
      parent->childRemove(_node->key);
    //Nobody can open this file anymore!
  }

  //3)) Tell Backend that this is gone!
  if(!_node->isRemote())
    if(!_node->hasInformedDelete)//Only once
      if(!_node->isMoving())//Not moving nodes
        UploadQueue::getInstance().push(
            new SyncEvent(SyncEventType::DELETE,_node->getFullPath()));

  //4)) Inform rest of World that I'm gone
  if(!_node->isRemote())
    if(!_node->hasInformedDelete)//Only once
      ZooHandler::getInstance().publishListOfFiles();

  bool resultRemote = true;
  if(_node->isRemote() && _informRemoteOwner)
    if(!_node->hasInformedDelete){
      resultRemote = _node->rmRemote();
    }

  _node->hasInformedDelete = true;
  //5) if is open just return and we'll come back later
  if(_node->isOpen()){
    LOG(DEBUG)<<"SIGNAL DELETE ISOPEN Key:"<<_node->key<<" howmanyOpen?"<<_node->refCount<<" isRemote():"<<_node->isRemote()<<" Ptr:"<<(FileNode*)_node;
    return true;//will be deleted on close
  }


  //6)) FILE DECONTAMINATION YOHAHHA :D

  //Remove from queue
  for(auto it=deleteQueue.begin();it!=deleteQueue.end();it++)
    if(*it == _node){
      deleteQueue.erase(it);
      break;
    }

  LOG(DEBUG)<<"SIGNAL DELETE DONE: MemUtil:"<<MemoryContorller::getInstance().getMemoryUtilization()<<" UsedMem:"<<MemoryContorller::getInstance().getTotal()/1024l/1024l<<" MB. Key:"<<_node->key<<" Size:"<<_node->getSize()<<" isOpen?"<<_node->concurrentOpen()<<" isRemote():"<<_node->isRemote()<<" Ptr:"<<(FileNode*)_node;

  //Update nodeInodemap and inodemap
  //if(!_node->isMoving()) {
    auto res = nodeInodeMap.find((intptr_t)_node);
    if(res != nodeInodeMap.end()) {//File is still open
      for(uint64_t inode:res->second) {
        auto inodeIt = inodeMap.find(inode);
        if(inodeIt == inodeMap.end())
          LOG(FATAL)<<"Inconcsitency between inodeMap and nodeInodeMap!";
        else
          inodeIt->second.second = true; //Inidicate deleted
      }
    }
  //}

  //Actual release of memory!
  delete _node;//this will recursively call signalDelete on all kids of this node

  return resultRemote;
}

} // namespace
