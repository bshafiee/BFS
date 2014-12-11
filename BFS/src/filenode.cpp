/*
 * fileNode.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "filenode.h"
#include <cstring>
#include <sstream>      // std::istringstream
#include "log.h"
#include "filesystem.h"
#include <Poco/MD5Engine.h>
#include "UploadQueue.h"
#include "MemoryController.h"
#include "BFSNetwork.h"
#include "ZooHandler.h"
#include "BFSTcpServer.h"
#include "Timer.h"

using namespace std;

namespace FUSESwift {

FileNode::FileNode(string _name,string _fullPath,bool _isDir, bool _isRemote):Node(_name,_fullPath),
    isDir(_isDir),size(0),refCount(0),blockIndex(0), needSync(false),
    mustDeleted(false), hasInformedDelete(false),isRem(_isRemote),
    mustInformRemoteOwner(true), moving(false), isUPLOADING(false) {
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
  int childrenSize = children.size();
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
  //Acquire lock
  lock_guard <recursive_mutex> lock(ioMutex);

  //Check Storage space Availability
  size_t newReqMemSize = (_offset + _size > size) ? _offset + _size - size : 0;
  if (newReqMemSize > 0)
    if (!MemoryContorller::getInstance().requestMemory(newReqMemSize))
      return -1;

  size_t retValue = 0;
  size_t backupSize = _size;
  if (_offset < size) { //update existing (unlikely)
    //Find correspondent block
    size_t blockNo = _offset / FileSystem::blockSize;
    unsigned int index = _offset - blockNo * FileSystem::blockSize;

    char* block = dataList[blockNo];

    //Start filling
    while (_size > 0) {
      size_t left = FileSystem::blockSize - index;//Left bytes in the last block
      size_t howMuch = (_size <= left) ? _size : left;
      memcpy(block + index, _data + (backupSize - _size), howMuch);
      _size -= howMuch;
      index += howMuch;
      retValue += howMuch;
      blockNo++;
      //Increase size and blockIndex if in the last block
      if (index > blockIndex && blockNo == dataList.size()) {
        size += index - blockIndex;
        blockIndex = index;
        if (blockIndex >= FileSystem::blockSize)
          blockIndex = 0;
      }
      if (index >= FileSystem::blockSize && blockNo < dataList.size()) {
        block = dataList[blockNo];
        index = 0;
      } else if (index >= FileSystem::blockSize && blockNo >= dataList.size()) {
        //We should allocate a new block
        char* block = new char[FileSystem::blockSize];
        dataList.push_back(block);
        //block index shouuld have alreadey be reset to 0
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
      size_t left = FileSystem::blockSize - blockIndex;	//Left bytes in the last block
      size_t howMuch = (_size <= left) ? _size : left;
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

bool FUSESwift::FileNode::truncate(size_t _size) {
  int64_t truncateDiff = _size - getSize();

  if(truncateDiff == 0)
    return true;//Nothing to do

  //Do we hace enough memory?
  if(!MemoryContorller::getInstance().checkPossibility(truncateDiff))
    return false;
  else if(_size > size) { //Expand
    //we just call write with '\0' chars buffers
    size_t diff = _size-size;
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
    size_t diff = size-_size;
    while(diff > 0) {
      if(blockIndex >= diff) {
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

  LOG(ERROR)<<"Open:"<<key<<" refCount:"<<refCount<<" ptr:"<<this;
  return true;
}

void FileNode::close(uint64_t _inodeNum) {
  refCount--;

  LOG(ERROR)<<"Close refCount:"<<refCount<<" ptr:"<<this;
  /**
   * add event to sync queue if all the references to this file
   * are closed and it actually needs updating!
   */
  if(refCount < 0){
    LOG(ERROR)<<"SPURIOUS CLOSE:"<<key<<" ptr:"<<this;
  }
  if(refCount == 0) {
  	//If all refrences to this files are deleted so it can be deleted

  	if(mustDeleted){
  	  LOG(ERROR)<<"SIGNAL FROM CLOSE KEY:"<<getName()<<" isOpen?"<<refCount<<" isRemote():"<<isRemote()<<" ptr:"<<this<<" isuploading:"<<isUPLOADING;
  	  FileSystem::getInstance().signalDeleteNode(this,mustInformRemoteOwner);//It's close now! so will be removed
  	  /// NOTE AFTER THIS LINE ALL OF DATA IN THIS FILE ARE INVALID ///
  	  ZooHandler::getInstance().requestUpdateGlobalView();
  	} else{//Should not be deleted
      if(needSync && !isRemote()){
        if(UploadQueue::getInstance().push(new SyncEvent(SyncEventType::UPDATE_CONTENT,this->getFullPath())))
          this->setNeedSync(false);
        else
          LOG(ERROR)<<"\n\n\nHOLLY SHITTTTTTTTTTTTTTTTTTTTTT\n\n\n\n"<<key;
      }
  	}
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
  //LOG(ERROR)<<"NeedSync:"<<_need<<" for:"<<key;
}

bool FileNode::isOpen() {
  return refCount>0;
}

std::string FileNode::getMD5() {
  if(size <= 0)
    return "";
  lock_guard<recursive_mutex> lk(ioMutex);
  Poco::MD5Engine md5;
  md5.reset();
  size_t blockSize = FileSystem::blockSize;
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

	if(!isRem){
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
	}
	else{
#ifdef BFS_ZERO
		bool res = BFSNetwork::readRemoteFileAttrib(stbuff,this->getFullPath(),remoteHostMAC);
#else
		bool res = BFSTcpServer::attribRemoteFile(stbuff,this->getFullPath(),remoteIP,remotePort);
#endif
		if(!res){
		  //fprintf(stderr,"ISFILEOPEN? %d   ",isOpen());
		  string test;
		  for(int i=0;i<100;i++)
		    test = this->getName();
			fprintf(stderr,"GetRemoteAttrib failed for:%s\n",this->getFullPath().c_str());
			for(int i=0;i<100;i++)
			  test = this->getName();
		}
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

long FileNode::readRemote(char* _data, size_t _offset, size_t _size) {
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
	Timer t;
  t.begin();
	long res = BFSTcpServer::readRemoteFile(_data,_size,_offset,this->getFullPath(),remoteIP,remotePort);
	t.end();
  //cout<<"RECV DONE IN:"<<t.elapsedMicro()<<" microseconds."<<endl;
  return res;
#endif
}

long FileNode::writeRemote(const char* _data, size_t _offset, size_t _size) {
	if(_size == 0)
	  return 0;
#ifdef BFS_ZERO
	unsigned long written = BFSNetwork::writeRemoteFile(
	    _data,_size,_offset,this->getFullPath(),remoteHostMAC);
#else
	unsigned long written = BFSTcpServer::writeRemoteFile(
	      _data,_size,_offset,this->getFullPath(),remoteIP,remotePort);
#endif
	if(written!=_size)
	  return -3;//ACK error
	else
	  return written;
}

bool FileNode::rmRemote() {
#ifdef BFS_ZERO
  return BFSNetwork::deleteRemoteFile(getFullPath(),remoteHostMAC);
#else
  return BFSTcpServer::deleteRemoteFile(getFullPath(),remoteIP,remotePort);
#endif
}

bool FileNode::truncateRemote(size_t size) {
#ifdef BFS_ZERO
  return BFSNetwork::truncateRemoteFile(getFullPath(),size, remoteHostMAC);
#else
  return BFSTcpServer::truncateRemoteFile(getFullPath(),size, remoteIP,remotePort);
#endif
}

void FileNode::makeLocal() {
  isRem.store(false);
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
long FileNode::writeHandler(const char* _data, size_t _offset, size_t _size, FileNode* &_afterMoveNewNode) {
  //By default no moves happen so
  _afterMoveNewNode = nullptr;

  if(isMoving()) {
    usleep(1000);//sleep a little and try again;
    LOG(ERROR) <<"SLEEPING FOR MOVE";
    return -1;
    //return writeHandler(_data,_offset,_size,_afterMoveNewNode);
  }

  long written = write(_data, _offset, _size);
  if(written == -1) {//No space
    LOG(ERROR) <<"NOT ENOUGH SPACE, MOVING FILE."<<key<<" curSize:"<<size<<" UTIL:"<<MemoryContorller::getInstance().getMemoryUtilization();

    string filePath = getFullPath();
    setMoving(true);//Nobody is going to write to this file anymore
    close(0);

    if(FileSystem::getInstance().moveToRemoteNode(this)) {
      FileNode *newNode = FileSystem::getInstance().findAndOpenNode(filePath);
      if(newNode == nullptr|| !newNode->isRemote()) {
        //Close it! so it can be removed if needed
        uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)newNode);
        newNode->close(inodeNum);
        LOG(ERROR)<<"HollyShit! we just moved "
            "this file:"<<filePath<<" to a remote node! but "
            "Does not exist or is not remote";
        return -3;
      } else {
        // all good ;)
        _afterMoveNewNode = newNode;
        //newNode->open();//Already opened by findAndOpen
        return newNode->writeRemote(_data, _offset, _size);
      }
    } else {
      LOG(ERROR) <<"MoveToRemoteNode failed.";
      setMoving(false);
      open();
      return -2;
    }
  }
  //Successfull Write
  setNeedSync(true);
  return written;
}

} //namespace

