/*
 * FUSESwift.cpp
 *
 *  Created on: 2014-06-25
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "FUSESwift.h"
#include "log.h"
#include "model/filesystem.h"
#include <cstring>
#include <ctime>
#include <unistd.h>

using namespace std;

namespace FUSESwift {

FileNode* createRootNode() {
  FileNode *node = new FileNode(FileSystem::getInstance()->getDelimiter(),
      true);
  unsigned long now = time(0);
  node->setCTime(now);
  node->setCTime(now);
  return node;
}

// Report errors to logfile and give -errno to caller
static int bb_error(char *str) {
  int ret = -errno;

  log_msg("    ERROR %s: %s\n", str, strerror(errno));

  return ret;
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
  stbuff->st_blksize = FileSystem::getInstance()->getBlockSize();
  stbuff->st_blocks = node->getSize()
      / FileSystem::getInstance()->getBlockSize();
  stbuff->st_atime = 0x00000000;
  stbuff->st_mtime = node->getMTime();
  stbuff->st_ctime = node->getCTime();
}

int swift_getattr(const char *path, struct stat *stbuff) {
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

  log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, rdev);

  if (S_ISREG(mode)) {
    string pathStr(path, strlen(path));
    FileNode *newFile = FileSystem::getInstance()->mkFile(pathStr);
    if (newFile == nullptr)
      retstat = bb_error("bb_mknod mkFile (newFile is nullptr)");
    //File created successfully
    newFile->setMode(mode); //Mode
    unsigned long now = time(0);
    newFile->setMTime(now); //MTime
    newFile->setCTime(now); //CTime
    //Get Context
    struct fuse_context fuseContext = *fuse_get_context();
    newFile->setGID(fuseContext.gid);    //gid
    newFile->setUID(fuseContext.uid);    //uid
  } else
    retstat = bb_error("bb_mknod error");

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
  mode = mode|S_IFDIR;
  log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

  if (S_ISDIR(mode)) {
    string pathStr(path, strlen(path));
    FileNode *newDir = FileSystem::getInstance()->mkDirectory(pathStr);
    if (newDir == nullptr)
      retstat = bb_error("bb_mkdir mkDir (newDir is nullptr)");
    //Directory created successfully
    newDir->setMode(mode); //Mode
    unsigned long now = time(0);
    newDir->setMTime(now); //MTime
    newDir->setCTime(now); //CTime
    //Get Context
    struct fuse_context fuseContext = *fuse_get_context();
    newDir->setGID(fuseContext.gid);    //gid
    newDir->setUID(fuseContext.uid);    //uid
  } else
    retstat = bb_error("bb_mknod error");

  return retstat;
}

int swift_unlink(const char* path) {
}

int swift_rmdir(const char* path) {
}

int swift_symlink(const char* from, const char* to) {
}

int swift_rename(const char* from, const char* to) {
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
}

int swift_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
}

int swift_write(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
}

int swift_statfs(const char* path, struct statvfs* stbuf) {
}

int swift_flush(const char* path, struct fuse_file_info* fi) {
}

int swift_release(const char* path, struct fuse_file_info* fi) {
}

int swift_fsync(const char* path, int isdatasynch, struct fuse_file_info* fi) {
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
}

int swift_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi) {

  int retstat = 0;
  log_msg(
      "\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
      path, buf, filler, offset, fi);
  /**
   * -- malformed -- path but we assume (null) path is root folder
   */
  /*
   if (path == nullptr) {
   log_msg("No such directory: %s\n",path);
   return ENOENT;
   }*/

  //Get associated FileNode*
  const char* modifiedPath = (path == nullptr) ? "/" : path;
  string pathStr(modifiedPath, strlen(modifiedPath));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if (node == nullptr) {
    log_msg("Node not found: %s\n", path);
    return ENOENT;
  }

  childDictionary::iterator it = node->childrendBegin();
  for (; it != node->childrenEnd(); it++) {
    FileNode* entry = (FileNode*) it->second;
    struct stat st;
    /**
     * void *buf, const char *name,
     const struct stat *stbuf, off_t off
     */
    if (filler(buf, entry->getName().c_str(), &st, 0)) {
      retstat = bb_error("swift_readdir filler error");
      break;
    }
  }
  log_msg("readdir successful: %d entry. Count:%d\n", it,node->childrenSize());

  return retstat;
}

int swift_releasedir(const char* path, struct fuse_file_info* fi) {
  int retstat = 0;

  log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
  log_fi(fi);

  return retstat;
}

int swift_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi) {
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
  log_msg("\nbb_init()\n");
  log_conn(conn);
  log_fuse_context(&fuseContext);

  return BB_DATA ;
}

void swift_destroy(void* userdata) {
}

int swift_access(const char* path, int mask) {
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
