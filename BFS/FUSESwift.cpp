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

using namespace std;

namespace FUSESwift {

int swift_getattr (const char *path, struct stat *stbuff) {
  int res = 0;
  memset(stbuff, 0, sizeof(struct stat));
  //Get associated FileNode*
  string pathStr(path,strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  //Fill Stat struct
  stbuff->st_dev = 0;
  stbuff->st_ino = 0;
  stbuff->st_mode = node->isDirectory()? S_IFDIR : S_IFREG;
  stbuff->st_nlink = 1;
  stbuff->st_uid = 0;
  stbuff->st_gid = 0;
  stbuff->st_rdev = 0;
  stbuff->st_size = 0;
  stbuff->st_blksize = FileSystem::getInstance()->getBlockSize();
  stbuff->st_blocks = node->getSize() / FileSystem::getInstance()->getBlockSize();
  stbuff->st_atime = 0x00000000;
  stbuff->st_mtime = 0x00000000;
  stbuff->st_ctime = 0x00000000;

  log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuff);
  log_stat (stbuff);

  //res = -ENOENT;
  return res;
}

void* swift_init(struct fuse_conn_info* conn) {
  log_msg("\nbb_init()\n");

  log_conn(conn);
  log_fuse_context(fuse_get_context());

  return BB_DATA;
}
}
