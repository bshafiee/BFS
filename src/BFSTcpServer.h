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

#ifndef BFSTCPSERVER_H_
#define BFSTCPSERVER_H_
#include "Global.h"
#include "Queue.h"
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/ParallelSocketReactor.h>
#include <Poco/Net/StreamSocket.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "BFSTcpServiceHandler.h"

enum class CONNECTION_STATUS {READY,BUSY};

struct TransferTask {
  TransferTask (WriteReqPacket _packet):data(nullptr),
      writeReq(_packet) {
    data = new u_char[writeReq.size];
  }
  ~TransferTask () {
    delete []data;
  }
  u_char* data;
  WriteReqPacket writeReq;
};

//Forward delcare
class BFSTcpServiceHandler;

class BFSTcpServer {
  //Make friend with BFSTcpServiceHandler
  friend BFSTcpServiceHandler;
  static Poco::Net::SocketReactor *reactor;
  static std::thread *thread;
  static std::uint32_t port;
  static std::string ip;
  static std::string iface;
  static std::atomic<bool> initialized;
  static std::atomic<bool> initSuccess;
  //Transfer Stuff
  static BFS::Queue<TransferTask*> transferQueue;
  static std::thread *transferThread;
  static std::atomic<bool> isRunning;
  //Utility
  static void findIP();
  static bool initialize();
  static void run();
  static void transferLoop();
  static void processTransfer(TransferTask*);
  BFSTcpServer();
public:
  virtual ~BFSTcpServer();
  static bool start();
  static void stop();
  static std::string getIP();
  static std::uint32_t getPort();
  /** IO OPERATION **/
  static int64_t readRemoteFile(void* _dstBuffer,uint64_t _size,size_t _offset,
      const std::string &_remoteFile, const std::string &_ip,uint _port);
  static int64_t writeRemoteFile(const void* _srcBuffer,uint64_t _size,size_t _offset,
      const std::string &_remoteFile,  const std::string &_ip,uint _port);
  static int64_t attribRemoteFile(struct packed_stat_info *attBuff,
      const std::string &remoteFile,  const std::string &_ip,uint _port);
  static int64_t deleteRemoteFile(const std::string &remoteFile,
      const std::string &_ip,uint _port);
  static int64_t flushRemoteFile(const std::string &remoteFile,
      const std::string &_ip,uint _port);
  static int64_t truncateRemoteFile(const std::string& _remoteFile,
      size_t _newSize,const std::string& _ip, uint _port);
  static int64_t createRemoteFile(const std::string &remoteFile,
      const std::string &_ip,uint _port);
  static int64_t moveFileToRemoteNode(const std::string &file,
      const std::string &_ip,uint _port);
  static void addTransferTask(TransferTask*);
};


#endif /* BFSTCPSERVER_H_ */
