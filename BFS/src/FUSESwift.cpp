/*
 * FUSESwift.cpp
 *
 *  Created on: 2014-06-25
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "FUSESwift.h"
#include "params.h"
#include "filenode.h"
#include "filesystem.h"
#include <cstring>
#include <ctime>
#include <unistd.h>
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "MemoryController.h"
#include "ZooHandler.h"
#include "LoggerInclude.h"
#include "Statistics.h"

using namespace std;

namespace FUSESwift {

FileNode* createRootNode() {
  FileNode *node = new FileNode(FileSystem::delimiter, FileSystem::delimiter,true, false);
  unsigned long now = time(0);
  node->setCTime(now);
  node->setCTime(now);
  return node;
}

int swift_getattr(const char *path, struct stat *stbuff) {
  if(DEBUG_GET_ATTRIB)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", statbuf="<< stbuff;
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    //LOG(DEBUG)<<"Node not found: "<<path;
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  bool res = node->getStat(stbuff);
  //Fill Stat struct
  node->close(inodeNum);

  if(res)
  	return 0;
  else
  	return -ENOENT;
}

int swift_readlink(const char* path, char* buf, size_t size) {
  if(DEBUG_READLINK)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", link=\""<<link<<"\", size="<<size;
  int res;
  res = readlink(path, buf, size - 1);
  if (res == -1)
    return -errno;
  buf[res] = '\0';
  return 0;
}

int swift_mknod(const char* path, mode_t mode, dev_t rdev) {
  if(DEBUG_MKNOD)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<", mode="<<mode;
  int retstat = 0;
  //Check if already exist! if yes truncate it!
	string pathStr(path, strlen(path));
	FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
	if (node != nullptr)
	{
	  //Close it! so it can be removed if needed
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
    node->close(inodeNum);
    if(DEBUG_MKNOD)
      LOG(DEBUG)<<("Existing file! truncating to 0!");
		return swift_ftruncate(path,0,nullptr);
	}

	string name = FileSystem::getInstance().getFileNameFromPath(path);
  if(!FileSystem::getInstance().nameValidator(name)) {
    if(DEBUG_MKNOD)
      LOG(ERROR)<<"Failure, can't create file: invalid name:"<<path;
    return -EINVAL;
  }

	//Not existing
  if (S_ISREG(mode)) {
    if(MemoryContorller::getInstance().getMemoryUtilization() < 0.9) {

      FileNode *newFile = FileSystem::getInstance().mkFile(pathStr,false,true);
      if (newFile == nullptr) {
        LOG(ERROR)<<"Failure, mkFile (newFile is nullptr)";
        return -ENOENT;
      }

      uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);

      //File created successfully
      newFile->setMode(mode); //Mode
      unsigned long now = time(0);
      newFile->setMTime(now); //MTime
      newFile->setCTime(now); //CTime
      //Get Context
      struct fuse_context fuseContext = *fuse_get_context();
      newFile->setGID(fuseContext.gid);    //gid
      newFile->setUID(fuseContext.uid);    //uid

      if(DEBUG_MKNOD)
        LOG(DEBUG)<<"MKNOD:"<<(path==nullptr?"null":path)<<" created Locally, returning:"<<newFile;

      newFile->close(inodeNum);//It's a create and open operation
    } else {//Remote file
      if(DEBUG_MKNOD)
        LOG(DEBUG) <<"NOT ENOUGH SPACE, GOINT TO CREATE REMOTE FILE. UTIL:"<<MemoryContorller::getInstance().getMemoryUtilization();
      if(FileSystem::getInstance().createRemoteFile(pathStr))
        retstat = 0;
      else{
        LOG(ERROR) <<"Failure, create remote file failed.";
        retstat = -EIO;
      }
    }
  } else {
    retstat = -ENOENT;
    LOG(ERROR)<<"Failure, expects regular file";
  }
  return retstat;
}

/** Create a directory
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 * */
int swift_mkdir(const char* path, mode_t mode) {
  int retstat = 0;
  mode = mode | S_IFDIR;
  if(DEBUG_MKDIR)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<", mode="<<mode;

  if (S_ISDIR(mode)) {
    string pathStr(path, strlen(path));
    string name = FileSystem::getInstance().getFileNameFromPath(path);
    if(!FileSystem::getInstance().nameValidator(name)) {
      if(DEBUG_MKDIR)
        LOG(ERROR)<<"Can't create directory: invalid name:"<<path;
      return -EINVAL;
    }
    FileNode *newDir = FileSystem::getInstance().mkDirectory(pathStr,false);
    if (newDir == nullptr){
      LOG(ERROR)<<" mkdir (newDir is nullptr)";
      return -ENOENT;
    }
    //Directory created successfully
    newDir->setMode(mode); //Mode
    unsigned long now = time(0);
    newDir->setMTime(now); //MTime
    newDir->setCTime(now); //CTime
    //Get Context
    struct fuse_context fuseContext = *fuse_get_context();
    newDir->setGID(fuseContext.gid);    //gid
    newDir->setUID(fuseContext.uid);    //uid
  } else {
    retstat = -ENOENT;
    LOG(ERROR)<<"expects dir";
  }

  return retstat;
}

int swift_unlink(const char* path) {
  if(DEBUG_UNLINK)
    LOG(DEBUG)<<"(path=\""<<(path==nullptr?"null":path)<<"\")";

  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    LOG(ERROR)<<"Node not found: "<<path;
    return -ENOENT;
  }

  //Close it! so it can be removed if needed
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);

  if(node->isDirectory()){
    node->close(inodeNum);
    return -EISDIR;
  }


  node->close(inodeNum);
  LOG(DEBUG)<<"SIGNAL DELETE FROM UNLINK Key:"<<node->getFullPath()<<" isOpen?"<<node->concurrentOpen()<<" isRemote():"<<node->isRemote();
  if(!FileSystem::getInstance().signalDeleteNode(node,true)){
    LOG(ERROR)<<"DELETE FAILED FOR:"<<path;
    return -EIO;
  }
  ZooHandler::getInstance().requestUpdateGlobalView();

  if(DEBUG_UNLINK)
  	LOG(DEBUG)<<"Removed "<<path;

  return 0;
}

int swift_rmdir(const char* path) {
  if(DEBUG_RMDIR)
    LOG(DEBUG)<<"(path=\""<<(path==nullptr?"null":path)<<"\")";

  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    LOG(ERROR)<<"Node not found: "<< path;
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);

  if(node->childrenSize() > 0){//Non empty directory
    node->close(inodeNum);
    return -ENOTEMPTY;
  }

  LOG(DEBUG)<<"SIGNAL DELETE From RMDIR:"<<node->getFullPath();
  node->close(inodeNum);

  if(!FileSystem::getInstance().signalDeleteNode(node,true)){
    LOG(ERROR)<<"DELETE FAILED FOR:"<<path;
    return -EIO;
  }
  ZooHandler::getInstance().requestUpdateGlobalView();

  return 0;
}
/*
int swift_symlink(const char* from, const char* to) {
}*/

/**
 * TODO:
 * XXX: causes some applications like gedit to fail!
 * They depend on writing on a tempfile and then rename it to
 * the original file.
 */
int swift_rename(const char* from, const char* to) {
  //if(DEBUG_RENAME)
  LOG(ERROR)<<"RENAME NOT IMPLEMENTED: from:"<<from<<" to="<<to;
  //Disabling rename
  return -EIO;
/*
  string oldPath(from, strlen(from));
  string newPath(to, strlen(to));
  string newName = FileSystem::getInstance().getFileNameFromPath(newPath);
  if(!FileSystem::getInstance().nameValidator(newName)) {
    if(DEBUG_RENAME)
      log_msg("\nbb_rename can't rename file: invalid name:\"%s\"\n", to);
    return EBFONT;
  }

  if(!FileSystem::getInstance().tryRename(oldPath,newPath)) {
    log_msg("\nbb_rename failed.\n");
    return -ENOENT;
  }
  else {
    if(DEBUG_RENAME)
      log_msg("bb_rename successful.\n");
    //FileNode* node = FileSystem::getInstance().getNode(newPath);
    //log_msg("bb_rename MD5:%s\n",node->getMD5().c_str());
    return 0;
  }
*/
}

/*int swift_link(const char* from, const char* to) {
}*/

int swift_chmod(const char* path, mode_t mode) {
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if(node == nullptr) {
    LOG(ERROR)<<"Failure, not found: "<<path;
    return -ENOENT;
  }
  //Assign inode number
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  if(inodeNum == 0) {
    LOG(ERROR)<<"Error in assigning inodeNum, or opening file.";
    return -ENOENT;
  }
  //Update mode
  node->setMode(mode);
  node->close(inodeNum);
  return 0;
}

int swift_chown(const char* path, uid_t uid, gid_t gid) {
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if(node == nullptr) {
    LOG(ERROR)<<"Failure, not found: "<<path;
    return -ENOENT;
  }
  //Assign inode number
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  if(inodeNum == 0) {
    LOG(ERROR)<<"Error in assigning inodeNum, or opening file.";
    return -ENOENT;
  }
  //Update mode
  node->setGID(gid);
  node->setUID(uid);
  node->close(inodeNum);
  return 0;
}

int swift_truncate(const char* path, off_t size) {
  if(DEBUG_TRUNCATE)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", size:"<<size;
  return swift_ftruncate(path,size,nullptr);
}

/*int swift_utime(const char* path, struct utimbuf* ubuf) {
}*/


int swift_open(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_OPEN)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", fi="<<fi<<" fh="<<fi->fh;
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    LOG(ERROR)<<"Failure, cannot Open Node not found: "<<path;
    fi->fh = 0;
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  if(inodeNum!=0) {
		fi->fh = inodeNum;
		//LOG(ERROR)<<"ptr:"<<node;
		//LOG(ERROR)<<"OPEN:"<<node->getFullPath();
		return 0;
  }
  else {
    fi->fh = 0;
    LOG(ERROR)<<"Error in assigning inodeNum, or opening file.";
  	return -ENOENT;
  }
}

int swift_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
  if(DEBUG_READ)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", buf="<<buf<<", size="<<
    size<<", offset="<<offset<<", fi="<<fi;
  //Handle path
  if(path == nullptr && fi->fh == 0) {
    LOG(ERROR)<<"fi->fh is null";
    return -ENOENT;
  }
  //Get associated FileNode*
  FileNode* node = (FileNode*) FileSystem::getInstance().getNodeByINodeNum(fi->fh);

  //Empty file
  if((!node->isRemote()&&node->getSize() == 0)||size == 0) {
    LOG(ERROR)<<"Read from:path="<<node->getFullPath()<<", buf="<<buf<<", size="<<size<<", offset="<<offset<<", EOF";
    return 0;
  }
  long readBytes = 0;
  if(node->isRemote())
  	readBytes = node->readRemote(buf,offset,size);
  else
  	readBytes = node->read(buf,offset,size);

  if(readBytes >= 0) {
    if(DEBUG_READ)
      LOG(DEBUG)<<"Successful read from:path=\""<<(path==nullptr?"null":path)<<"\", buf="<<buf<<", size="<<size<<", offset="<<offset;
    Statistics::reportRead(size);
    return readBytes;
  }
  else {
    LOG(ERROR)<<"Error in reading: path=\""<<(path==nullptr?"null":path)<<"\", buf="<<buf<<", size="<<size<<", offset="<<offset<<" RetValue:"<<readBytes<<" nodeSize:"<<node->getSize();
    return -EIO;
  }
}
int swift_write_error_tolerant(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
  int retry = 3;
  while(retry>0) {
    int res = swift_write(path, buf, size, offset,fi);
    if (res != -EIO){
      if(res > 0)
    	  Statistics::reportWrite(size);
      return res;
    }
    LOG(ERROR)<<"write failed for:"<<(path==nullptr?"null":path)<<" retrying:"<<(3-retry+1)<<" Time.";
    retry--;
  }
  return -EIO;
}
int swift_write(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
  if(DEBUG_WRITE)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", buf="<<buf<<", size="<<
    size<<", offset="<<offset<<", fi="<<fi;
  //Handle path
  if (path == nullptr && fi->fh == 0) {
    LOG(ERROR)<<"\nswift_write: fi->fh is null";
    return -ENOENT;
  }
  //Get associated FileNode*
  FileNode* node = (FileNode*) FileSystem::getInstance().getNodeByINodeNum(fi->fh);


  long written = 0;

  if (node->isRemote())
    written = node->writeRemote(buf, offset, size);
  else {
    /**
     * @return
     * Failures:
     * -1 Moving
     * -2 NoSpace
     * -3 InternalError
     * Success:
     * >= 0 written bytes
     */
    FileNode* afterMove = nullptr;
    written = node->writeHandler(buf, offset, size, afterMove);
    if(afterMove) {//set fi->fh
      FileSystem::getInstance().replaceAllInodesByNewNode((intptr_t)node,(intptr_t)afterMove);
      node = afterMove;
    }
  }

  if (written != (long) size) {
    //LOG(ERROR)<<"Error in writing to:"<<node->getName()<< " IsRemote?"<<node->isRemote()<<" Code:"<<written;
    if (written == -1) //Moving
      //return -EAGAIN;
      return swift_write(path,buf,size,offset,fi);
    else if(written == -2){ // No space
      LOG(ERROR)<<"No space left, error in writing to:"<<node->getName()<< " IsRemote?"<<node->isRemote()<<" Code:"<<written;
      return -ENOSPC;
    }
    else {//Internal IO Error
      LOG(ERROR)<<"Internal IO Error, error in writing to:"<<node->getName()<< " IsRemote?"<<node->isRemote()<<" Code:"<<written;
      return -EIO;
    }
  }

  return written;
}

/*int swift_statfs(const char* path, struct statvfs* stbuf) {
}*/

int swift_flush(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_FLUSH)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", fi="<<fi;
  int retstat = 0;
  //Get associated FileNode*
  //FileNode* node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);
  //LOG(ERROR)<<"FLUSH is not implemeted! path="<<(path==nullptr?"null":path)<<" fi->fh="<<fi->fh;

  return retstat;
}

int swift_release(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_RELEASE)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", fi="<<fi;
  int retstat = 0;
  //Handle path
  if(path == nullptr && fi->fh == 0) {
    LOG(ERROR)<<"fi->fh is null.";
    return ENOENT;
  }
  //Get associated FileNode*
  FileNode* node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);
  //Update modification time
  node->setMTime(time(0));
  //Node might get deleted after close! so no reference to it anymore!
  //Debug info backup
  string pathStr = node->getFullPath();
  //LOG(ERROR)<<"CLOSE: ptr:"<<node;
  //LOG(ERROR)<<node->getFullPath();
  //Now we can safetly close it!
  node->close(fi->fh);
  //we can earse ionode num from map as well
  //FileSystem::getInstance().removeINodeEntry(fi->fh);

  return retstat;
}

int swift_fsync(const char* path, int isdatasynch, struct fuse_file_info* fi) {
  //Get associated FileNode*
  //FileNode* node = (FileNode*)fi->fh;
  LOG(ERROR)<<"FSYNC not implemeted: path=\""<<(path==nullptr?"null":path)<<"\", fi="<<fi<<" isdatasynch:"<<isdatasynch;
  return 0;
}

int swift_setxattr(const char* path, const char* name, const char* value,
    size_t size, int flags) {
  LOG(ERROR)<<"SETXATTR not implemeted: path="<<(path==nullptr?"null":path)<<" name="<<name
      <<" value="<<value<<" size="<<size<<" flags="<<flags;
  return 1;
}

/*int swift_getxattr(const char* path, const char* name, char* value,
    size_t size) {
}

int swift_listxattr(const char* path, char* list, size_t size) {
}

int swift_removexattr(const char* path, const char* name) {
}*/

int swift_opendir(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_OPENDIR)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", fi="<<fi;
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    LOG(ERROR)<<"Node not found: "<< path;
    fi->fh = 0;
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  if(inodeNum != 0) {
    fi->fh = inodeNum;
    return 0;
  } else {
    fi->fh = 0;
    LOG(ERROR)<<"Error in assigning inodeNum or openning dir.";
    return -EIO;
  }
}

int swift_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi) {
  if(DEBUG_READDIR)
    LOG(DEBUG)<<"path="<<(path==nullptr?"null":path)<<", buf="<<buf<<", filler="<<
    filler<<", offset="<<offset<<", fi="<<fi<<", fi->fh="<<fi->fh;
  //Get associated FileNode*
  FileNode* node = nullptr;
  //Handle path
  if(fi->fh == 0){
    LOG(ERROR)<<"fi->fh is null";
    return ENOENT;
  }

  //Get Node
  node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);
  if(node == nullptr){
    LOG(ERROR)<<"node is null";
    return -ENOENT;
  }

  int retstat = 0;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  node->childrenLock();
  auto it = node->childrendBegin2();
  for (; it != node->childrenEnd2(); it++) {
    FileNode* entry = (FileNode*) it->second;
    struct stat st;
    /**
     * void *buf, const char *name,
     const struct stat *stbuf, off_t off
     */
    if(!entry->open()) {//protect against delete
      continue;
    }
    uint64_t inodeNumEntry = FileSystem::getInstance().assignINodeNum((intptr_t)entry);

    entry->getStat(&st);
    if (filler(buf, entry->getName().c_str(), &st, 0)) {
      LOG(ERROR)<<"Filler error.";
      entry->close(inodeNumEntry);//protect against delete
      node->childrenUnlock();
      return -EIO;
    }
    entry->close(inodeNumEntry);//protect against delete
  }
  node->childrenUnlock();

  return retstat;
}

int swift_releasedir(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_RELEASEDIR)
    LOG(DEBUG)<<"path=\""<<(path==nullptr?"null":path)<<"\", fi="<<fi;
  int retstat = 0;
  //Handle path
  if(path == nullptr && fi->fh == 0){
    LOG(ERROR)<<"fi->fh is null";
    return ENOENT;
  }

  //Get associated FileNode*
  FileNode* node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);
  //Update modification time
  node->setMTime(time(0));
  node->close(fi->fh);
  //we can earse ionode num from map as well
  //FileSystem::getInstance().removeINodeEntry(fi->fh);

  return retstat;
}

int swift_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi) {
  LOG(ERROR)<<"FSYNCDIR not implemeted path:"<<(path==nullptr?"null":path)<<", fi->fh:"<<fi->fh<<", isdatasynch:"<<datasync;
  return 0;
}

void* swift_init(struct fuse_conn_info* conn) {
  //Initialize file system
  FileNode* rootNode = createRootNode();
  FileSystem::getInstance().initialize(rootNode);
  //Get Context
  struct fuse_context fuseContext = *fuse_get_context();
  rootNode->setGID(fuseContext.gid);
  rootNode->setUID(fuseContext.uid);
  //Log
  if(DEBUG_INIT)
    LOG(INFO)<<"Fuse Initialization";

  LOG(INFO)<<"Starting SyncThreads";
  //Start SyncQueue threads
  UploadQueue::getInstance().startSynchronization();
  DownloadQueue::getInstance().startSynchronization();
  LOG(INFO)<<"SyncThreads running...";
  //Start Zoo Election
  ZooHandler::getInstance().startElection();
  LOG(INFO)<<"ZooHandler running...";

  return nullptr;
}

/**
 * we just give all the permissions
 * TODO: we should not just give all the permissions
 */
int swift_access(const char* path, int mask) {
  if(DEBUG_ACCESS)
    LOG(DEBUG)<<"path="<<(path==nullptr?"null":path)<<", mask="<<mask;
  //int retstat = R_OK | W_OK | X_OK | F_OK;
  int retstat = 0;
  return retstat;
}

/*int swift_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  if(S_ISREG(mode)) {
    if(swift_open(path,fi) == 0)
      return 0;//Success
    dev_t dev = 0;
    if(swift_mknod(path,mode,dev)==0)
      return swift_open(path,fi);
  } else if(S_ISDIR(mode)){
    if(swift_opendir(path,fi) == 0)
      return 0;
    if(swift_mkdir(path,mode)==0)
      return swift_opendir(path,fi);
  } else
    LOG(ERROR)<<"Failure, neither a directory nor a regular file.";

  LOG(ERROR)<<"Failure, Error in create.";
  return -EIO;
}*/

int swift_ftruncate(const char* path, off_t size, struct fuse_file_info* fi) {
  if(DEBUG_TRUNCATE)
    LOG(DEBUG)<<"path="<<(path==nullptr?"null":path)<<", fi:"<<fi<<" newsize:"<<size;
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    LOG(ERROR)<<"Error swift_ftruncate: Node not found: "<<path;
    return -ENOENT;
  }
  else if(DEBUG_TRUNCATE)
    LOG(DEBUG)<<"Truncating:"<<(path==nullptr?"null":path)<<" from:"<<node->getSize()<<" to:"<<size<<" bytes";

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);

  //Checking Space availability
  int64_t diff = size - node->getSize();
  if(diff > 0)
    if(!MemoryContorller::getInstance().checkPossibility(diff)) {
      LOG(ERROR)<<"Ftruncate failed(not enough space): "<<(path==nullptr?"null":path)<<" newSize:"<<node->getSize()<<" MemUtil:"<<
    		  MemoryContorller::getInstance().getMemoryUtilization()<<" TotalMem:"<<MemoryContorller::getInstance().getTotal()<<
			  " MemAvail:"<<MemoryContorller::getInstance().getAvailableMemory();
      node->close(inodeNum);
      return -ENOSPC;
    }


  bool res;
  if(node->isRemote())
    res = node->truncateRemote(size);
  else
    res = node->truncate(size);
  node->close(inodeNum);

  if(!res) {
    LOG(ERROR)<<"Ftruncate failed: "<<(path==nullptr?"null":path)<<" newSize:"<<node->getSize();
    return EIO;
  }
  else
    return 0;
}

/*int swift_fgetattr(const char* path, struct stat* statbuf,
    struct fuse_file_info* fi) {
}

int swift_lock(const char* arg1, struct fuse_file_info* arg2, int cmd,
    struct flock* arg4) {
}

int swift_utimens(const char* path, const struct timespec tv[2]) {
}

int swift_bmap(const char* arg1, size_t blocksize, uint64_t* idx) {
}

int swift_ioctl(const char* arg1, int cmd, void* arg3,
    struct fuse_file_info* arg4, unsigned int flags, void* data) {
}

int swift_poll(const char* arg1, struct fuse_file_info* arg2,
    struct fuse_pollhandle* ph, unsigned * reventsp) {
}

int swift_write_buf(const char* arg1, struct fuse_bufvec* buf, off_t off,
    struct fuse_file_info* arg4) {
}

int swift_read_buf(const char* arg1, struct fuse_bufvec** bufp, size_t size,
    off_t off, struct fuse_file_info* arg5) {
}

int swift_flock(const char* arg1, struct fuse_file_info* arg2, int op) {
}

int swift_fallocate(const char* path, int mode, off_t offset, off_t length,
    struct fuse_file_info* fi) {
}*/

} //FUSESwift namespace
