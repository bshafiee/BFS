//============================================================================
// Name        : BFS.cpp
// Author      : Behrooz
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

/*
 FUSE: Filesystem in Userspace
 Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

 This program can be distributed under the terms of the GNU GPL.
 See the file COPYING.

 gcc -Wall fusexmp.c `pkg-config fuse --cflags --libs` -o fusexmp
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

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

using namespace Swift;
using namespace std;
using namespace Poco;

static int xmp_getattr(const char *path, struct stat *stbuf) {
  log_msg("getattr\n");
  return 0;
  int res;

  res = lstat(path, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_access(const char *path, int mask) {
  log_msg("access\n");
    return 0;
  int res;

  res = access(path, mask);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
  log_msg("readlink\n");
    return 0;
  int res;

  res = readlink(path, buf, size - 1);
  if (res == -1)
    return -errno;

  buf[res] = '\0';
  return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) {
  log_msg("readdir\n");
    return 0;
  DIR *dp;
  struct dirent *de;

  (void) offset;
  (void) fi;

  dp = opendir(path);
  if (dp == NULL)
    return -errno;

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buf, de->d_name, &st, 0))
      break;
  }

  closedir(dp);
  return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
  log_msg("mknod\n");
    return 0;
  int res;

  /* On Linux this could just be 'mknod(path, mode, rdev)' but this
   is more portable */
  if (S_ISREG(mode)) {
    res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (res >= 0)
      res = close(res);
  } else if (S_ISFIFO(mode))
    res = mkfifo(path, mode);
  else
    res = mknod(path, mode, rdev);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
  log_msg("mkdir\n");
    return 0;
  int res;

  res = mkdir(path, mode);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_unlink(const char *path) {
  log_msg("unlink\n");
    return 0;
  int res;

  res = unlink(path);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_rmdir(const char *path) {
  log_msg("rmdir\n");
    return 0;
  int res;

  res = rmdir(path);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_symlink(const char *from, const char *to) {
  log_msg("symlink\n");
    return 0;
  int res;

  res = symlink(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_rename(const char *from, const char *to) {
  log_msg("rename\n");
    return 0;
  int res;

  res = rename(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_link(const char *from, const char *to) {
  log_msg("link\n");
    return 0;
  int res;

  res = link(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_chmod(const char *path, mode_t mode) {
  log_msg("chmod\n");
    return 0;
  int res;

  res = chmod(path, mode);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid) {
  log_msg("chown\n");
    return 0;
  int res;

  res = lchown(path, uid, gid);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_truncate(const char *path, off_t size) {
  log_msg("truncate\n");
    return 0;
  int res;

  res = truncate(path, size);
  if (res == -1)
    return -errno;

  return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2])
{
  int res;

  /* don't use utime/utimes since they follow symlinks */
  res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
  if (res == -1)
  return -errno;

  return 0;
}
#endif

static int xmp_open(const char *path, struct fuse_file_info *fi) {
  log_msg("open\n");
    return 0;
  int res;

  res = open(path, fi->flags);
  if (res == -1)
    return -errno;

  close(res);
  return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {
  log_msg("read\n");
    return 0;
  int fd;
  int res;

  (void) fi;
  fd = open(path, O_RDONLY);
  if (fd == -1)
    return -errno;

  res = pread(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
    off_t offset, struct fuse_file_info *fi) {
  log_msg("write\n");
    return 0;
  int fd;
  int res;

  (void) fi;
  fd = open(path, O_WRONLY);
  if (fd == -1)
    return -errno;

  res = pwrite(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf) {
  log_msg("statfs\n");
    return 0;
  int res;

  res = statvfs(path, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi) {
  log_msg("release\n");
    return 0;
  /* Just a stub.  This method is optional and can safely be left
   unimplemented */

  (void) path;
  (void) fi;
  return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
    struct fuse_file_info *fi) {
  log_msg("fsync\n");
    return 0;
  /* Just a stub.  This method is optional and can safely be left
   unimplemented */

  (void) path;
  (void) isdatasync;
  (void) fi;
  return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
    off_t offset, off_t length, struct fuse_file_info *fi)
{
  int fd;
  int res;

  (void) fi;

  if (mode)
  return -EOPNOTSUPP;

  fd = open(path, O_WRONLY);
  if (fd == -1)
  return -errno;

  res = -posix_fallocate(fd, offset, length);

  close(fd);
  return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
    size_t size, int flags)
{
  int res = lsetxattr(path, name, value, size, flags);
  if (res == -1)
  return -errno;
  return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
    size_t size)
{
  int res = lgetxattr(path, name, value, size);
  if (res == -1)
  return -errno;
  return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
  int res = llistxattr(path, list, size);
  if (res == -1)
  return -errno;
  return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
  int res = lremovexattr(path, name);
  if (res == -1)
  return -errno;
  return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
  .getattr = FUSESwift::swift_getattr ,
  .readlink = FUSESwift::swift_readlink ,
  .getdir = NULL ,
  .mknod = FUSESwift::swift_mknod ,
  .mkdir = FUSESwift::swift_mkdir ,
  .unlink = xmp_unlink ,
  .rmdir = xmp_rmdir ,
  .symlink = xmp_symlink ,
  .rename = xmp_rename ,
  .link = xmp_link ,
  .chmod = xmp_chmod ,
  .chown = xmp_chown ,
  .truncate = xmp_truncate ,
  .utime = NULL ,
  .open = xmp_open ,
  .read = xmp_read ,
  .write = xmp_write ,
  .statfs = xmp_statfs ,
  .flush = NULL ,
  .release = xmp_release ,
  .fsync = xmp_fsync ,
  .setxattr = NULL ,
  .getxattr = NULL ,
  .listxattr = NULL ,
  .removexattr = NULL ,
  .opendir = NULL ,
  .readdir = FUSESwift::swift_readdir ,
  .releasedir = FUSESwift::swift_releasedir ,
  .fsyncdir = NULL ,
  .init = FUSESwift::swift_init ,
  .destroy = NULL ,
  .access = xmp_access ,
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
  SwiftResult<Account*>* authenticateResult = Account::authenticate(info);

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

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  fuse_stat = fuse_main(argc, argv, &xmp_oper, bb_data);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

  return fuse_stat;
}
