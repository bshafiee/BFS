//============================================================================
// Name        : BFS.cpp
// Author      : Behrooz
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 29

#include "params.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <Account.h>
#include <Container.h>
#include <Object.h>
#include <iostream>
#include <istream>
#include <Poco/StreamCopier.h>
#include "FUSESwift.h"
#include "log.h"
#include "model/filesystem.h"
#include "model/filenode.h"
#include "model/SyncQueue.h"
#include "model/SwiftBackend.h"
#include "model/BackendManager.h"
#include "string.h"
#include "model/DownloadQueue.h"


using namespace Swift;
using namespace FUSESwift;
using namespace std;
using namespace Poco;

static struct fuse_operations xmp_oper = {
  .getattr = FUSESwift::swift_getattr ,
  .readlink = FUSESwift::swift_readlink ,
  .getdir = NULL ,
  .mknod = FUSESwift::swift_mknod ,
  .mkdir = FUSESwift::swift_mkdir ,
  .unlink = FUSESwift::swift_unlink ,
  .rmdir = FUSESwift::swift_rmdir ,
  .symlink = NULL ,
  .rename = FUSESwift::swift_rename ,
  .link = NULL ,
  .chmod = NULL ,
  .chown = NULL ,
  .truncate = NULL ,
  .utime = NULL ,
  .open = FUSESwift::swift_open ,
  .read = FUSESwift::swift_read ,
  .write = FUSESwift::swift_write ,
  .statfs = NULL ,
  .flush = FUSESwift::swift_flush ,
  .release = FUSESwift::swift_release ,
  .fsync = NULL ,
  .setxattr = NULL ,
  .getxattr = NULL ,
  .listxattr = NULL ,
  .removexattr = NULL ,
  .opendir = FUSESwift::swift_opendir ,
  .readdir = FUSESwift::swift_readdir ,
  .releasedir = FUSESwift::swift_releasedir ,
  .fsyncdir = NULL ,
  .init = FUSESwift::swift_init ,
  .destroy = FUSESwift::swift_destroy ,
  .access = FUSESwift::swift_access ,
  .create = NULL ,
  .ftruncate = FUSESwift::swift_ftruncate ,
  .fgetattr = NULL ,
  .lock = NULL ,
  .utimens = NULL ,
  .bmap = NULL ,
  .flag_nullpath_ok = 1,
  .flag_nopath = 1,
  .flag_utime_omit_ok = 1,
  .flag_reserved = 29,
  .ioctl = NULL ,
  .poll = NULL ,
  .write_buf = NULL ,
  .read_buf = NULL ,
  .flock = NULL ,
  .fallocate = NULL
};

void bb_usage()
{
    fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[]) {
  AuthenticationInfo info;
  info.username = "behrooz";
  info.password = "behrooz";
  info.authUrl = "http://10.42.0.83:5000/v2.0/tokens";
  info.tenantName = "kos";
  info.method = AuthenticationMethod::KEYSTONE;

  //Authenticate
  //SwiftResult<Account*>* authenticateResult = Account::authenticate(info);

  //Start fuse_main
  int fuse_stat;
  //make ready log file
  log_open();

  // bbfs doesn't do any access checking on its own (the comment
  // blocks in fuse.h mention some of the functions that need
  // accesses checked -- but note there are other functions, like
  // chown(), that also need checking!).  Since running bbfs as root
  // will therefore open Metrodome-sized holes in the system
  // security, we'll check if root is trying to mount the filesystem
  // and refuse if it is.  The somewhat smaller hole of an ordinary
  // user doing it with the allow_other flag is still there because
  // I don't want to parse the options string.
  if ((getuid() == 0) || (geteuid() == 0)) {
    fprintf(stderr,
        "Running BBFS as root opens unnacceptable security holes\n");
    //return 1;
  }

  // Perform some sanity checking on the command line:  make sure
  // there are enough arguments, and that neither of the last two
  // start with a hyphen (this will break if you actually have a
  // mountpoint whose name starts with a hyphen, but so
  // will a zillion other programs)
  if (argc < 1)
    bb_usage();

  /*
  long len = 1000;
  char buff[len];
  memset(buff,'*',len);

  FUSESwift::FileNode* myFile = new FileNode("F1",false);
  long offset = 0;
  for(int i=0;i<100000;i++) {
    myFile->write(buff,offset,len);
    offset += len;
  }

  FUSESwift::FileNode* f1 = new FileNode("F1",false);
  FUSESwift::FileNode* f2 = new FileNode("F2",false);
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::RENAME,f1));
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::RENAME,f1));
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::DELETE,f1));
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::DELETE,f2));
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::UPDATE_METADATA,f1));
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::DELETE,f2));
  SyncQueue::getInstance()->push(new SyncEvent(SyncEventType::UPDATE_CONTENT,f2));

  while(SyncQueue::getInstance()->size())
    cout<<SyncQueue::getInstance()->pop()->print()<<endl;
  */


  SwiftBackend swiftBackend;
  swiftBackend.initialize(&info);
  BackendManager::registerBackend(&swiftBackend);


  /*FUSESwift::FileNode* f1 = new FileNode("F1",false,nullptr);
  long len = 5000;
  char buff[len];
  memset(buff,'*',len);
  //for(int i=0;i<5000;i++)
  f1->write(buff,f1->getSize(),len);
  cout<<"MD5:"<<f1->getMD5()<<"\tSize:"<<f1->getSize()<<endl;
  char buff2[4000];
  memset(buff2,'*',4000);
  f1->write(buff2,4500,4000);
  cout<<"MD5:"<<f1->getMD5()<<"\tSize:"<<f1->getSize()<<endl;

  f1->truncate(5000);
  cout<<"MD5:"<<f1->getMD5()<<"\tSize:"<<f1->getSize()<<endl;*/
/*
  SyncQueue::push(new SyncEvent(SyncEventType::UPDATE_CONTENT,f1,f1->getFullPath()));
  SyncQueue::startSyncThread();

  FileNode* root = nullptr;
  this_thread::sleep_for(chrono::milliseconds(5000));
  SyncQueue::push(new SyncEvent(SyncEventType::DELETE,f1,f1->getFullPath()));*/

  //swift_init(nullptr);
  //DownloadQueue::getInstance()->startSynchronization();

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  fuse_stat = fuse_main(argc, argv, &xmp_oper, nullptr);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
  //while(1) {}

  return fuse_stat;
}
