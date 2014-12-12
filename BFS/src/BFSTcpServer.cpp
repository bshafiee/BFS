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
#include "ZooHandler.h"
#include <string.h> /* for strncpy */
#include <iostream>
#include <string.h>
#include "Timer.h"

using namespace Poco;
using namespace Poco::Net;
using namespace std;
using namespace BFSTCPNetworkTypes;


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
    cout<<"Error in createing socket to:"<<_ip<<":"<<_port<<" why?"<<e.message()<<endl<<flush;
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

      /*FUSESwift::Timer t;
      t.begin();*/

      do {
        count = socket.receiveBytes((void*)((char*)_dstBuffer+total),left);
        total += count;
        left -= count;
      } while(left > 0 && count);


      /*t.end();
      cout<<" Read:"<<total<<" bytes in:"<<t.elapsedMillis()<<" millis"<<endl<<flush;*/

      try {
        socket.close();
      } catch(...){}
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
  } else if(resPacket.statusCode == 200 && resPacket.readSize == 0){//EOF
    try {
      socket.close();
    } catch(...){}
    return 0;
  }
  //Unsuccessful
  try {
    socket.close();
  } catch(...){}
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
  try {
    uint64_t left = _size;
    do{
      int sent = socket.sendBytes((char*)_srcBuffer+(_size-left),left);
      left -= sent;
      if(sent <= 0){//Error
        cout<<"ERROR in sending write data, asked to send:"<<left<<" but sent:"<<sent<<endl<<std::flush;
        break;
      }
    }while(left);
  }catch(Exception &e){
    try {
      socket.close();
    } catch(...){}
    cout<<"Error in sending write data:"<<_remoteFile<<":"<<e.message()<<endl<<std::flush;
    return -2;
  }

  //Now read writeStatus
  WriteResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(WriteResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving write status packet"<<_remoteFile<<": "<<e.message()<<endl<<std::flush;
    try {
      socket.close();
    } catch(...){}
    return -2;
  }

  try {
    socket.close();
  } catch(...){}

  resPacket.statusCode = be64toh(resPacket.statusCode);
  if(resPacket.statusCode == 200)
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

      try {
        socket.close();
      } catch(...){}
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
  try {
    socket.close();
  } catch(...){}
  //Unsuccessful
  return resPacket.statusCode;
}

int64_t BFSTcpServer::deleteRemoteFile(const std::string& remoteFile,
    const std::string& _ip, uint _port) {
  if(remoteFile.length() > 1024){
      LOG(ERROR)<<"Filename too long!";
    return false;
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
  DeleteReqPacket deleteReqPacket;
  strncpy(deleteReqPacket.fileName,remoteFile.c_str(),remoteFile.length());
  deleteReqPacket.fileName[remoteFile.length()]= '\0';
  deleteReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::DELETE);

  //Send Packet
  try {
   socket.sendBytes((void*)&deleteReqPacket,sizeof(DeleteReqPacket));
  }catch(Exception &e){
   cout<<"Error in sending Delete request"<<remoteFile<<": "<<e.message()<<endl<<std::flush;
   try {
     socket.close();
   }catch(...){}
   return -2;
  }

  //Now Delete Status
  DeleteResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(DeleteResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving Delete status packet"<<remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }

  try {
    socket.close();
  } catch(...){}
  resPacket.statusCode = be64toh(resPacket.statusCode);
  return resPacket.statusCode;
}

int64_t BFSTcpServer::truncateRemoteFile(const std::string& remoteFile,
    size_t _newSize,const std::string& _ip, uint _port) {
  if(remoteFile.length() > 1024){
      LOG(ERROR)<<"Filename too long!";
    return false;
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
  TruncReqPacket truncReqPacket;
  strncpy(truncReqPacket.fileName,remoteFile.c_str(),remoteFile.length());
  truncReqPacket.fileName[remoteFile.length()]= '\0';
  truncReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::TRUNCATE);
  truncReqPacket.size = htobe64(_newSize);

  //Send Packet
  try {
   socket.sendBytes((void*)&truncReqPacket,sizeof(TruncReqPacket));
  }catch(Exception &e){
   cout<<"Error in sending Truncate request"<<remoteFile<<": "<<e.message()<<endl<<std::flush;
   try {
     socket.close();
   }catch(...){}
   return -2;
  }

  //Now Truncate Status
  TruncateResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(TruncateResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving Truncate status packet"<<remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }
  try {
    socket.close();
  } catch(...){}

  resPacket.statusCode = be64toh(resPacket.statusCode);
  return resPacket.statusCode;
}

int64_t BFSTcpServer::createRemoteFile(const std::string& remoteFile,
    const std::string& _ip, uint _port) {
  if(remoteFile.length() > 1024){
      LOG(ERROR)<<"Filename too long!";
    return false;
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
  CreateReqPacket createReqPacket;
  strncpy(createReqPacket.fileName,remoteFile.c_str(),remoteFile.length());
  createReqPacket.fileName[remoteFile.length()]= '\0';
  createReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::CREATE);

  //Send Packet
  try {
   socket.sendBytes((void*)&createReqPacket,sizeof(CreateReqPacket));
  }catch(Exception &e){
   cout<<"Error in sending Create request"<<remoteFile<<": "<<e.message()<<endl<<std::flush;
   try {
     socket.close();
   }catch(...){}
   return -2;
  }

  //Now Create Status
  CreateResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(CreateResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving Create status packet"<<remoteFile<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }

  try {
    socket.close();
  } catch(...){}

  resPacket.statusCode = be64toh(resPacket.statusCode);
  return resPacket.statusCode;
}

int64_t BFSTcpServer::moveFileToRemoteNode(const std::string& file,
    const std::string& _ip, uint _port) {
  if(file.length() > 1024){
      LOG(ERROR)<<"Filename too long!";
    return false;
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
  MoveReqPacket moveReqPacket;
  strncpy(moveReqPacket.fileName,file.c_str(),file.length());
  moveReqPacket.fileName[file.length()]= '\0';
  moveReqPacket.opCode = htonl((uint32_t)BFS_REMOTE_OPERATION::MOVE);

  //Send Packet
  try {
   socket.sendBytes((void*)&moveReqPacket,sizeof(MoveReqPacket));
  }catch(Exception &e){
   cout<<"Error in sending Move request"<<file<<": "<<e.message()<<endl<<std::flush;
   try {
     socket.close();
   }catch(...){}
   return -2;
  }

  //Now Move Status
  MoveResPacket resPacket;
  try {
    socket.receiveBytes((void*)&resPacket,sizeof(MoveResPacket));
  }catch(Exception &e){
    cout<<"Error in receiving Move status packet"<<file<<": "<<e.message()<<endl<<std::flush;
    try{
      socket.close();
    }catch(...){}
    return -2;
  }
  resPacket.statusCode = be64toh(resPacket.statusCode);
  if(resPacket.statusCode == 200) {//Success
    cout<<"Move to remote node successful: "<<file<< " Updating global view"<<endl<<flush;
    FUSESwift::ZooHandler::getInstance().requestUpdateGlobalView();
  }
  else
    cout<<"Move to remote node failed: "<<file<<endl<<flush;

  try {
    socket.close();
  } catch(...){}

  return resPacket.statusCode;
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
  string devName = FUSESwift::SettingManager::get(FUSESwift::CONFIG_KEY_ZERO_NETWORK_DEV);
  if(devName.length() > 0){
    iface = devName;
    findIP();
    string portStr = FUSESwift::SettingManager::get(FUSESwift::CONFIG_KEY_TCP_PORT);
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
