/*
 * FUSESwift.cpp
 *
 *  Created on: 2014-06-25
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "FUSESwift.h"
#include "filenode.h"
#include "log.h"
#include "filesystem.h"
#include <cstring>
#include <ctime>
#include <unistd.h>
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "MemoryController.h"
#include "ZooHandler.h"
#include "LoggerInclude.h"

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
    log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuff);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    if(DEBUG_GET_ATTRIB)
      log_msg("swift_getattr: Node not found: %s\n", path);
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  bool res = node->getStat(stbuff);
  //Fill Stat struct
  node->close(inodeNum);

  if(DEBUG_GET_ATTRIB)
    log_stat(stbuff);

  if(res)
  	return 0;
  else
  	return -ENOENT;
}

int swift_readlink(const char* path, char* buf, size_t size) {
  log_msg("bb_readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link, size);
  int res;
  res = readlink(path, buf, size - 1);
  if (res == -1)
    return -errno;
  buf[res] = '\0';
  return 0;
}

int swift_mknod(const char* path, mode_t mode, dev_t rdev) {
  int retstat = 0;
  LOG(ERROR)<<"MKNOD:"<<path;

  //Check if already exist! if yes truncate it!
	string pathStr(path, strlen(path));
	FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
	if (node != nullptr)
	{
	  //Close it! so it can be removed if needed
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
    node->close(inodeNum);

	  LOG(ERROR)<<("Failure, Existing file! truncating to 0!");
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

      newFile->close(inodeNum);//It's a create and open operation
    } else {//Remote file
      LOG(ERROR) <<"Failure, NOT ENOUGH SPACE, GOINT TO CREATE REMOTE FILE. UTIL:"<<MemoryContorller::getInstance().getMemoryUtilization();
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
    log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

  if (S_ISDIR(mode)) {
    string pathStr(path, strlen(path));
    string name = FileSystem::getInstance().getFileNameFromPath(path);
    if(!FileSystem::getInstance().nameValidator(name)) {
      if(DEBUG_MKDIR)
        log_msg("\nbb_mkdir can't create directory: invalid name:\"%s\"\n", path);
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
    log_msg("bb_mkdir expects dir\n");
  }

  return retstat;
}

int swift_unlink(const char* path) {
  if(DEBUG_UNLINK)
    log_msg("bb_unlink(path=\"%s\")\n", path);

  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    log_msg("swift_unlink: Node not found: %s\n", path);
    return -ENOENT;
  }
  //Close it! so it can be removed if needed
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  node->close(inodeNum);

  LOG(ERROR)<<"SIGNAL DELETE FROM UNLINK Key:"<<node->getName()<<" isOpen?"<<node->isOpen()<<" isRemote():"<<node->isRemote();
  if(!FileSystem::getInstance().signalDeleteNode(node,true)){
    LOG(ERROR)<<"DELETE FAILED FOR:"<<path;
    return -EIO;
  }
  ZooHandler::getInstance().requestUpdateGlobalView();

  if(DEBUG_UNLINK)
    //log_msg("Removed %d nodes from %s file.\n", remNodes, path);
  	log_msg("Removed %s file.\n", path);

  return 0;
}

int swift_rmdir(const char* path) {
  if(DEBUG_RMDIR)
    log_msg("bb_rmdir(path=\"%s\")\n", path);

  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    log_msg("swift_rmdir: Node not found: %s\n", path);
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  node->close(inodeNum);

  if(!FileSystem::getInstance().signalDeleteNode(node,true)){
    LOG(ERROR)<<"DELETE FAILED FOR:"<<path;
    return -EIO;
  }
  ZooHandler::getInstance().requestUpdateGlobalView();

  if(DEBUG_RMDIR)
    log_msg("Removed nodes from %s dir.\n", path);

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
  if(DEBUG_RENAME)
    log_msg("\nNOT IMPLEMENTED: bb_rename(fpath=\"%s\", newpath=\"%s\")\n", from, to);
  //Disabling rename
  return -ENOENT;
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
}

int swift_chmod(const char* path, mode_t mode) {
}

int swift_chown(const char* path, uid_t uid, gid_t gid) {
}*/

int swift_truncate(const char* path, off_t size) {
  log_msg("\nbb_truncate(path=\"%s\", size=%zu)\n", path, size);
  return swift_ftruncate(path,size,nullptr);
}

/*int swift_utime(const char* path, struct utimbuf* ubuf) {
}*/


int swift_open(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_OPEN)
    log_msg("\nbb_open(path=\"%s\", fi=0x%08x fh=0x%08x)\n", path, fi,fi->fh);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    if(node == nullptr)
      LOG(ERROR)<<"Failure, cannot Open Node not found: "<<path;
    else
      LOG(ERROR)<<"Failure, Node is delete."<<path;
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
    fprintf(stderr,"swift_open(): error in assigning inodeNum, or opening file.\n");
    fflush(stderr);
  	return -ENOENT;
  }
}

int swift_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
  /*errMutex.lock();
  static uint64_t counter = 0;
  fprintf(stderr,"FUSE READ, size:%zu\n",++counter);
  fflush(stderr);
  errMutex.unlock();*/
  if(DEBUG_READ)
    log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
        path, buf, size, offset, fi);
  //Handle path
  if(path == nullptr && fi->fh == 0)
  {
    log_msg("\nswift_read: fi->fh is null\n");
    return -ENOENT;
  }
  //Get associated FileNode*
  FileNode* node = (FileNode*) FileSystem::getInstance().getNodeByINodeNum(fi->fh);

  //Empty file
  if((!node->isRemote()&&node->getSize() == 0)||size == 0) {
    if(DEBUG_READ)
      log_msg("swift_read from:%s size=%d, offset=%lld EOF\n",node->getName().c_str(),0,offset);
    return 0;
  }
  long readBytes = 0;
  if(node->isRemote())
  	readBytes = node->readRemote(buf,offset,size);
  else
  	readBytes = node->read(buf,offset,size);

  if(readBytes >= 0) {
    if(DEBUG_READ)
      log_msg("swift_read successful from:%s size=%d, offset=%lld\n",node->getName().c_str(),readBytes,offset);
    return readBytes;
  }
  else {
    log_msg("\nswift_read: error in reading: size:%d offset:%lld readBytes:%d fileSize:%lld\n",size,offset,readBytes,node->getSize());
    return -EIO;
  }
}
int swift_write_error_tolerant(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
  int retry = 3;
  while(retry>0) {
    int res = swift_write(path, buf, size, offset,fi);
    if (res != -EIO)
	return res;
    LOG(ERROR)<<"write failed for:"<<path<<" retrying:"<<(3-retry+1)<<" Time.";
    retry--;
  }
  return -EIO;
}
int swift_write(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
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
    else if(written == -2) // No space
      return -ENOSPC;
    else //Internal IO Error
      return -EIO;
  }
  return written;
}

/*int swift_statfs(const char* path, struct statvfs* stbuf) {
}*/

int swift_flush(const char* path, struct fuse_file_info* fi) {
  int retstat = 0;
  //Get associated FileNode*
  FileNode* node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);
  if(DEBUG_FLUSH)
    log_msg("\nbb_flush(path=\"%s\", fi=0x%08x) name:%s\n", path, fi,node->getName().c_str());
  // no need to get fpath on this one, since I work from fi->fh not the path
  if(DEBUG_FLUSH)
    log_fi(fi);

  return retstat;
}

int swift_release(const char* path, struct fuse_file_info* fi) {
  int retstat = 0;

  //Handle path
  if(path == nullptr && fi->fh == 0)
  {
    log_msg("\nswift_release: fi->fh is null\n");
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
  if(DEBUG_RELEASE) {
    log_msg("\nbb_release(name=\"%s\", fi=0x%08x) \n", pathStr.c_str(), fi);
    log_fi(fi);
  }
  return retstat;
}

int swift_fsync(const char* path, int isdatasynch, struct fuse_file_info* fi) {
  //Get associated FileNode*
  //FileNode* node = (FileNode*)fi->fh;
  log_msg("swift_fsync: path:%s,fi->fh:0x%08x isdatasynch:%d\n",path,fi->fh,isdatasynch);
  return 0;
}

int swift_setxattr(const char* path, const char* name, const char* value,
    size_t size, int flags) {
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
    log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    log_msg("swift_opendir: swift_opendir: Node not found: %s\n", path);
    fi->fh = 0;
    return -ENOENT;
  }

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  if(inodeNum != 0) {
    fi->fh = inodeNum;
    return 0;
  } else {
    fi->fh = 0;
    fprintf(stderr,"swift_opendir(): error in assigning inodeNum or openning dir.\n");
    fflush(stderr);
    return -EIO;
  }
}

int swift_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi) {

  //Get associated FileNode*
  FileNode* node = nullptr;
  //Handle path
  if(path == nullptr && fi->fh == 0)
  {
    log_msg("\nswift_readdir: fi->fh is null\n");
    return ENOENT;
  }
  else if(path != nullptr) {
    //Get associated FileNode*
    string pathStr(path, strlen(path));
    node = FileSystem::getInstance().findAndOpenNode(pathStr);
  }
  else
    node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);


  if(node == nullptr)
    return -ENOENT;

  uint64_t inodeNumDIR = FileSystem::getInstance().assignINodeNum((intptr_t)node);

  int retstat = 0;

  if(DEBUG_READDIR)
    log_msg(
      "\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x, fh=0x%08x)\n",
      path, buf, filler, offset, fi, fi->fh);

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
      log_msg("swift_readdir filler error\n");
      entry->close(inodeNumDIR);//protect against delete
      node->close(inodeNumEntry);
      node->childrenUnlock();
      return -EIO;
    }
    entry->close(inodeNumEntry);//protect against delete
  }
  node->childrenUnlock();

  if(DEBUG_READDIR)
    log_msg("readdir successful: %d entry on Path:%s\n", node->childrenSize(),node->getName().c_str());

  node->close(inodeNumDIR);

  return retstat;
}

int swift_releasedir(const char* path, struct fuse_file_info* fi) {
  int retstat = 0;

  //Handle path
  if(path == nullptr && fi->fh == 0)
  {
    log_msg("\nswift_releasedir: fi->fh is null\n");
    return ENOENT;
  }
  //Get associated FileNode*
  FileNode* node = (FileNode*)FileSystem::getInstance().getNodeByINodeNum(fi->fh);
  //Update modification time
  node->setMTime(time(0));
  node->close(fi->fh);
  //we can earse ionode num from map as well
  //FileSystem::getInstance().removeINodeEntry(fi->fh);

  if(DEBUG_RELEASEDIR) {
    log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x) isStillOpen?%d \n", path, fi, node->isOpen());
    log_fi(fi);
  }
  return retstat;
}

int swift_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi) {
  log_msg("swift_fsyncdir: path:%s,fi->fh:0x%08x isdatasynch:%d\n",path,fi->fh,datasync);
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
  if(DEBUG_INIT) {
    log_msg("\nbb_init()\n");
    log_conn(conn);
    log_fuse_context(&fuseContext);
  }

  log_msg("\nStarting SyncThreads\n");
  //Start SyncQueue threads
  UploadQueue::getInstance().startSynchronization();
  DownloadQueue::getInstance().startSynchronization();
  log_msg("\nSyncThreads running...\n");
  //Start Zoo Election
  ZooHandler::getInstance().startElection();
	log_msg("\nZooHandler running...\n");

  return nullptr;
}

/**
 * we just give all the permissions
 * TODO: we should not just give all the permissions
 */
int swift_access(const char* path, int mask) {
  if(DEBUG_ACCESS)
    log_msg("\nbb_access(path=\"%s\", mask=0%o)\n", path, mask);
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
  log_msg("\nswift_ftruncate(path=\"%s\", fi:%p newsize:%zu )\n", path,fi,size);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance().findAndOpenNode(pathStr);
  if (node == nullptr) {
    log_msg("swift_ftruncate: error swift_ftruncate: Node not found: %s\n", path);
    return -ENOENT;
  }
  else
    log_msg("swift_ftruncate: Truncating:%s from:%zu to:%zu bytes\n", path,node->getSize(),size);

  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);

  //Checking Space availability
  size_t diff = size - node->getSize();
  if(diff > 0)
    if(!MemoryContorller::getInstance().checkPossibility(diff)) {
      log_msg("error swift_ftruncate: truncate failed(not enough space): %s newSize:%zu\n", path,node->getSize());
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
    log_msg("error swift_ftruncate: truncate failed: %s newSize:%zu\n", path,node->getSize());
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
