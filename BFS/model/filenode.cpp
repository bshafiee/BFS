/*
 * fileNode.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "filenode.h"
#include <cstring>

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
  metadata.insert(make_pair(_key,_value));
}

void FileNode::metadataRemove(std::string _key) {
  metadata.erase(_key);
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

void FileNode::changeName(std::string _newName) {
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

bool FileNode::isDirectory() {
  return isDir;
}

}
