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

using namespace std;

namespace FUSESwift {

FileNode* createRootNode() {
  FileNode *node = new FileNode(FileSystem::getInstance()->getDelimiter(),true);
  unsigned long = time(0);
  node->setCTime(0);
  node->setCTime(0);
}

int swift_getattr (const char *path, struct stat *stbuff) {
  memset(stbuff, 0, sizeof(struct stat));
  //Get associated FileNode*
  string pathStr(path,strlen(path));
  FileNode* node = FileSystem::getInstance()->getNode(pathStr);
  if(node == nullptr)
    return ENOENT;
  //Fill Stat struct
  stbuff->st_dev = 0;
  stbuff->st_ino = 0;
  stbuff->st_mode = node->isDirectory()? S_IFDIR : S_IFREG;
  stbuff->st_nlink = 1;
  stbuff->st_uid = node->getUID();
  stbuff->st_gid = node->getGID();
  stbuff->st_rdev = 0;
  stbuff->st_size = node->getSize();
  stbuff->st_blksize = FileSystem::getInstance()->getBlockSize();
  stbuff->st_blocks = node->getSize() / FileSystem::getInstance()->getBlockSize();
  stbuff->st_atime = 0x00000000;
  stbuff->st_mtime = node->getMTime();
  stbuff->st_ctime = node->getCTime();

  log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuff);
  log_stat (stbuff);

  return 0;
}

void* swift_init(struct fuse_conn_info* conn) {
  //Initialize file system
  FileNode* rootNode = createRootNode();
  FileSystem::getInstance()->initialize(rootNode);
  //Get Context
  struct fuse_context *fuseContext = fuse_get_context();
  rootNode->setGID(fuseContext->gid);
  rootNode->setGID(fuseContext->uid);
  //Log
  log_msg("\nbb_init()\n");
  log_conn(conn);
  log_fuse_context(fuseContext);

  return BB_DATA;
}
}
