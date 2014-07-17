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
#include <atomic>
#include <vector>
#include <mutex>


namespace FUSESwift {

typedef std::map<std::string,std::string> metadataDictionary;

class FileNode: public Node {
  const std::string uidKey   = "uid";//User ID
  const std::string gidKey   = "gid";//Group ID
  const std::string mtimeKey = "mtime";//Last modified time
  const std::string ctimeKey = "ctime";//Create Time
  const std::string modeKey = "mode";//File mode

  //Private Members
  metadataDictionary metadata;
  bool isDir;
  size_t size;
  std::atomic<int> refCount;
  std::vector<char*> dataList;
  unsigned int blockIndex;
  std::atomic<bool> needSync;
public:
  FileNode(std::string _name,bool _isDir, FileNode* _parent);
  virtual ~FileNode();
  /**
   * if an element with key '_key' exist this will override it
   */
  void metadataAdd(std::string _key, std::string _value);
  void metadataRemove(std::string _key);
  std::string metadataGet(std::string _key);
  unsigned long getUID();
  unsigned long getGID();
  void setUID(unsigned long _uid);
  void setGID(unsigned long _gid);
  unsigned long getMTime();
  unsigned long getCTime();
  void setMTime(unsigned long _mtime);
  void setCTime(unsigned long _ctime);
  mode_t getMode();
  void setMode(mode_t _mode);
  bool getNeedSync();
  void setNeedSync(bool _need);
  metadataDictionary::iterator metadataBegin();
  metadataDictionary::iterator metadataEnd();
  std::string getName();
  std::string getMD5();
  FileNode* getParent();
  std::string getFullPath();
  size_t getSize();
  /**
   * tries to rename input child
   * @return
   *  true if successful
   *  false if fails
   */
  bool renameChild(FileNode* _child,const std::string &_newName);

  void setName(std::string _newName);

  /**
   * writes input data to this file
   * returns true if successful, false if fails.
   */
  long write(const char* _data,size_t _size);
  /**
   * reads file data to input arguments.
   * returns true if successful, false if fails.
   */
  long read(char* &_data,size_t _size);
  /**
   * not inclusive get data by range
   * returns data store from index _offset to _offset+size-1
   * if specified _offset or _size returns false.
   */
  long read(char* &_data, size_t _offset, size_t _size);
  /**
   * not inclusive set data by range
   * sets data from index _offset to _offset+size-1 by the specified input _data
   * if specified _offset or _size are irrelevant false is returned
   */
  long write(const char *_data, size_t _offset, size_t _size);

  bool isDirectory();
  bool open();
  void close();
  bool isOpen();
};

} /* namespace FUSESwift */
#endif /* FILENODE_H_ */
