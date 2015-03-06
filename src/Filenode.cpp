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

#include <cstring>
#include <sstream>      // std::istringstream
#include "LoggerInclude.h"
#include <Poco/MD5Engine.h>
#include "UploadQueue.h"
#include "MemoryController.h"
#include "BFSNetwork.h"
#include "ZooHandler.h"
#include "BFSTcpServer.h"
#include "Filenode.h"
#include "Filesystem.h"
#include "Timer.h"


using namespace std;

namespace FUSESwift {

FileNode::FileNode(string _name,string _fullPath,bool _isDir, bool _isRemote):
    Node(_name,_fullPath),isDir(_isDir),size(0),refCount(0),blockIndex(0),
    needSync(false), isFlushed(true), mustDeleted(false),
    hasInformedDelete(false), isRem(_isRemote), mustInformRemoteOwner(true),
    moving(false),shouldNotRemoveZooHandler(false) ,remoteIP(""),remotePort(0), transfering(false) {
	/*if(_isRemote)
		readBuffer = new ReadBuffer(READ_BUFFER_SIZE);*/
}

FileNode::~FileNode() {
  //delete readBuffer;
  for(auto it = dataList.begin();it != dataList.end();it++) {
    char *block = *it;
    delete []block;
    block = nullptr;
  }
  dataList.clear();
  //Clean up children
  //(auto it = children.begin();it != children.end();it++) {//We cann't use a for loop because the iteratoros might get deleted
  long childrenSize = children.size();
  while(childrenSize > 0){
    FileNode* child = (FileNode*)children.begin()->second;
    string key = children.begin()->first;
    if(child)
      FileSystem::getInstance().signalDeleteNode(child,mustInformRemoteOwner);//children loop is not valid after one delete
    //Sometimes it won't be delete so we'll delete it if exist
    children.erase(key);
    childrenSize--;
  }
  children.clear();

  //Release Memory in the memory controller!
  MemoryContorller::getInstance().releaseMemory(size);
  //Inform memory usage
  MemoryContorller::getInstance().informMemoryUsage();
}

FileNode* FileNode::findParent() {
  return FileSystem::getInstance().findParent(getFullPath());
}

void FileNode::metadataAdd(std::string _key, std::string _value) {
	lock_guard<std::mutex> lock(metadataMutex);
  auto it = metadata.find(_key);
  if(it == metadata.end())
    metadata.insert(make_pair(_key,_value));
  else //update value
    it->second = _value;
}

void FileNode::metadataRemove(std::string _key) {
	//Acquire lock
	lock_guard<mutex> lock(metadataMutex);
  metadata.erase(_key);
}

string FileNode::metadataGet(string _key) {
  lock_guard<mutex> lock(metadataMutex);
  auto it = metadata.find(_key);
  return (it == metadata.end())? "": it->second;
}

long FileNode::read(char* &_data, int64_t _size) {
  return this->read(_data,0,_size);
}

std::string FileNode::getName() {
  return key;
}

int64_t FileNode::getSize() {
  return size;
}

void FileNode::setName(std::string _newName) {
  key = _newName;
}

long FileNode::read(char* &_data, int64_t _offset, int64_t _size) {
  //Acquire lock
  lock_guard <recursive_mutex> lock(ioMutex);

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
  int64_t blockNo = _offset / FileSystem::blockSize;
  uint64_t index = _offset - blockNo*FileSystem::blockSize;
  char* block = dataList[blockNo];

  int64_t total = 0;
  //Start filling
  while(_size > 0) {
  	int64_t howMuch = FileSystem::blockSize - index;//Left bytes in the last block
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

long FileNode::write(const char* _data, int64_t _offset, int64_t _size) {
  //Acquire lock
  lock_guard <recursive_mutex> lock(ioMutex);

  if(_offset > size){
    LOG(ERROR)<<"\nOFFSET IS BIGGER THAN SIZE, OFFSET:"<<_offset<<" SIZE:"<<size<<" requestSize:"<<_size<<" name:"<<getFullPath();
    return -50;
  }

  //Check Storage space Availability
  int64_t newReqMemSize = (_offset + _size > size) ? _offset + _size - size : 0;
  if (newReqMemSize > 0)
    if (!MemoryContorller::getInstance().requestMemory(newReqMemSize))
      return -1;

  int64_t retValue = 0;
  int64_t backupSize = _size;
  if (_offset < size) { //update existing (unlikely)
    //Find correspondent block
    int64_t blockNo = _offset / FileSystem::blockSize;
    uint64_t index = _offset - blockNo * FileSystem::blockSize;

    char* block = dataList[blockNo];

    //Start filling
    while (_size > 0) {
      int64_t left = FileSystem::blockSize - index;//Left bytes in the last block
      int64_t howMuch = (_size <= left) ? _size : left;
      memcpy(block + index, _data + (backupSize - _size), howMuch);
      _size -= howMuch;
      index += howMuch;
      retValue += howMuch;
      blockNo++;
      //Increase size and blockIndex if in the last block
      if (index > blockIndex && blockNo == (int64_t)dataList.size()) {
        size += index - blockIndex;
        blockIndex = index;
        if (blockIndex >= FileSystem::blockSize)
          blockIndex = 0;
      }
      if (index >= FileSystem::blockSize && blockNo < (int64_t)dataList.size()) {
        block = dataList[blockNo];
        index = 0;
      } else if (index >= FileSystem::blockSize && blockNo >= (int64_t)dataList.size()) {
        //We should allocate a new block
        char* block = new char[FileSystem::blockSize];
        dataList.push_back(block);
        //block index shouuld have already be reset to 0
        return retValue
            + this->write(_data + retValue, getSize(), backupSize - retValue);
      }
    }
  } else { //append to the end
    //first time (unlikely)
    if (dataList.size() == 0) {
      char* block = new char[FileSystem::blockSize];
      dataList.push_back(block);
      blockIndex = 0;
    }

    //Start filling
    while (_size > 0) {
      //Get pointer to the last buffer
      char* lastBlock = dataList.at(dataList.size() - 1);
      int64_t left = FileSystem::blockSize - blockIndex;	//Left bytes in the last block
      int64_t howMuch = (_size <= left) ? _size : left;
      memcpy(lastBlock + blockIndex, _data + (backupSize - _size), howMuch);
      _size -= howMuch;
      size += howMuch;	  //increase file size
      retValue += howMuch;
      blockIndex += howMuch;
      if (blockIndex >= FileSystem::blockSize) {
        char* block = new char[FileSystem::blockSize];
        dataList.push_back(block);
        blockIndex = 0;
      }
    }
  }
  return retValue;
}

bool FUSESwift::FileNode::truncate(int64_t _size) {
  int64_t truncateDiff = _size - getSize();

  if(truncateDiff == 0)
    return true;//Nothing to do

  //Do we hace enough memory?
  if(!MemoryContorller::getInstance().checkPossibility(truncateDiff))
    return false;
  else if(_size > size) { //Expand
    //we just call write with '\0' chars buffers
    int64_t diff = _size-size;
    while(diff > 0) {
      long howMuch = (diff>FileSystem::blockSize)?FileSystem::blockSize:diff;
      char buff[howMuch];
      memset(buff,'\0',howMuch);
      //write
      if(write(buff,getSize(),howMuch) != howMuch)
        return false; //Error in writing
      diff -= howMuch;
    }
  }
  else {//Shrink
  	//Acquire lock
  	lock_guard<recursive_mutex> lock(ioMutex);
    int64_t diff = size-_size;
    while(diff > 0) {
      if((int64_t)blockIndex >= diff) {
        blockIndex -= diff;
        size -= diff;
        MemoryContorller::getInstance().releaseMemory(truncateDiff*-1);
        break;
      }
      else {
        //release block by block
        char *block = dataList[dataList.size()-1];
        delete []block;
        block = nullptr;
        dataList.erase(dataList.end()-1);
        diff -= blockIndex;
        size -= blockIndex;
        blockIndex = FileSystem::blockSize;
      }
    }
  }

  //File size changed so need to be synced with the backend.
  setNeedSync(true);

  return (size == _size);
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
  /**
   * Somehow enabling this line causes segfault in Fuse
   * multithread mode!
   */
  //metadataAdd(mtimeKey,ss.str());
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
    FileSystem::getInstance().signalDeleteNode(existingNodes,true);
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
  if(mustDeleted)
    return false;


  refCount++;

  //LOG(ERROR)<<"Open:"<<key<<" refCount:"<<refCount<<" ptr:"<<this;
  return true;
}

void FileNode::close(uint64_t _inodeNum) {
  refCount--;

  //LOG(ERROR)<<"Close refCount:"<<refCount<<" ptr:"<<this;
  /**
   * add event to sync queue if all the references to this file
   * are closed and it actually needs updating!
   */
  if(refCount < 0){
    LOG(ERROR)<<"SPURIOUS CLOSE:"<<key<<" ptr:"<<this;
  }
  if(refCount == 0) {
  	//If all references to this files are deleted so it can be deleted
  	if(mustDeleted){
  	  LOG(DEBUG)<<"SIGNAL FROM CLOSE KEY:"<<getFullPath()<<" isOpen?"<<refCount<<" isRemote():"<<isRemote()<<" ptr:"<<this;
  	  FileSystem::getInstance().signalDeleteNode(this,mustInformRemoteOwner);//It's close now! so will be removed
  	  /// NOTE AFTER THIS LINE ALL OF DATA IN THIS FILE ARE INVALID ///
  	  ZooHandler::getInstance().requestUpdateGlobalView();
  	} else{//Should not be deleted
      if(needSync && !isRemote()){
        if(UploadQueue::getInstance().push(new SyncEvent(SyncEventType::UPDATE_CONTENT,this->getFullPath())))
          this->setNeedSync(false);
        else
          LOG(ERROR)<<"\n\n\nHOLLY SHITTTTTTTTTTTT can't sync\n\n\n\n"<<key;
      }
  	}
  	//Inform memory usage
  	MemoryContorller::getInstance().informMemoryUsage();
  }
  else {
    /*int refs = refCount;
    bool needs = needSync;
    log_msg("UPDATE Event File:%s  but: refCount:%d  needSync:%d\n",getFullPath().c_str(),refs,needs);*/
  }

  //we can earse ionode num from map as well
  FileSystem::getInstance().removeINodeEntry(_inodeNum);
}

bool FUSESwift::FileNode::getNeedSync() {
  return needSync;
}

void FUSESwift::FileNode::setNeedSync(bool _need) {
  needSync = _need;
  if(needSync)
    isFlushed = false;
  //LOG(ERROR)<<"NeedSync:"<<_need<<" for:"<<key;
}

bool FileNode::isOpen() {
  return refCount>0;
}

std::string FileNode::getMD5() {
  if(isRemote()){
    Poco::MD5Engine md5;
    md5.reset();

    uint64_t offset = 0;
    int64_t read = 0;
    uint64_t bufferSize = 1024ll*1024ll*100ll;
    char * buffer = new char[bufferSize];//100MB buffer

    do{
      read = readRemote(buffer,offset,bufferSize);
      if(read > 0)
        md5.update(buffer,read);
      offset += read;
    }
    while(read > 0);

    delete []buffer;
    return Poco::DigestEngine::digestToHex(md5.digest());
  }

  //Not remote
  if(size <= 0)
    return "";
  lock_guard<recursive_mutex> lk(ioMutex);
  Poco::MD5Engine md5;
  md5.reset();
  int64_t blockSize = FileSystem::blockSize;
  for(uint i=0;i<dataList.size()-1;i++)
    md5.update(dataList[i],blockSize);
  //last block might not be full
  md5.update(dataList[dataList.size()-1],blockIndex);
  return Poco::DigestEngine::digestToHex(md5.digest());
}

bool FileNode::isRemote() {
	return isRem;
}

int FileNode::concurrentOpen(){
  return refCount;
}

const unsigned char* FileNode::getRemoteHostMAC() {
	return this->remoteHostMAC;
}

const string & FileNode::getRemoteHostIP() {
  return this->remoteIP;
}

void FileNode::setRemoteHostMAC(const unsigned char *_mac) {
  if(_mac!=nullptr)
    memcpy(this->remoteHostMAC,_mac,6*sizeof(char));
}

void FileNode::setRemoteIP(const string &_ip) {
  remoteIP = _ip;
}

void FileNode::setRemotePort(const uint32_t _port) {
  remotePort = _port;
}

bool FileNode::getStat(struct stat *stbuff) {

	if(!isRem || isDir){
		memset(stbuff, 0, sizeof(struct stat));
		//Fill Stat struct
		stbuff->st_dev = 0;
		stbuff->st_ino = 0;
		stbuff->st_mode = this->isDirectory() ? S_IFDIR : S_IFREG;
		stbuff->st_nlink = 1;
		stbuff->st_uid = this->getUID();
		stbuff->st_gid = this->getGID();
		stbuff->st_rdev = 0;
		stbuff->st_size = this->getSize();
		stbuff->st_blksize = FileSystem::blockSize;
		stbuff->st_blocks = this->getSize() / FileSystem::blockSize;
		stbuff->st_atime = 0x00000000;
		stbuff->st_mtime = this->getMTime();
		stbuff->st_ctime = this->getCTime();
		return true;
	} else {

	  struct packed_stat_info packedSt;
#ifdef BFS_ZERO
		bool res = BFSNetwork::readRemoteFileAttrib(&packedSt,this->getFullPath(),remoteHostMAC);
#else
		bool res = BFSTcpServer::attribRemoteFile(&packedSt,this->getFullPath(),remoteIP,remotePort)==200;
#endif
		fillStatWithPacket(*stbuff,packedSt);

		if(!res)
			LOG(ERROR)<<"GetRemoteAttrib failed for:"<<this->getFullPath();
		else {//update local info!
			//get io mutex to update size
			/*ioMutex.lock();//We should not do this, it will fuck move operations
			this->size = stbuff->st_size;
			ioMutex.unlock();*/

			//Update meta info
			stbuff->st_blksize = FileSystem::blockSize;
			stbuff->st_mode = this->isDirectory() ? S_IFDIR : S_IFREG;
			this->setCTime(stbuff->st_ctim.tv_sec);
			this->setMTime(stbuff->st_mtim.tv_sec);
			this->setUID(stbuff->st_uid);
			this->setGID(stbuff->st_gid);
		}

		return res;
	}
}

ReadBuffer::ReadBuffer(uint64_t _capacity):capacity(_capacity),
		offset(0),size(0),reachedEOF(false) {
	buffer = new unsigned char[_capacity];
}
ReadBuffer::~ReadBuffer(){
	delete []buffer;
}
bool inline ReadBuffer::contains(uint64_t _reqOffset,uint64_t _reqSize) {
	if(_reqOffset >= this->offset)
		if(reachedEOF)
			return true;
		else
			if(_reqOffset+_reqSize<=this->offset+this->size)
				return true;
			else
				return false;
	else
		return false;
}

long ReadBuffer::readBuffered(void* _dstBuff,uint64_t _reqOffset,uint64_t _reqSize){
	uint64_t start = _reqOffset - this->offset;

	if(reachedEOF){
		uint64_t howMuch = 0;
		if(_reqSize+_reqOffset <= offset+size)
			howMuch = _reqSize;
		else
			howMuch = size - start;

		memcpy(_dstBuff,this->buffer+start,howMuch);
		return howMuch;
	}
	else {
		memcpy(_dstBuff,this->buffer+start,_reqSize);
		return _reqSize;
	}
}

long FileNode::readRemote(char* _data, int64_t _offset, int64_t _size) {
	//fprintf(stderr,"BLCOK SIZE:%lu\n",_size);
	//Check offset
/*	if(_offset >= this->size)//This fucks up move operation
		return 0;*/

	/*if(readBuffer->contains(_offset,_size))
		return readBuffer->readBuffered(_data,_offset,_size);
	else {
		//Fill buffer!
		long res = BFSNetwork::readRemoteFile(readBuffer->buffer,readBuffer->capacity,_offset,this->getFullPath(),remoteHostMAC);
		if(res <= 0) {//error
			return res;
		}
		else if((unsigned long)res == readBuffer->capacity) {//NOT EOF
			readBuffer->reachedEOF = false;
			readBuffer->offset = _offset;
			readBuffer->size = readBuffer->capacity;
			return readBuffer->readBuffered(_data,_offset,_size);
		}
		else {//EOF
			readBuffer->reachedEOF = true;
			readBuffer->offset = _offset;
			readBuffer->size = res;
			return readBuffer->readBuffered(_data,_offset,_size);
		}
	}*/
	//fprintf(stderr,"RemtoeRead Req!\n");
#ifdef BFS_ZERO
	return BFSNetwork::readRemoteFile(_data,_size,_offset,this->getFullPath(),remoteHostMAC);
#else
	return BFSTcpServer::readRemoteFile(_data,_size,_offset,this->getFullPath(),remoteIP,remotePort);
#endif
}

long FileNode::writeRemote(const char* _data, int64_t _offset, int64_t _size) {
	if(_size == 0)
	  return 0;
#ifdef BFS_ZERO
	int64_t written = BFSNetwork::writeRemoteFile(
	    _data,_size,_offset,this->getFullPath(),remoteHostMAC);
#else
	int64_t written = BFSTcpServer::writeRemoteFile(
	      _data,_size,_offset,this->getFullPath(),remoteIP,remotePort);
#endif

	return written;
}

bool FileNode::rmRemote() {
#ifdef BFS_ZERO
  return BFSNetwork::deleteRemoteFile(getFullPath(),remoteHostMAC);
#else
  return BFSTcpServer::deleteRemoteFile(getFullPath(),remoteIP,remotePort) == 200;
#endif
}

bool FileNode::truncateRemote(int64_t size) {
#ifdef BFS_ZERO
  return BFSNetwork::truncateRemoteFile(getFullPath(),size, remoteHostMAC);
#else
  return BFSTcpServer::truncateRemoteFile(getFullPath(),size, remoteIP,remotePort) == 200;
#endif
}

void FileNode::makeLocal() {
  isRem.store(false);
  setRemoteHostMAC(nullptr);
  setRemoteIP("");
  setRemotePort(0);
}

void FileNode::makeRemote() {
  isRem.store(true);
}

void FileNode::deallocate() {
  //Release readbuffer
  //delete readBuffer;
  for(auto it = dataList.begin();it != dataList.end();it++) {
    char *block = *it;
    delete []block;
    block = nullptr;
  }
  dataList.clear();
  //Clean up children
  for(auto it = children.begin();it != children.end();it++) {
    FileNode* child = (FileNode*)it->second;
    delete child;
    it->second = nullptr;
  }
  children.clear();

  //Release Memory in the memory controller!
  MemoryContorller::getInstance().releaseMemory(size);
  //Update size
  size = 0;
}

bool FileNode::mustBeDeleted() {
  return mustDeleted;
}

bool FileNode::isMoving() {
  return moving;
}

void FileNode::setMoving(bool _isMoving) {
  moving.store(_isMoving);
}

/**
 * @return
 * Failures:
 * -1 Moving
 * -2 NoSpace
 * -3 InternalError
 * -4 Try Again
 * Success:
 * >= 0 written bytes
 */
long FileNode::writeHandler(const char* _data, int64_t _offset, int64_t _size, FileNode* &_afterMoveNewNode, bool _shouldMove) {
  //By default no moves happen so
  _afterMoveNewNode = nullptr;

  if(isMoving()) {
    usleep(100000l);//sleep a little and try again;
    LOG(ERROR) <<"SLEEPING FOR MOVE:"<<key;
    return -1;
    //return writeHandler(_data,_offset,_size,_afterMoveNewNode);
  }

  long written = write(_data, _offset, _size);
  if(written == -50)
    return -50;
  if(!_shouldMove && written == -1){
    LOG(ERROR)<<"Not Enough Space and not moving to other nodes:"<< this->getFullPath()<<" memUtil:"<<MemoryContorller::getInstance().getMemoryUtilization();
    ZooHandler::getInstance().publishFreeSpace();
    return -2;//No space
  }
  if(written == -1) {//No space
    string filePath = getFullPath();
    setMoving(true);//Nobody is going to write to this file anymore
    close(0);
    //LOG(INFO)<<"MOVE triggered for:"<<getFullPath()<<" offset:"<<_offset<<" size:"<<_size;
    if(FileSystem::getInstance().moveToRemoteNode(this)) {
      FileNode *newNode = FileSystem::getInstance().findAndOpenNode(filePath);
      if(newNode == nullptr|| !newNode->isRemote()) {
        //Close it! so it can be removed if needed
        if(newNode!=nullptr){
          uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)newNode);
          newNode->close(inodeNum);
        }
        LOG(ERROR)<<"HollyShit! we just moved "
            "this file:"<<filePath<<" to a remote node! but "
            "Does not exist or is not remote. IsNull:"<<((newNode==nullptr)?"null":"not Null.");
        ZooHandler::getInstance().requestUpdateGlobalView();
        return -3;
      } else {
        // all good ;)
        _afterMoveNewNode = newNode;
        //newNode->open();//Already opened by findAndOpen
        return newNode->writeRemote(_data, _offset, _size);
      }
    } else {
      LOG(ERROR) <<"MoveToRemoteNode failed:"<<getFullPath();
      setMoving(false);
      open();
      return -2;//No space
    }
  }
  //Successfull Write
  setNeedSync(true);
  return written;
}

void FileNode::fillStatWithPacket(struct stat &st,const struct packed_stat_info& stPacket) {
  st.st_dev = stPacket.st_dev;
  st.st_ino = stPacket.st_ino;
  st.st_mode = stPacket.st_mode;
  st.st_nlink = stPacket.st_nlink;
  st.st_uid = stPacket.st_uid;
  st.st_gid = stPacket.st_gid;
  st.st_rdev = stPacket.st_rdev;
  st.st_size = stPacket.st_size;
  st.st_blksize = stPacket.st_blksize;
  st.st_blocks = stPacket.st_blocks;
  st.st_atim.tv_nsec = stPacket.st_atim;
  st.st_mtim.tv_nsec = stPacket.st_mtim;
  st.st_ctim.tv_nsec = stPacket.st_ctim;
}

void FileNode::fillPackedStat(struct packed_stat_info& st) {
  //lock_guard<recursive_mutex> lk(ioMutex);
  st.st_dev = 0;
  st.st_ino = 0;
  st.st_mode = this->isDirectory() ? S_IFDIR : S_IFREG;
  st.st_nlink = 1;
  st.st_uid = this->getUID();
  st.st_gid = this->getGID();
  st.st_rdev = 0;
  st.st_size = this->getSize();
  st.st_blksize = FileSystem::blockSize;
  st.st_blocks = this->getSize() / FileSystem::blockSize;
  st.st_atim = 0x00000000;
  st.st_mtim = this->getMTime();
  st.st_ctim = this->getCTime();
}

bool FileNode::flush() {
  if(isRemote())
    return flushRemote();

  if(isFlushed)
    return true;

  Backend *backend = BackendManager::getActiveBackend();
  if(!backend){
    //LOG(ERROR)<<"No backend for flushing.";
    return true;
  }
  isFlushed = backend->put(new SyncEvent(SyncEventType::UPDATE_CONTENT,this->getFullPath()));
  return isFlushed;
}

bool FileNode::flushRemote() {
#ifdef BFS_ZERO
  return BFSNetwork::flushRemoteFile(getFullPath(), remoteHostMAC);
#else
  return BFSTcpServer::flushRemoteFile(getFullPath(), remoteIP,remotePort) == 200;
#endif
}

bool FileNode::flushed() {
  return isFlushed;
}

bool FileNode::isTransfering() {
  return transfering;
}

void FileNode::setTransfering(bool _value) {
  transfering.store(_value);
}

bool FileNode::shouldNotZooRemove(){
  return shouldNotRemoveZooHandler;
}

void FileNode::setShouldNotZooRemove(bool _value){
  shouldNotRemoveZooHandler.store(_value);
}

}  //namespace


