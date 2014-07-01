/*
 * filenode.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef FILENODE_H_
#define FILENODE_H_

#include "node.h"
#include <map>

namespace FUSESwift {

typedef std::map<std::string,std::string> metadataDictionary;

class FileNode: public Node {
  metadataDictionary metadata;
  bool isDir;
  char* data;
  size_t size;
public:
  FileNode(std::string _name,bool _isDir);
  virtual ~FileNode();
  void metadataAdd(std::string _key, std::string _value);
  void metadataRemove(std::string _key);
  metadataDictionary::iterator metadataBegin();
  metadataDictionary::iterator metadataEnd();
  std::string getName();
  size_t getSize();
  void changeName(std::string _newName);

  /**
   * writes input data to this file
   * returns true if successful, false if fails.
   */
  bool write(const void* _data,size_t _size);
  /**
   * reads file data to input arguments.
   * returns true if successful, false if fails.
   */
  bool read(void* &_data,size_t &_size);
  /**
   * not inclusive get data by range
   * returns data store from index _offset to _offset+size-1
   * if specified _offset or _size returns false.
   */
  bool read(void* &_data, size_t _offset, size_t &_size);
  /**
   * not inclusive set data by range
   * sets data from index _offset to _offset+size-1 by the specified input _data
   * if specified _offset or _size are irrelevant false is returned
   */
  bool write(const void *_data, size_t _offset, size_t _size);
  /**
   * appends input data to the end of current data
   * returns true if successful, false if fails.
   */
  bool append(const void *_data, size_t _size);
  bool isDirectory();
};

} /* namespace FUSESwift */
#endif /* FILENODE_H_ */
