/**********************************************************************
Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
This program comes with ABSOLUTELY NO WARRANTY;

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#include "Global.h"

#define FUSE_USE_VERSION 29

#include <signal.h>
#include "Params.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include "BFSTcpServer.h"
#include "Backend.h"
#include "GlusterBackend.h"
#include <Swift/Account.h>
#include <Swift/Container.h>
#include <Swift/Object.h>
#include <iostream>
#include <istream>
#include "FuseBFS.h"
#include "Filesystem.h"
#include "Filenode.h"
#include "SyncQueue.h"
#include "SwiftBackend.h"
#include "BackendManager.h"
#include <string.h>
#include "MemoryController.h"
#include "SettingManager.h"
#include "BFSNetwork.h"
//#include "BFSTcpServer.h"
#include "MasterHandler.h"
#include "ZooHandler.h"
#include <thread>
#include "Statistics.h"
#include "Timer.h"
#include "StringView.h"

#if PROFILE
  #include <gperftools/profiler.h>
#endif

//Initialize logger
#include "LoggerInclude.h"
_INITIALIZE_EASYLOGGINGPP


using namespace Swift;
using namespace BFS;
using namespace std;
using namespace Poco;

void shutdown(void* userdata);

static struct fuse_operations fuse_oper = {
  .getattr = BFS::bfs_getattr ,
  .readlink = BFS::bfs_readlink ,
  .getdir = NULL ,
  .mknod = BFS::bfs_mknod ,
  .mkdir = BFS::bfs_mkdir ,
  .unlink = BFS::bfs_unlink ,
  .rmdir = BFS::bfs_rmdir ,
  .symlink = NULL ,
  .rename = NULL ,
  .link = NULL ,
  .chmod = BFS::bfs_chmod,
  .chown = BFS::bfs_chown,
  .truncate = BFS::bfs_truncate ,
  .utime = NULL ,
  .open = BFS::bfs_open ,
  .read = BFS::bfs_read_error_tolerant ,
  .write = BFS::bfs_write_error_tolerant ,
  .statfs = NULL ,
  .flush = BFS::bfs_flush ,
  .release = BFS::bfs_release ,
  .fsync = NULL ,
  .setxattr = NULL ,
  .getxattr = NULL ,
  .listxattr = NULL ,
  .removexattr = NULL ,
  .opendir = BFS::bfs_opendir ,
  .readdir = BFS::bfs_readdir ,
  .releasedir = BFS::bfs_releasedir ,
  .fsyncdir = NULL ,
  .init = BFS::bfs_init ,
  .destroy = shutdown ,
  .access = BFS::bfs_access ,
  //.create = BFS::bfs_create,
  .create = NULL,
  .ftruncate = BFS::bfs_ftruncate ,
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
  .fallocate = BFS::bfs_fallocate
};

void shutdown(void* context) {
  static bool hasBeenShutdown = false;
  if(hasBeenShutdown)
    return;
  hasBeenShutdown = true;
  LOG(INFO) <<"Leaving...";
#if PROFILE
  ProfilerFlush();
#endif
  Statistics::logStatInfo();
  if(SettingManager::runtimeMode() == RUNTIME_MODE::DISTRIBUTED) {
    ZooHandler::getInstance().stopZooHandler();
#ifdef BFS_ZERO
    BFSNetwork::stopNetwork();
#else
    //BFSTcpServer::stop();
#endif
    MasterHandler::stopLeadership();
  }
  FileSystem::getInstance().destroy();

#if PROFILE
  ProfilerStop();
#endif
  std::exit(0);
}

void bfs_usage(){
  fprintf(stderr, "usage:  BFS [FUSE and mount options] mountPoint\n");
  LOG(FATAL)<<"Invalid options to start BFS.";
  shutdown(nullptr);
}

void sigproc(int sig) {
  shutdown(nullptr);
}

// function to call if operator new can't allocate enough memory or error arises
void systemErrorHandler() {
  LOG(FATAL) <<"System Termination Occurred";
  shutdown(nullptr);
}

// function to call if operator new can't allocate enough memory or error arises
void outOfMemHandler() {
  LOG(FATAL) <<"Unable to satisfy request for memory";
  shutdown(nullptr);
}

void initLogger(int argc, char *argv[]) {
  _START_EASYLOGGINGPP(argc, argv);
  // Load configuration from file
  el::Configurations conf("log_config");
  // Reconfigure single logger
  el::Loggers::reconfigureLogger("default", conf);
  // Actually reconfigure all loggers instead
  el::Loggers::reconfigureAllLoggers(conf);

  el::Loggers::addFlag(el::LoggingFlag::LogDetailedCrashReason);
}

int main(int argc, char *argv[]) {

#if PROFILE
  ProfilerStart("/tmp/bfsprofile.cpu");
#endif
  initLogger(argc,argv);
  //Load configs first
  SettingManager::load("config");
  //Set signal handlers
  signal(SIGINT, sigproc);
  signal(SIGTERM, sigproc);
  //set the new_handler
  //std::set_new_handler(outOfMemHandler);
  //std::set_terminate(systemErrorHandler);

  //Run as root
  if ((getuid() == 0) || (geteuid() == 0)) {
    LOG(ERROR) << "Running BBFS as root opens unnacceptable security holes";
    //return 1;
  }

  // Sanity check
  if (argc < 1)
    bfs_usage();

  GlusterBackend glusterBackend;
  SwiftBackend swiftBackend;
  AuthenticationInfo info;
  switch(SettingManager::getBackendType()){
    case BackendType::GLUSTER:
      if(glusterBackend.initialize(
          SettingManager::get(CONFIG_KEY_GLUSTER_VOLUME),
          SettingManager::get(CONFIG_KEY_GLUSTER_SERVERS)))
        BackendManager::registerBackend(&glusterBackend);
      break;
    case BackendType::SWIFT:
      info.username = SettingManager::get(CONFIG_KEY_SWIFT_USERNAME);
      info.password = SettingManager::get(CONFIG_KEY_SWIFT_PASSWORD);
      info.authUrl = SettingManager::get(CONFIG_KEY_SWIFT_URL);
      info.tenantName = SettingManager::get(CONFIG_KEY_SWIFT_TENANT);
      info.method = AuthenticationMethod::KEYSTONE;
      if(swiftBackend.initialize(&info))
        BackendManager::registerBackend(&swiftBackend);
      break;
    default:
      break;
  }

  //Get Physical Memory amount
  LOG(INFO) <<"Total Physical Memory:" << MemoryContorller::getInstance().getTotalSystemMemory()/1024/1024 << " MB";
  LOG(INFO) <<"BFS Available Memory:" << MemoryContorller::getInstance().getAvailableMemory()/1024/1024 << " MB";
  LOG(INFO) <<"Memory Utilization:" << MemoryContorller::getInstance().getMemoryUtilization()*100<< "%";

  if(SettingManager::runtimeMode() == RUNTIME_MODE::DISTRIBUTED) {
#ifdef BFS_ZERO
  //Start BFS Network(before zoo, zoo uses mac info from this package)
	if(!BFSNetwork::startNetwork()) {
	  LOG(FATAL) <<"Cannot initialize ZeroNetworking!";
		shutdown(nullptr);
	}
#else
	if(!BFSTcpServer::start()) {
	  LOG(FATAL) <<"Cannot initialize TCPNetworking!";
    shutdown(nullptr);
	}
#endif
  }

  // turn over control to fuse
  LOG(INFO) <<"calling fuse_main";
  //Start fuse_main
  int fuse_stat = 0;
  fuse_stat = fuse_main(argc, argv, &fuse_oper, nullptr);
  LOG(INFO) <<"fuse returned: "<<fuse_stat;
  //while(1) {sleep(1);}
  shutdown(nullptr);
  return fuse_stat;
}
