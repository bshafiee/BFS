/*
 * BFSTcpServer.cpp
 *
 *  Created on: Dec 3, 2014
 *      Author: root
 */

#include "BFSTcpServer.h"
#include "BFSTcpServiceHandler.h"
#include "SettingManager.h"
#include <Poco/Net/SocketAcceptor.h>
#include "LoggerInclude.h"
#include <string.h> /* for strncpy */
#include <iostream>
#include <string.h>
#include "Timer.h"

using namespace Poco;
using namespace Poco::Net;
using namespace std;
using namespace FUSESwift;


SocketReactor *BFSTcpServer::reactor = nullptr;
thread *BFSTcpServer::thread = nullptr;
unordered_map<std::string,ConnectionEntry> BFSTcpServer::socketMap;
mutex BFSTcpServer::mapMutex;
uint32_t BFSTcpServer::port = 0;
string BFSTcpServer::ip = "";
string BFSTcpServer::iface;
atomic<bool> BFSTcpServer::initialized(false);
atomic<bool> BFSTcpServer::initSuccess(false);

BFSTcpServer::BFSTcpServer() {}

BFSTcpServer::~BFSTcpServer() {}

void BFSTcpServer::run() {
  try{
    initialize();
    ServerSocket serverSocket(port);
    reactor = new SocketReactor();
    SocketAcceptor<BFSTcpServiceHandler> acceptor(serverSocket, *reactor);
    initSuccess.store(true);
    initialized.store(true);
    //Start Reactor
    reactor->run();
  }catch(Exception&e){
    LOG(ERROR)<<"ERROR in initializing BFSTCPServer:"<<e.message();
    initialized.store(true);
    initSuccess.store(false);
    return;
  }
}

bool BFSTcpServer::start() {
  //Start Server Socket
  thread = new std::thread(run);

  //Wait for initialization
  while(!initialized)
    usleep(10);

  return initSuccess;
}

void BFSTcpServer::stop() {
  cout<<"STOPPING REACTOR!"<<endl<<std::flush;
  if(reactor)
    reactor->stop();
  delete reactor;
  reactor = nullptr;
}

bool BFSTcpServer::addConnection(std::string _ip,
    ConnectionEntry _connectionEntry) {
  lock_guard<mutex> lock(mapMutex);
  auto it = socketMap.find(_ip);
  if(it != socketMap.end())//Duplicate
    return false;
  return socketMap.insert(pair<string,ConnectionEntry>(_ip,_connectionEntry)).second;
}

void BFSTcpServer::delConnection(std::string _ip) {
  lock_guard<mutex> lock(mapMutex);
  auto it = socketMap.find(_ip);
  if(it == socketMap.end())
    return;
  if(it->second.status == CONNECTION_STATUS::BUSY) {
    it->second.mustBeDelete = true;
    return;
  }
  socketMap.erase(it);
}

ConnectionEntry* BFSTcpServer::findAndBusyConnection(std::string _ip) {
  lock_guard<mutex> lock(mapMutex);
  auto it = socketMap.find(_ip);
  if(it == socketMap.end())
    return nullptr;
  if(it->second.mustBeDelete)//Don't return deleted connections
    return nullptr;
  it->second.status = CONNECTION_STATUS::BUSY;
  return &(it->second);
}

bool BFSTcpServer::makeConnectionReady(std::string _ip) {
  lock_guard<mutex> lock(mapMutex);
  auto it = socketMap.find(_ip);
  if(it == socketMap.end())
    return false;
  it->second.status = CONNECTION_STATUS::READY;
  if(it->second.mustBeDelete)
    socketMap.erase(it);
  return true;
}

int64_t BFSTcpServer::readRemoteFile(void* _dstBuffer, uint64_t _size,
    size_t _offset, const std::string& _remoteFile,
    const std::string& _ip,uint _port) {

  if(_remoteFile.length() >= 1024){
    cout <<"Filename too long:"<<_remoteFile<<endl<<std::flush;
    return -1;
  }

  StreamSocket socket;
  try {
    SocketAddress addres(_ip,_port);
    socket.connect(addres);
    //socket.setKeepAlive(true);
  }catch(Exception &e){
    cout<<"Error in createing socket to:"<<_ip<<":"<<_port<<endl<<flush;
    return -2;
  }

  //Create a request
  ReadReqPacket readReqPacket;
  strncpy(readReqPacket.fileName,_remoteFile.c_str(),_remoteFile.length());
  readReqPacket.fileName[_remoteFile.length()]= '\0';
  readReqPacket.offset = htobe64(_offset);
  readReqPacket.size = htobe64(_size);
  readReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::READ);

  //Send Packet
  try {
    socket.sendBytes((void*)&readReqPacket,sizeof(ReadReqPacket));
  }catch(Exception &e){
    cout<<"Error in sending read request"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }
  //Now read Status
  ReadResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(ReadResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving read status packet"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }
  resPacket.statusCode = be64toh(resPacket.statusCode);
  resPacket.readSize = be64toh(resPacket.readSize);
  if(resPacket.statusCode == 200 && resPacket.readSize > 0){//Successful
    try {
      int count = 0;
      uint64_t total = 0;
      uint64_t left = resPacket.readSize;

      FUSESwift::Timer t;
      t.begin();

      do {
        count = socket.receiveBytes((void*)((char*)_dstBuffer+total),left);
        total += count;
        left -= count;
      } while(left > 0 && count);


      t.end();
      cout<<" Read:"<<total<<" bytes in:"<<t.elapsedMillis()<<" millis"<<endl<<flush;

      if(total != resPacket.readSize)
        return -4;
      else
        return total;
    }catch(Exception &e){
      cout<<"Error in receiving read status packet"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
      try{
        socket.close();
      }catch(...){}
      return -2;
    }
  } else if(resPacket.statusCode == 200 && resPacket.readSize == 0)//EOF
    return 0;
  //Unsuccessful
  return resPacket.statusCode;
}

int64_t BFSTcpServer::writeRemoteFile(const void* _srcBuffer, uint64_t _size,
    size_t _offset, const std::string& _remoteFile,
    const std::string &_ip,uint _port) {
  if(_remoteFile.length() >= 1024){
    cout <<"Filename too long:"<<_remoteFile<<endl<<std::flush;
    return -1;
  }

  StreamSocket socket;
  try {
    SocketAddress addres(_ip,_port);
    socket.connect(addres);
    socket.setKeepAlive(true);
  }catch(Exception &e){
    cout<<"Error in creating socket to:"<<_ip<<":"<<_port<<endl<<flush;
    return -2;
  }

  //Create a request
  WriteReqPacket writeReqPacket;
  strncpy(writeReqPacket.fileName,_remoteFile.c_str(),_remoteFile.length());
  writeReqPacket.fileName[_remoteFile.length()]= '\0';
  writeReqPacket.offset = htobe64(_offset);
  writeReqPacket.size = htobe64(_size);
  writeReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::WRITE);

  //Send Packet
  try {
    socket.sendBytes((void*)&writeReqPacket,sizeof(WriteReqPacket));
  }catch(Exception &e){
    cout<<"Error in sending write request"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }

  //Now send real write data
  char* buf = new char[_size];
  try {
    uint64_t left = _size;
    do{
      int sent = socket.sendBytes(buf+(_size-left),left);
      left -= sent;
      if(sent <= 0){//Error
        cout<<"ERROR in sending write data, asked to send:"<<left<<" but sent:"<<sent<<endl<<std::flush;
        break;
      }
    }while(left);
  }catch(Exception &e){
    cout<<"Error in sending write data:"<<_remoteFile<<":"<<e.message()<<endl<<std::flush;
    return -2;
  }
  //socket.sendUrgent(1);

  //Now read writeStatus
  WriteResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(WriteResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving write status packet"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    return -2;
  }

  resPacket.statusCode = be64toh(resPacket.statusCode);
  resPacket.writtenSize = be64toh(resPacket.writtenSize);
  if(resPacket.statusCode == 200 && resPacket.writtenSize == _size)
    return _size;
  else
    return resPacket.statusCode;//Unsuccessful
}

int64_t BFSTcpServer::attribRemoteFile(struct stat* attBuff,
    const std::string& _remoteFile, const std::string& _ip, uint _port) {
  if(_remoteFile.length() >= 1024){
    cout <<"Filename too long:"<<_remoteFile<<endl<<std::flush;
    return -1;
  }

  StreamSocket socket;
  try {
    SocketAddress addres(_ip,_port);
    socket.connect(addres);
    socket.setKeepAlive(true);
  }catch(Exception &e){
    cout<<"Error in creating socket to:"<<_ip<<":"<<_port<<endl<<flush;
    return -2;
  }

  //Create a request
  AttribReqPacket attribReqPacket;
  strncpy(attribReqPacket.fileName,_remoteFile.c_str(),_remoteFile.length());
  attribReqPacket.fileName[_remoteFile.length()]= '\0';
  attribReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::ATTRIB);

  //Send Packet
  try {
    socket.sendBytes((void*)&attribReqPacket,sizeof(AttribReqPacket));
  }catch(Exception &e){
    cout<<"Error in sending Attrib request"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }
  //Now Attrib Status
  AttribResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(AttribResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving Attrib status packet"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }
  resPacket.statusCode = be64toh(resPacket.statusCode);
  resPacket.attribSize = be64toh(resPacket.attribSize);
  if(resPacket.statusCode == 200 && resPacket.attribSize > 0){//Successful
    try {
      int count = 0;
      uint64_t total = 0;
      uint64_t left = resPacket.attribSize;
      do {
        count = socket.receiveBytes((void*)((char*)attBuff+total),left);
        total += count;
        left -= count;
      } while(left > 0 && count);

      if(total != resPacket.attribSize)
        return -4;
      else
        return total;
    }catch(Exception &e){
      cout<<"Error in receiving attrib status packet"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
      try{
        socket.close();
      }catch(...){}
      return -2;
    }
  }
  //Unsuccessful
  return resPacket.statusCode;
}

int64_t BFSTcpServer::deleteRemoteFile(const std::string& remoteFile,
    const std::string& _ip, uint _port) {
}

int64_t BFSTcpServer::truncateRemoteFile(const std::string& _remoteFile,
    size_t _newSize,const std::string& _ip, uint _port) {
}

int64_t BFSTcpServer::createRemoteFile(const std::string& remoteFile,
    const std::string& _ip, uint _port) {
}

int64_t BFSTcpServer::moveFileToRemoteNode(const std::string& file,
    const std::string& _ip, uint _port) {
}

std::string BFSTcpServer::getIP() {
  if(ip=="")
    initialize();
  return ip;
}
void BFSTcpServer::findIP(){
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name , iface.c_str() , IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);
  ip = string(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  printf("IP: %s\n",ip.c_str());
}

std::uint32_t BFSTcpServer::getPort() {
  if(port == 0)
    initialize();
  return port;
}

bool BFSTcpServer::initialize() {
  string devName = SettingManager::get(CONFIG_KEY_ZERO_NETWORK_DEV);
  if(devName.length() > 0){
    iface = devName;
    findIP();
    string portStr = SettingManager::get(CONFIG_KEY_TCP_PORT);
    if(!portStr.length()){
      LOG(DEBUG) <<"No tcp_port in the config file! \n"
          "Initializing BFSTCPServer failed.\n";
      return false;
    }
    port = stoul(portStr);
  } else{
    LOG(DEBUG) <<"No device specified in the config file! \n"
        "Initializing BFSTCPServer failed.\n";
    return false;
  }

  return true;
}
