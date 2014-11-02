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

#include <signal.h>
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
#include "model/UploadQueue.h"
#include "model/MemoryController.h"
#include "model/SettingManager.h"
#include "model/znet/BFSNetwork.h"


using namespace Swift;
using namespace FUSESwift;
using namespace std;
using namespace Poco;

static struct fuse_operations fuse_oper = {
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

void shutdown() {
	fprintf(stderr, "Leaving...\n");
	//Log Close
	log_close();
	BFSNetwork::stopNetwork();
	FileSystem::getInstance().destroy();
	exit(0);
}

void bfs_usage(){
	fprintf(stderr, "usage:  BFS [FUSE and mount options] mountPoint\n");
  shutdown();
}

void sigproc(int sig) {
	shutdown();
}

// function to call if operator new can't allocate enough memory or error arises
void systemErrorHandler() {
	fprintf(stderr,"System Termination Occurred\n");
  shutdown();
}

// function to call if operator new can't allocate enough memory or error arises
void outOfMemHandler() {
	fprintf(stderr,"Unable to satisfy request for memory\n");
  shutdown();
}

int main(int argc, char *argv[]) {
	//Load configs first
	SettingManager::load("config");

	//Set signal handlers
	signal(SIGINT, sigproc);
	signal(SIGTERM, sigproc);
	signal(SIGINT, sigproc);
  //set the new_handler
  std::set_new_handler(outOfMemHandler);
  std::set_terminate(systemErrorHandler);

  AuthenticationInfo info;
  info.username = SettingManager::get(CONFIG_KEY_SWIFT_USERNAME);
  info.password = SettingManager::get(CONFIG_KEY_SWIFT_PASSWORD);
  info.authUrl = SettingManager::get(CONFIG_KEY_SWIFT_URL);
  info.tenantName = SettingManager::get(CONFIG_KEY_SWIFT_TENANT);
  info.method = AuthenticationMethod::KEYSTONE;

  //make ready log file
  log_open();

  // BFS doesn't do any access checking on its own (the comment
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

  // Sanity check
  if (argc < 1)
    bfs_usage();


  SwiftBackend swiftBackend;
  swiftBackend.initialize(&info);
  BackendManager::registerBackend(&swiftBackend);

  //Get Physical Memory amount
  cout <<"Total Physical Memory:"<<MemoryContorller::getInstance().getTotalSystemMemory()/1024/1024<<" MB"<<endl;

  //Start BFS Network(before zoo, zoo uses mac info from this package)
	if(!BFSNetwork::startNetwork()) {
		fprintf(stderr, "Cannot initialize ZeroNetworking!\n");
		shutdown();
	}

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  //Start fuse_main
  int fuse_stat = fuse_main(argc, argv, &fuse_oper, nullptr);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
  //while(1) {}

  shutdown();
  return fuse_stat;
}
