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

using namespace std;

namespace FUSESwift {

FileNode::FileNode(string _name,bool _isDir):Node(_name),
    isDir(_isDir),data(nullptr),size(0) {
}

FileNode::~FileNode() {
  if(data != nullptr) {
    free(data);
    data = nullptr;
  }
}

void FileNode::metadataAdd(std::string _key, std::string _value) {
  metadataDictionary::iterator it = metadata.find(_key);
  if(it == metadata.end())
    metadata.insert(make_pair(_key,_value));
  else //update value
    it->second = _value;
}

void FileNode::metadataRemove(std::string _key) {
  metadata.erase(_key);
}

string FileNode::metadataGet(string _key) {
  metadataDictionary::iterator it = metadata.find(_key);
  return (it == metadata.end())? "": it->second;
}

metadataDictionary::iterator FileNode::metadataBegin() {
  return metadata.begin();
}

metadataDictionary::iterator FileNode::metadataEnd() {
  return metadata.end();
}

bool FileNode::write(const void* _data, size_t _size) {
  data = (char*)malloc(_size);
  if(data == nullptr)
    return false;
  size = _size;
  memcpy(data,_data,_size);
  return true;
}

bool FileNode::read(void* &_data, size_t &_size) {
  if(data == nullptr) {
    data = nullptr;
    _size = 0;
    return false;
  }

  memcpy(_data,data,size);
  _size = size;
  return true;
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

bool FileNode::read(void* &_data, size_t _offset, size_t &_size) {
  if(_offset+_size > size || data == nullptr) {
    _data = nullptr;
    _size = 0;
    return false;
  }

  //Valid indices
  void *pointer = data + _offset;
  memcpy(_data,pointer,_size);
  return true;
}

bool FileNode::write(const void* _data, size_t _offset, size_t _size) {
  if(_offset+_size > size)
    return false;
  if(data == nullptr)
    return false;
  //Valid indices
  memcpy(data+_offset,_data,_size);
  return true;
}

bool FileNode::append(const void *_data, size_t _size) {
  void *newData = realloc(data,size+_size);
  if(newData == nullptr)
    return false;


  data = (char*)newData;
  void *pointer = data + size;
  size = size+_size;
  memcpy(pointer,_data,_size);
  return true;
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
  childDictionary::iterator it = children.find(_child->getName());
  if(it == children.end())
    return false;
  //First remove node
  children.erase(it);
  //Now insert it again with the updated name
  _child->setName(_newName);
  return childAdd(_child).second;
}

bool FileNode::isDirectory() {
  return isDir;
}

}
