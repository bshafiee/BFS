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

using namespace std;

namespace FUSESwift {

FileNode::FileNode(string _name,bool _isDir, FileNode* _parent, bool _isRemote):Node(_name,_parent),
    isDir(_isDir),size(0),refCount(0),blockIndex(0), needSync(false),mustDeleted(false), isRem(_isRemote) {
	/*if(_isRemote)
		readBuffer = new ReadBuffer(READ_BUFFER_SIZE);*/
}

FileNode::~FileNode() {
  //We need Delete lock before releasing this document.
  lockDelete();

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
  //Unlock delete no unlock when being removed
  //unlockDelete();
  //Release Memory in the memory controller!
  MemoryContorller::getInstance().releaseMemory(size);
}

FileNode* FileNode::getParent() {
  return (FileNode*)parent;
}

void FileNode::metadataAdd(std::string _key, std::string _value) {
	lock_guard<mutex> lock(metadataMutex);
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
  auto it = metadata.find(_key);
  return (it == metadata.end())? "": it->second;
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
  //Acquire lock
  lock_guard < mutex > lock(ioMutex);

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
  	lock_guard<mutex> lock(ioMutex);
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
    FileNode* parent = this;
    FileSystem::getInstance().rmNode(parent, existingNodes);
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
  if(refCount == 0) {
  	if(needSync)
  		if(UploadQueue::getInstance().push(new SyncEvent(SyncEventType::UPDATE_CONTENT,this,this->getFullPath())))
  			this->setNeedSync(false);

  	//If all refrences to this files are deleted so it can be deleted
  	if(mustDeleted) {
  		string path = this->getFullPath();
  		FileNode* parent = FileSystem::getInstance().findParent(path);
  		FileNode* thisNode = FileSystem::getInstance().getNode(path);
  		FileSystem::getInstance().rmNode(parent,thisNode);
  	}
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
  //Acquire lock
  lock_guard < mutex > lock(ioMutex);
  needSync = _need;
}

std::string FUSESwift::FileNode::getFullPath() {
  string path = this->getName();
  FileNode* par = getParent();
  while(par != nullptr) {
    path = par->getName() + (par->getParent()==nullptr?"":FileSystem::delimiter) + path;
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
  size_t blockSize = FileSystem::blockSize;
  for(uint i=0;i<dataList.size()-1;i++)
    md5.update(dataList[i],blockSize);
  //last block might not be full
  md5.update(dataList[dataList.size()-1],blockIndex);
  return Poco::DigestEngine::digestToHex(md5.digest());
}

void FileNode::lockDelete() {
  deleteMutex.lock();
}

void FileNode::unlockDelete() {
  deleteMutex.unlock();
}

bool FileNode::signalDelete() {
	mustDeleted = true;
	if(isOpen())
	  return true;//will be deleted on close
	//Otherwise delete it right away
	FileNode* thisPtr = this;

	if(isRemote())
    return rmRemote();
	else
	  return FileSystem::getInstance().rmNode(thisPtr);
}

bool FileNode::isRemote() {
	return isRem;
}
const unsigned char* FileNode::getRemoteHostMAC() {
	return this->remoteHostMAC;
}

void FileNode::setRemoteHostMAC(const unsigned char *_mac) {
	memcpy(this->remoteHostMAC,_mac,6*sizeof(char));
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
		bool res = BFSNetwork::readRemoteFileAttrib(stbuff,this->getFullPath(),remoteHostMAC);
		if(!res)
			fprintf(stderr,"GetRemoteAttrib failed for:%s\n",this->getFullPath().c_str());
		else {//update local info!
			//get io mutex to update size
			/*ioMutex.lock();//We should not do this, it will fuck move operations
			this->size = stbuff->st_size;
			ioMutex.unlock();*/

			//Update meta info
			stbuff->st_blksize = FileSystem::blockSize;
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

	return BFSNetwork::readRemoteFile(_data,_size,_offset,this->getFullPath(),remoteHostMAC);
}

long FileNode::writeRemote(const char* _data, size_t _offset, size_t _size) {
	//fprintf(stderr,"BLCOK SIZE:%lu\n",_size);
	//Check offset
/*	if(_offset > this->size)
		return 0;*///This fuck move operation
	if(_size == 0)
	  return 0;
	unsigned long written = BFSNetwork::writeRemoteFile(
	    _data,_size,_offset,this->getFullPath(),remoteHostMAC);
	if(written!=_size)
	  return -2;//ACK error
	else
	  return written;
}

bool FileNode::rmRemote() {
  return BFSNetwork::deleteRemoteFile(getFullPath(),remoteHostMAC);
}

bool FileNode::truncateRemote(size_t size) {
  return BFSNetwork::truncateRemoteFile(getFullPath(),size, remoteHostMAC);
}

void FileNode::makeLocal() {
  isRem.store(false);
}

void FileNode::makeRemote() {
  isRem.store(true);
}

void FileNode::deallocate() {
  //We need Delete lock before releasing this document.
  lockDelete();
  //Get IO MUTEX
  lock_guard<mutex> lk_guard(ioMutex);

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
  //Unlock delete no unlock when being removed
  unlockDelete();
  //Release Memory in the memory controller!
  MemoryContorller::getInstance().releaseMemory(size);
  //Update size
  size = 0;
}

} //namespace


