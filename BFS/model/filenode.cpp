/*
 * fileNode.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "filenode.h"
#include <cstring>
#include <sstream>      // std::istringstream
#include "../log.h"
#include "filesystem.h"
#include <Poco/MD5Engine.h>
#include "UploadQueue.h"

using namespace std;

namespace FUSESwift {

FileNode::FileNode(string _name,bool _isDir, FileNode* _parent):Node(_name,_parent),
    isDir(_isDir),size(0),refCount(0),blockIndex(0), needSync(false) {
}

FileNode::~FileNode() {
  for(auto it = dataList.begin();it != dataList.end();it++) {
    char *block = *it;
    free(block);
    block = nullptr;
  }
}

FileNode* FileNode::getParent() {
  return (FileNode*)parent;
}

void FileNode::metadataAdd(std::string _key, std::string _value) {
  auto it = metadata.find(_key);
  if(it == metadata.end())
    metadata.insert(make_pair(_key,_value));
  else //update value
    it->second = _value;
}

void FileNode::metadataRemove(std::string _key) {
  metadata.erase(_key);
}

string FileNode::metadataGet(string _key) {
  auto it = metadata.find(_key);
  return (it == metadata.end())? "": it->second;
}

metadataDictionary::iterator FileNode::metadataBegin() {
  return metadata.begin();
}

metadataDictionary::iterator FileNode::metadataEnd() {
  return metadata.end();
}

long FileNode::write(const char* _data, size_t _size) {
  return this->write(_data,0,_size);
}

long FileNode::read(char* &_data, size_t _size) {
  return this->read(_data,0,_size);
}

std::string FileNode::getName() {
  return key;
}

size_t FileNode::getSize() {
  return size;
}

void FileNode::setName(std::string _newName) {
  key = _newName;
}

long FileNode::read(char* &_data, size_t _offset, size_t _size) {
  if(_offset == size)
    return 0;

  if(_offset > size) {
    _data = nullptr;
    return -1;
  }

  /**
   * sometimes fuse (for last block) ask for more
   * bytes than actual size, because it's buffer size
   * is usually fix
   */
  if (_size > size)
	  _size = size;
  if(_size+_offset > size)
  	_size = size - _offset;

  //Find correspondent block
  size_t blockNo = _offset / FileSystem::blockSize;
  unsigned int index = _offset - blockNo*FileSystem::blockSize;
  char* block = dataList[blockNo];

  size_t total = 0;
  //Start filling
  while(_size > 0) {
  	size_t howMuch = FileSystem::blockSize - index;//Left bytes in the last block
  	howMuch = (howMuch > _size)?_size:howMuch;
  	//log_msg("BEFORE howMuch:%zu index:%zu _size:%zu total:%zu SIZE:%zu blockAddr:0x%08x blockNo:%zu  totalBlocks:%zu\n",howMuch,index,_size,total,size,block,blockNo,dataList.size());
  	memcpy(_data+total, block+index,howMuch);
  	_size -= howMuch;
  	index += howMuch;
  	total += howMuch;
  	blockNo++;
  	if(index >= FileSystem::blockSize) {
  		block = dataList[blockNo];
  		index = 0;
  	}
  }

  return total;
}

long FileNode::write(const char* _data, size_t _offset, size_t _size) {
  size_t retValue = 0;
  size_t backupSize = _size;
  if(_offset < size) { //update existing (unlikely)
	//Find correspondent block
	size_t blockNo = _offset / FileSystem::blockSize;
	unsigned int index = _offset - blockNo*FileSystem::blockSize;

	char* block = dataList[blockNo];

	//Start filling
	while(_size > 0) {
		size_t left = FileSystem::blockSize - index;//Left bytes in the last block
		size_t howMuch = (_size <= left)? _size:left;
		memcpy(block+index,_data+(backupSize-_size),howMuch);
		_size -= howMuch;
		index += howMuch;
		retValue += howMuch;
		blockNo++;
		if(index >= FileSystem::blockSize && blockNo < dataList.size()) {
		  block = dataList[blockNo];
		  index = 0;
		} else if(index >= FileSystem::blockSize && blockNo >= dataList.size()) {
			return retValue + this->write(_data+retValue,0,backupSize - retValue);
		}
	}
  }
  else { //append to the end
	  //first time (unlikely)
	  if(dataList.size() == 0) {
		  char* block = new char[FileSystem::blockSize];
		  dataList.push_back(block);
		  blockIndex = 0;
	  }

	  //Start filling
	  while(_size > 0) {
		//Get pointer to the last buffer
		char* lastBlock = dataList.at(dataList.size()-1);
		size_t left = FileSystem::blockSize - blockIndex;//Left bytes in the last block
		size_t howMuch = (_size <= left)? _size:left;
		memcpy(lastBlock+blockIndex,_data+(backupSize-_size),howMuch);
		_size -= howMuch;
		size += howMuch;//increase file size
		retValue += howMuch;
		blockIndex += howMuch;
		if(blockIndex >= FileSystem::blockSize) {
		  char* block = (char*)malloc(FileSystem::blockSize*sizeof(char));
		  dataList.push_back(block);
		  blockIndex = 0;
		}
	  }
  }
  return retValue;
}

unsigned long FileNode::getUID() {
  string uidStr = metadataGet(uidKey);
  istringstream ss(uidStr);
  unsigned long output = 0;
  ss >> output;
  return output;
}

unsigned long FileNode::getGID() {
  string gidStr = metadataGet(gidKey);
  istringstream ss(gidStr);
  unsigned long output = 0;
  ss >> output;
  return output;
}

void FileNode::setUID(unsigned long _uid) {
  stringstream ss;
  ss << _uid;
  metadataAdd(uidKey,ss.str());
}

void FileNode::setGID(unsigned long _gid) {
  stringstream ss;
  ss << _gid;
  metadataAdd(gidKey,ss.str());
}

unsigned long FileNode::getMTime() {
  string gidStr = metadataGet(mtimeKey);
  istringstream ss(gidStr);
  unsigned long output = 0;
  ss >> output;
  return output;
}

unsigned long FileNode::getCTime() {
  string gidStr = metadataGet(ctimeKey);
  istringstream ss(gidStr);
  unsigned long output = 0;
  ss >> output;
  return output;
}

void FileNode::setMTime(unsigned long _mtime) {
  stringstream ss;
  ss << _mtime;
  metadataAdd(mtimeKey,ss.str());
}

void FileNode::setCTime(unsigned long _ctime) {
  stringstream ss;
  ss << _ctime;
  metadataAdd(ctimeKey,ss.str());
}

mode_t FileNode::getMode() {
  string modeStr = metadataGet(modeKey);
  istringstream ss(modeStr);
  mode_t output = 0;
  ss >> output;
  return output;
}

void FileNode::setMode(mode_t _mode) {
  stringstream ss;
  ss << _mode;
  metadataAdd(modeKey,ss.str());
}

bool FileNode::renameChild(FileNode* _child,const string &_newName) {
  //Not such a node
  auto it = children.find(_child->getName());
  if(it == children.end())
    return false;
  //First remove node
  children.erase(it);
  //Second remove all the existing nodes with _newName
  it = children.find(_newName);
  if(it != children.end()) {
    FileNode* existingNodes = (FileNode*)(it->second);
    FileNode* parent = this;
    FileSystem::getInstance()->rmNode(parent, existingNodes);
    //children.erase(it);
  }

  //Now insert it again with the updated name
  _child->setName(_newName);
  return childAdd(_child).second;
}

bool FileNode::isDirectory() {
  return isDir;
}

bool FileNode::open() {
  refCount++;
  return true;
}

void FileNode::close() {
  refCount--;
  /**
   * add event to sync queue if all the refrences to this file
   * are closed and it actually needs updating!
   */
  if(refCount == 0 && needSync) {
    if(UploadQueue::push(new SyncEvent(SyncEventType::UPDATE_CONTENT,this)))
      this->setNeedSync(false);
  }
  else {
    /*int refs = refCount;
    bool needs = needSync;
    log_msg("UPDATE Event File:%s  but: refCount:%d  needSync:%d\n",getFullPath().c_str(),refs,needs);*/
  }
}

bool FUSESwift::FileNode::getNeedSync() {
  return needSync;
}

void FUSESwift::FileNode::setNeedSync(bool _need) {
  needSync = _need;
}

std::string FUSESwift::FileNode::getFullPath() {
  string path = this->getName();
  FileNode* par = getParent();
  while(par != nullptr) {
    path = par->getName() + (par->getParent()==nullptr?"":"/") + path;
    par = par->getParent();
  }
  return path;
}

bool FileNode::isOpen() {
  return refCount;
}

std::string FileNode::getMD5() {
  if(size <= 0)
    return "";
  Poco::MD5Engine md5;
  md5.reset();
  register size_t blockSize = FileSystem::blockSize;
  for(uint i=0;i<dataList.size()-1;i++)
    md5.update(dataList[i],blockSize);
  //last block might not be full
  md5.update(dataList[dataList.size()-1],blockIndex);
  return Poco::DigestEngine::digestToHex(md5.digest());
}

} //namespace
