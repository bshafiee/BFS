/*
 * BFSTcpServer.h
 *
 *  Created on: Dec 3, 2014
 *      Author: root
 */

#ifndef BFSTCPSERVER_H_
#define BFSTCPSERVER_H_
#include "Global.h"
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Thread.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "BFSTcpServiceHandler.h"

enum class CONNECTION_STATUS {READY,BUSY};

struct ConnectionEntry {
  ConnectionEntry(Poco::Net::StreamSocket* _socket,CONNECTION_STATUS _status):
    socket(_socket),status(_status),mustBeDelete(false) {}
  Poco::Net::StreamSocket* socket;
  CONNECTION_STATUS status;
  bool mustBeDelete;
};

//Forward delcare
class BFSTcpServiceHandler;

class BFSTcpServer {
  //Make friend with BFSTcpServiceHandler
  friend BFSTcpServiceHandler;
  static Poco::Net::SocketReactor *reactor;
  static Poco::Thread *thread;
  static std::unordered_map<std::string,ConnectionEntry> socketMap;
  static std::mutex mapMutex;
  static std::uint32_t port;
  static std::string ip;
  static std::string iface;
  //Socket Map Functions
  static bool addConnection(std::string _ip,ConnectionEntry _connectionEntry);
  static void delConnection(std::string _ip);
  static ConnectionEntry* findAndBusyConnection(std::string _ip);
  static bool makeConnectionReady(std::string _ip);
  //Utility
  static void findIP();
  static bool initialize();
  static void run();
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
  static int64_t truncateRemoteFile(const std::string& _remoteFile,
      size_t _newSize,const std::string& _ip, uint _port);
  static int64_t createRemoteFile(const std::string &remoteFile,
      const std::string &_ip,uint _port);
  static int64_t moveFileToRemoteNode(const std::string &file,
      const std::string &_ip,uint _port);
};


#endif /* BFSTCPSERVER_H_ */
