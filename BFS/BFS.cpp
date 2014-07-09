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

using namespace Swift;
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
  .ftruncate = NULL ,
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
  .fallocate = NULL ,
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
  struct bb_state *bb_data;

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
    return 1;
  }

  // Perform some sanity checking on the command line:  make sure
  // there are enough arguments, and that neither of the last two
  // start with a hyphen (this will break if you actually have a
  // rootpoint or mountpoint whose name starts with a hyphen, but so
  // will a zillion other programs)
  if ((argc < 3) || (argv[argc - 2][0] == '-') || (argv[argc - 1][0] == '-'))
    bb_usage();

  bb_data = (bb_state*)malloc(sizeof(struct bb_state));
  if (bb_data == NULL) {
    perror("main calloc");
    abort();
  }

  // Pull the rootdir out of the argument list and save it in my
  // internal data
  bb_data->rootdir = realpath(argv[argc - 2], NULL);
  argv[argc - 2] = argv[argc - 1];
  argv[argc - 1] = NULL;
  argc--;

  bb_data->logfile = log_open();

/*
  FUSESwift::swift_init(nullptr);
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir2");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1/Dir3");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1/Dir3/Dir4");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1/Dir3/Dir4/Dir5");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1/Dir3/Dir4/Dir6");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1/Dir3/Dir4/Dir7");
  FUSESwift::FileSystem::getInstance()->mkDirectory("/Dir1/Dir3/Dir4/Dir5/Dir8");

  cout<<FUSESwift::FileSystem::getInstance()->printFileSystem()<<endl;
  cout<<"\nRename:\n";
  FUSESwift::FileSystem::getInstance()->tryRename("/Dir1/Dir3/Dir4","/Dir1/Dir3/Dir4(has3kids)");
  FUSESwift::FileSystem::getInstance()->tryRename("/Dir1/Dir3/Dir4(has3kids)/Dir5","/Dir1/Dir3/Dir4(has3kids)/DirNew5");
  FUSESwift::FileSystem::getInstance()->tryRename("/Dir1/Dir3/Dir4(has3kids)/DirNew5/Dir8","/Dir1/Dir3/Dir4(has3kids)/DirNew5/DirNew5/New8");
  cout<<FUSESwift::FileSystem::getInstance()->printFileSystem()<<endl;*/

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  fuse_stat = fuse_main(argc, argv, &xmp_oper, bb_data);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

  return fuse_stat;
}
