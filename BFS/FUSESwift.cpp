/*
 * FUSESwift.cpp
 *
 *  Created on: 2014-06-25
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "FUSESwift.h"
#include "model/filenode.h"
#include "log.h"
#include "model/filesystem.h"
#include <cstring>
#include <ctime>
#include <unistd.h>
#include "model/UploadQueue.h"
#include "model/DownloadQueue.h"

using namespace std;

namespace FUSESwift {

FileNode* createRootNode() {
  FileNode *node = new FileNode(FileSystem::getInstance()->getDelimiter(), true, nullptr);
  unsigned long now = time(0);
  node->setCTime(now);
  node->setCTime(now);
  return node;
}

void fillStat(struct stat *stbuff, FileNode* node) {
  memset(stbuff, 0, sizeof(struct stat));
  //Fill Stat struct
  stbuff->st_dev = 0;
  stbuff->st_ino = 0;
  stbuff->st_mode = node->isDirectory() ? S_IFDIR : S_IFREG;
  stbuff->st_nlink = 1;
  stbuff->st_uid = node->getUID();
  stbuff->st_gid = node->getGID();
  stbuff->st_rdev = 0;
  stbuff->st_size = node->getSize();
  stbuff->st_blksize = FileSystem::blockSize;
  stbuff->st_blocks = node->getSize() / FileSystem::blockSize;
  stbuff->st_atime = 0x00000000;
  stbuff->st_mtime = node->getMTime();
  stbuff->st_ctime = node->getCTime();
}

int swift_getattr(const char *path, struct stat *stbuff) {
  if(DEBUG_GET_ATTRIB)
    log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuff);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if (node == nullptr) {
    log_msg("Node not found: %s\n", path);
    return -ENOENT;
  }
  //Fill Stat struct
  memset(stbuff, 0, sizeof(struct stat));
  fillStat(stbuff, node);
  if(DEBUG_GET_ATTRIB)
    log_stat(stbuff);
  return 0;
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
  if(DEBUG_MKNOD)
    log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, rdev);

  if (S_ISREG(mode)) {
    string pathStr(path, strlen(path));
    FileNode *newFile = FileSystem::getInstance()->mkFile(pathStr);
    if (newFile == nullptr) {
      retstat = ENOENT;
      log_msg("bb_mknod mkFile (newFile is nullptr)\n");
    }
    //File created successfully
    newFile->setMode(mode); //Mode
    unsigned long now = time(0);
    newFile->setMTime(now); //MTime
    newFile->setCTime(now); //CTime
    //Get Context
    struct fuse_context fuseContext = *fuse_get_context();
    newFile->setGID(fuseContext.gid);    //gid
    newFile->setUID(fuseContext.uid);    //uid
  } else {
    retstat = ENOENT;
    log_msg("bb_mknod expects regular file\n");
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
    FileNode *newDir = FileSystem::getInstance()->mkDirectory(pathStr);
    if (newDir == nullptr){
      retstat = ENOENT;
      log_msg("bb_mkdir mkFile (newFile is nullptr)\n");
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
    retstat = ENOENT;
    log_msg("bb_mkdir expects dir\n");
  }

  return retstat;
}

int swift_unlink(const char* path) {
  log_msg("bb_unlink(path=\"%s\")\n", path);

  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if (node == nullptr) {
    log_msg("Node not found: %s\n", path);
    return -ENOENT;
  }

  FileNode* parent = FileSystem::getInstance()->findParent(path);
  size_t remNodes = FileSystem::getInstance()->rmNode(parent, node);
  log_msg("Removed %d nodes from %s file.\n", remNodes, path);

  return 0;
}

int swift_rmdir(const char* path) {
  if(DEBUG_RMDIR)
    log_msg("bb_rmdir(path=\"%s\")\n", path);

  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if (node == nullptr) {
    log_msg("Node not found: %s\n", path);
    return -ENOENT;
  }

  FileNode* parent = FileSystem::getInstance()->findParent(path);
  size_t remNodes = FileSystem::getInstance()->rmNode(parent,node);
  if(DEBUG_RMDIR)
    log_msg("Removed %d nodes from %s dir.\n", remNodes, path);

  return 0;
}

int swift_symlink(const char* from, const char* to) {
}

int swift_rename(const char* from, const char* to) {
  if(DEBUG_RENAME)
    log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n", from, to);

  string oldPath(from, strlen(from));
  string newPath(to, strlen(to));
  if(!FileSystem::getInstance()->tryRename(oldPath,newPath)) {
    log_msg("\nbb_rename failed.\n");
    return -ENOENT;
  }
  else {
    if(DEBUG_RENAME)
      log_msg("bb_rename successful.\n");
    /*FileNode* node = FileSystem::getInstance()->getNode(newPath);
    log_msg("bb_rename MD5:%s\n",node->getMD5().c_str());*/
    return 0;
  }
}

int swift_link(const char* from, const char* to) {
}

int swift_chmod(const char* path, mode_t mode) {
}

int swift_chown(const char* path, uid_t uid, gid_t gid) {
}

int swift_truncate(const char* path, off_t size) {
}

int swift_utime(const char* path, struct utimbuf* ubuf) {
}

int swift_open(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_OPEN)
    log_msg("\nbb_open(path=\"%s\", fi=0x%08x fh=0x%08x)\n", path, fi,fi->fh);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if (node == nullptr) {
    log_msg("error swift_open: Node not found: %s\n", path);
    fi->fh = 0;
    return -ENOENT;
  }
  node->open();
  fi->fh = (intptr_t)node;
  return 0;
}

int swift_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
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
  FileNode* node = (FileNode*)fi->fh;
  //Empty file
  if(node->getSize() == 0)
  {
    if(DEBUG_READ)
      log_msg("bb_read successful from:%s size=%d, offset=%lld EOF\n",node->getName().c_str(),0,offset);
    return 0;
  }
  //Lock
  FileSystem::getInstance()->lock();
  long readBytes = node->read(buf,offset,size);
  //UnLock
	FileSystem::getInstance()->unlock();
  if(readBytes >= 0) {
    if(DEBUG_READ)
      log_msg("bb_read successful from:%s size=%d, offset=%lld\n",node->getName().c_str(),readBytes,offset);
    return readBytes;
  }
  else {
    log_msg("\nswift_read: error in reading: size:%d offset:%lld readBytes:%d fileSize:%lld\n",size,offset,readBytes,node->getSize());
    return -EIO;
  }
}

int swift_write(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
  if(DEBUG_WRITE)
    log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
        path, buf, size, offset, fi);
  // no need to get fpath on this one, since I work from fi->fh not the path
  if(DEBUG_WRITE)
    log_fi(fi);

  //Handle path
  if(path == nullptr && fi->fh == 0)
  {
    log_msg("\nswift_write: fi->fh is null\n");
    return -ENOENT;
  }
  //Get associated FileNode*
  FileNode* node = (FileNode*)fi->fh;
  //Lock
	FileSystem::getInstance()->lock();
  int written = node->write(buf,offset,size);
  //Update modification time
  node->setMTime(time(0));
  //Needs synchronization with the backend
  node->setNeedSync(true);
  //UnLock
	FileSystem::getInstance()->unlock();
  if(written == (int)size )
  {
    if(DEBUG_WRITE)
      log_msg("bb_write successful to:%s size=%d, offset=%lld\n",node->getName().c_str(),written,offset);
    return written;
  }
  else {
    log_msg("\nswift_write: error in writing to:%s\n",node->getName().c_str());
    return -EIO;
  }
}

int swift_statfs(const char* path, struct statvfs* stbuf) {
}

int swift_flush(const char* path, struct fuse_file_info* fi) {
  int retstat = 0;
  //Get associated FileNode*
  FileNode* node = (FileNode*)fi->fh;
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
  FileNode* node = (FileNode*)fi->fh;
  node->close();
  if(DEBUG_RELEASE) {
    log_msg("\nbb_release(name=\"%s\", fi=0x%08x) isStillOpen?%d \n", node->getFullPath().c_str(), fi, node->isOpen());
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
}

int swift_getxattr(const char* path, const char* name, char* value,
    size_t size) {
}

int swift_listxattr(const char* path, char* list, size_t size) {
}

int swift_removexattr(const char* path, const char* name) {
}

int swift_opendir(const char* path, struct fuse_file_info* fi) {
  if(DEBUG_OPENDIR)
    log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
  //Get associated FileNode*
  string pathStr(path, strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if (node == nullptr) {
    log_msg("swift_opendir: Node not found: %s\n", path);
    fi->fh = 0;
    return -ENOENT;
  }
  node->open();
  fi->fh = (intptr_t)node;
  return 0;
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
    node = FileSystem::getInstance()->getNode(pathStr);
  }
  else
    node = (FileNode*)fi->fh;


  int retstat = 0;

  if(DEBUG_READDIR)
    log_msg(
      "\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x, fh=0x%08x)\n",
      path, buf, filler, offset, fi, fi->fh);

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  auto it = node->childrendBegin();
  for (; it != node->childrenEnd(); it++) {
    FileNode* entry = (FileNode*) it->second;
    struct stat st;
    /**
     * void *buf, const char *name,
     const struct stat *stbuf, off_t off
     */
    fillStat(&st,entry);
    if (filler(buf, entry->getName().c_str(), &st, 0)) {
      log_msg("swift_readdir filler error\n");
      return EIO;
    }
  }
  if(DEBUG_READDIR)
    log_msg("readdir successful: %d entry on Path:%s\n", node->childrenSize(),node->getName().c_str());

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
  FileNode* node = (FileNode*)fi->fh;
  node->close();

  if(DEBUG_RELEASEDIR) {
    log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x) isStillOpen?%d \n", path, fi, node->isOpen());
    log_fi(fi);
  }
  return retstat;
}

int swift_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi) {
  log_msg("swift_fsync: path:%s,fi->fh:0x%08x isdatasynch:%d\n",path,fi->fh,datasync);
  return 0;
}

void* swift_init(struct fuse_conn_info* conn) {
  //Initialize file system
  FileNode* rootNode = createRootNode();
  FileSystem::getInstance()->initialize(rootNode);
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

  log_msg("\nStarting SyncThread\n");
  //Start SyncQueue threads
  UploadQueue::startSynchronization();
  DownloadQueue::startSynchronization();

  return nullptr;
}

void swift_destroy(void* userdata) {
  if(DEBUG_DESTROY)
    log_msg("\nbb_destroy(userdata=0x%08x)\n", userdata);
  FileSystem::getInstance()->destroy();
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

int swift_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
}

int swift_ftruncate(const char* path, off_t offset, struct fuse_file_info* fi) {
}

int swift_fgetattr(const char* path, struct stat* statbuf,
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
}

} //FUSESwift namespace
