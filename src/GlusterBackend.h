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

#ifndef GLUSTERBACKEND_H_
#define GLUSTERBACKEND_H_
#include "Global.h"
#include "Backend.h"
#include <tuple>
#include <string>

namespace FUSESwift {

struct VolumeServer {
  VolumeServer(std::string _serverIP,int _port,std::string _transport):
    transport(_transport),serverIP(_serverIP),port(_port){}
  std::string transport;//tcp,rdma
  std::string serverIP;
  int port;
};

struct GlusterFSConnection {
  std::string volumeName;
  std::vector<VolumeServer> volumeServers;
};

class GlusterBackend: public Backend {
  GlusterFSConnection connectionInfo;
  void* fs = NULL;
  bool recursiveListDir(const char* path,std::vector<BackendItem>& _list);
  bool createDirectory(const char* path);
public:
  GlusterBackend();
  virtual ~GlusterBackend();

  bool initialize(std::string _volume,std::string _volumeServer);
  bool initialize(GlusterFSConnection _connectionInfo);
  //Implement backend interface
  bool list(std::vector<BackendItem>& _list);
  bool get(const SyncEvent *_getEvent);
  std::vector<std::pair<std::string,std::string> >* get_metadata(const SyncEvent *_getMetaEvent);
  bool put(const SyncEvent *_putEvent);
  bool put_metadata(const SyncEvent *_putMetaEvent);
  bool move(const SyncEvent *_moveEvent);
  bool remove(const SyncEvent *_removeEvent);
};

} /* namespace FUSESwift */
#endif /* SWIFTBACKEND_H_ */
