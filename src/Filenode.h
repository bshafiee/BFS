/**********************************************************************
Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
This program comes with ABSOLUTELY NO WARRANTY;

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#ifndef FILENODE_H_
#define FILENODE_H_
#include "Global.h"
#include <map>
#include <atomic>
#include <vector>
#include <mutex>
#include <sys/stat.h>
#include "BFSTcpServiceHandler.h"
#include "Filesystem.h"
#include "Node.h"


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

  friend FileSystem;

  const std::string uidKey   = "uid";//User ID
  const std::string gidKey   = "gid";//Group ID
  const std::string mtimeKey = "mtime";//Last modified time
  const std::string ctimeKey = "ctime";//Create Time
  const std::string modeKey = "mode";//File mode
  static const uint64_t READ_BUFFER_SIZE = 1024*1024*10; //10MB buffer

  //Private Members
  metadataDictionary metadata;
  bool isDir;
  std::atomic<int64_t> size;
  std::atomic<int> refCount;
  std::vector<char*> dataList;
  uint64_t blockIndex;
  std::atomic<bool> needSync;
  //Flush stuff
  std::atomic<bool> isFlushed;

  std::atomic<bool> mustDeleted;//To indicate this file should be deleted after being closed
  std::atomic<bool> hasInformedDelete;//To indicate if we have already informed the world about deleting this file or not(delete fucntion might be called several times on a file)
  std::atomic<bool> isRem;//indicates whether this node exist on the local RAM or on a remote machine
  std::atomic<bool> mustInformRemoteOwner;//To indicate if we should tell remote owner to remove or not(e.g zookeeper deletes don't need to do so).
  std::atomic<bool> moving;//to indicate if it's being moved to another node
  /**
   * Don't remove by zoo handler because we are reading a remote file into this
   * file. so eventhough it's remote it contains data and zoo handler should not
   * remove a filenode which has this flag true;
   */
  std::atomic<bool> shouldNotRemoveZooHandler;//to indicate if it's being moved to another node
  unsigned char remoteHostMAC[6];
  std::string remoteIP;
  uint32_t remotePort;
  //Read/Write Lock
  std::recursive_mutex ioMutex;
  //Metadata Lock
	std::mutex metadataMutex;
	//Buffer
	//ReadBuffer* readBuffer;

	/**
	 * Marked for transfer indicates if we are planning to move this file
	 * to another on a remote write request hit
	 */
	std::atomic<bool> transfering;
  long write(const char *_data, int64_t _offset, int64_t _size);
  bool flushRemote();
public:
  FileNode(std::string _name,std::string _fullPath,bool _isDir,bool _isRemote);
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
  FileNode* findParent();
  int64_t getSize();
  bool isRemote();
  int concurrentOpen();
  const unsigned char* getRemoteHostMAC();
  const std::string & getRemoteHostIP();
  void setRemoteHostMAC(const unsigned char *_mac);
  void setRemoteIP(const std::string &_ip);
  void setRemotePort(const uint32_t _port);
  bool mustBeDeleted();
  bool isMoving();
  void setMoving(bool _isMoving);
  /**
   * tries to rename input child
   * @return
   *  true if successful
   *  false if fails
   */
  bool renameChild(FileNode* _child,const std::string &_newName);

  void setName(std::string _newName);

  /**
   * @return
   * Failures:
   * -1 Moving
   * -2 NoSpace
   * -3 InternalError
   * Success:
   * >= 0 written bytes
   */
  long writeHandler(const char *_data, int64_t _offset, int64_t _size,FileNode* &_afterMoveNewNode,bool _shouldMove);
  /**
   * reads file data to input arguments.
   * returns true if successful, false if fails.
   */
  long read(char* &_data,int64_t _size);
  /**
   * not inclusive get data by range
   * returns data store from index _offset to _offset+size-1
   * if specified _offset or _size returns false.
   */
  long read(char* &_data, int64_t _offset, int64_t _size);

  /**
   * Same as Write except no data copy
   */
  long allocate(int64_t _offset, int64_t _size);

  /**
   * Truncates the file to the specified size if possible
   */
  bool truncate(int64_t _size);

  bool isDirectory();
  bool open();
  void close(uint64_t _inodeNum);
  bool isOpen();
  bool flush();
  bool flushed();
  bool isTransfering();
  void setTransfering(bool _value);
  bool shouldNotZooRemove();
  void setShouldNotZooRemove(bool _value);
  //Remote File Operation
  bool getStat(struct stat *stbuff);
  void fillPackedStat(struct packed_stat_info &st);
  void fillStatWithPacket(struct stat &st,const struct packed_stat_info& stPacket);
  long readRemote(char* _data, int64_t _offset, int64_t _size);
  long writeRemote(const char* _data, int64_t _offset, int64_t _size);
  bool rmRemote();
  bool truncateRemote(int64_t size);
  void makeLocal();
  void makeRemote();
  void deallocate();
  bool isVisitedByZooUpdate() const;
};

} /* namespace FUSESwift */
#endif /* FILENODE_H_ */
