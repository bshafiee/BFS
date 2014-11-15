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
#include <sys/stat.h>


namespace FUSESwift {

typedef std::map<std::string,std::string> metadataDictionary;

struct ReadBuffer {
	unsigned char* buffer;
	uint64_t capacity;
	//of a file
	uint64_t offset;
	uint64_t size;
	bool reachedEOF;
	//
	ReadBuffer(uint64_t _capacity);
	~ReadBuffer();
	bool inline contains(uint64_t _reqOffset,uint64_t _reqSize);
	long readBuffered(void* _dstBuff,uint64_t _reqOffset,uint64_t _reqSize);
};

class FileNode: public Node {
  const std::string uidKey   = "uid";//User ID
  const std::string gidKey   = "gid";//Group ID
  const std::string mtimeKey = "mtime";//Last modified time
  const std::string ctimeKey = "ctime";//Create Time
  const std::string modeKey = "mode";//File mode
  static const uint64_t READ_BUFFER_SIZE = 1024*1024*10; //10MB buffer

  //Private Members
  metadataDictionary metadata;
  bool isDir;
  size_t size;
  std::atomic<int> refCount;
  std::vector<char*> dataList;
  unsigned int blockIndex;
  std::atomic<bool> needSync;
  std::atomic<bool> mustDeleted;//To indicate this file should be deleted after being closed
  std::atomic<bool> isRem;//indicates whether this node exist on the local RAM or on a remote machine
  unsigned char remoteHostMAC[6];
  //Delete Lock
  std::mutex deleteMutex;
  //Read/Write Lock
  std::mutex ioMutex;
  //Metadata Lock
	std::mutex metadataMutex;
	//Buffer
	//ReadBuffer* readBuffer;
public:
  FileNode(std::string _name,bool _isDir, FileNode* _parent,bool _isRemote);
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
  std::string getName();
  std::string getMD5();
  FileNode* getParent();
  std::string getFullPath();
  size_t getSize();
  bool isRemote();
  const unsigned char* getRemoteHostMAC();
  void setRemoteHostMAC(const unsigned char *_mac);
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
  /**
   * Truncates the file to the specified size if possible
   */
  bool truncate(size_t _size);

  bool isDirectory();
  bool open();
  void close();
  bool isOpen();
  void lockDelete();
  void unlockDelete();
  /**
   * When a file is being removed it might be open yet!
   * therefore, we indicate this file should be removed after all
   * the references to it being closed!
   */
  bool signalDelete();
  //Remote File Operation
  bool getStat(struct stat *stbuff);
  long readRemote(char* _data, size_t _offset, size_t _size);
  long writeRemote(const char* _data, size_t _offset, size_t _size);
  bool rmRemote();
  bool truncateRemote(size_t size);
  void makeLocal();
  void makeRemote();
  void deallocate();
};

} /* namespace FUSESwift */
#endif /* FILENODE_H_ */
