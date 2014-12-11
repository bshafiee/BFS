/*
 * BFSTcpServiceHandler.cpp
 *
 *  Created on: Dec 3, 2014
 *      Author: root
 */

#include "BFSTcpServiceHandler.h"
#include "BFSTcpServer.h"
#include <Poco/NObserver.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Exception.h>
#include "filesystem.h"
#include "filenode.h"
#include <sys/stat.h>
#include <endian.h>
#include <string>
#include <iostream>
#include "Timer.h"

using namespace Poco;
using namespace Poco::Net;
using namespace std;
using namespace FUSESwift;



BFSTcpServiceHandler::BFSTcpServiceHandler(StreamSocket& _socket,
    SocketReactor& _reactor): socket(_socket),reactor(_reactor) {
  //Set Keeep Alive for socket
  socket.setKeepAlive(true);

  //Register Callbacks
  reactor.addEventHandler(socket, NObserver<BFSTcpServiceHandler,
    ReadableNotification>(*this, &BFSTcpServiceHandler::onReadable));
  /*reactor.addEventHandler(socket, NObserver<BFSTcpServiceHandler,
    WritableNotification>(*this, &BFSTcpServiceHandler::onWriteable));*/
  reactor.addEventHandler(socket, NObserver<BFSTcpServiceHandler,
    ShutdownNotification>(*this, &BFSTcpServiceHandler::onShutdown));
  reactor.addEventHandler(socket, NObserver<BFSTcpServiceHandler,
    ErrorNotification>(*this, &BFSTcpServiceHandler::onError));
  /*reactor.addEventHandler(socket, NObserver<BFSTcpServiceHandler,
    TimeoutNotification>(*this, &BFSTcpServiceHandler::onTimeout));*/
  /*reactor.addEventHandler(socket, NObserver<BFSTcpServiceHandler,
    IdleNotification>(*this, &BFSTcpServiceHandler::onIdle));*/


}

BFSTcpServiceHandler::~BFSTcpServiceHandler() {
  //Unregister Callbacks
  reactor.removeEventHandler(socket, NObserver<BFSTcpServiceHandler,
    ReadableNotification>(*this, &BFSTcpServiceHandler::onReadable));
  /*reactor.removeEventHandler(socket, NObserver<BFSTcpServiceHandler,
    WritableNotification>(*this, &BFSTcpServiceHandler::onWriteable));*/
  reactor.removeEventHandler(socket, NObserver<BFSTcpServiceHandler,
    ShutdownNotification>(*this, &BFSTcpServiceHandler::onShutdown));
  reactor.removeEventHandler(socket, NObserver<BFSTcpServiceHandler,
    ErrorNotification>(*this, &BFSTcpServiceHandler::onError));
  /*reactor.removeEventHandler(socket, NObserver<BFSTcpServiceHandler,
    TimeoutNotification>(*this, &BFSTcpServiceHandler::onTimeout));*/
  /*reactor.removeEventHandler(socket, NObserver<BFSTcpServiceHandler,
    IdleNotification>(*this, &BFSTcpServiceHandler::onIdle));*/
  //Close socket
  try {
    socket.close();
  }catch(...){}
}

BFS_REMOTE_OPERATION BFSTcpServiceHandler::toBFSRemoteOperation(
    uint32_t _opCode) {
  /**
    READ = 1, WRITE = 2, ATTRIB = 3,
    DELETE = 4, TRUNCATE = 5, CREATE = 6,
    MOVE = 7, UNKNOWN = 0
   */
  switch (_opCode) {
  case 1: return BFS_REMOTE_OPERATION::READ;
  case 2: return BFS_REMOTE_OPERATION::WRITE;
  case 3: return BFS_REMOTE_OPERATION::ATTRIB;
  case 4: return BFS_REMOTE_OPERATION::DELETE;
  case 5: return BFS_REMOTE_OPERATION::TRUNCATE;
  case 6: return BFS_REMOTE_OPERATION::CREATE;
  case 7: return BFS_REMOTE_OPERATION::MOVE;
  //Invalid
  default: return BFS_REMOTE_OPERATION::UNKNOWN;
  }
}

void BFSTcpServiceHandler::onReadable(
    const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf) {
  //cout<<"onReadable:"<<socket.peerAddress().toString()<<endl;

  //Read First Packet to Figure out
  int len = 2000;
  u_char _packet[len];
  ReqPacket reqPacket;
  try {
    //IMPORTANT ONLY READ AS BIG AS OPCODE
    int read = socket.receiveBytes(_packet,sizeof(reqPacket.opCode));
    if(read == 0){
      //
      //cout<<"onReadable size of 0, Means the other peer of this connection is dead!"<<endl<<std::flush;
      delete this;
      return;
    }

    //Check opcode
    uint32_t opCode = ntohl(((ReqPacket*)_packet)->opCode);


    FUSESwift::Timer t;
    double t1;
    double t2;
    switch(toBFSRemoteOperation(opCode)) {
      case BFS_REMOTE_OPERATION::READ:
        //Read the rest of request packet
        t.begin();
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(ReadReqPacket)-sizeof(reqPacket.opCode));
        t.end();
        t1 = t.elapsedMicro();
        t.begin();
        onReadRequest(_packet);
        t.end();
        t2 = t.elapsedMicro();
        //cout<<"READ SERVERD IN :"<<t.elapsedMicro()<<" Micro T1:"<<t1<<" Total:"<<t1+t2<<endl<<flush;
        break;
      case BFS_REMOTE_OPERATION::WRITE:
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(WriteReqPacket)-sizeof(reqPacket.opCode));
        onWriteRequest(_packet);
        break;
      case BFS_REMOTE_OPERATION::ATTRIB:
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(AttribReqPacket)-sizeof(reqPacket.opCode));
        onAttribRequest(_packet);
        break;
      case BFS_REMOTE_OPERATION::DELETE:
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(DeleteReqPacket)-sizeof(reqPacket.opCode));
        onDeleteRequest(_packet);
        break;
      case BFS_REMOTE_OPERATION::TRUNCATE:
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(TruncReqPacket)-sizeof(reqPacket.opCode));
        onTruncateRequest(_packet);
        break;
      case BFS_REMOTE_OPERATION::CREATE:
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(CreateReqPacket)-sizeof(reqPacket.opCode));
        onCreateRequest(_packet);
        break;
      case BFS_REMOTE_OPERATION::MOVE:
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(MoveReqPacket)-sizeof(reqPacket.opCode));
        onMoveRequest(_packet);
        break;
      default:
        cout<<"UNKNOWN OPCODE:"<<opCode<<endl<<std::flush;
    }
  } catch(Exception &e){
    cout<<"Error in reading data loop:"<<e.message()<<endl<<std::flush;
    delete this;
  }
}

void BFSTcpServiceHandler::onShutdown(
    const Poco::AutoPtr<Poco::Net::ShutdownNotification>& pNf) {
  cout<<"onShutdown:"<<socket.peerAddress().toString()<<endl<<std::flush;

  //Call destructor of this class
  delete this;
}

void BFSTcpServiceHandler::onWriteable(
    const Poco::AutoPtr<Poco::Net::WritableNotification>& pNf) {
  static bool once = true;
  if(once) {
    cout<<"onWritable:"<<socket.peerAddress().toString()<<" keepAlive?"<<socket.getKeepAlive()<<" isBlocking?"<<socket.getBlocking()<<" noDeley?"<<socket.getNoDelay()<<endl<<std::flush;
    once = false;
  }
}

void BFSTcpServiceHandler::onTimeout(
    const Poco::AutoPtr<Poco::Net::TimeoutNotification>& pNf) {
  cout<<"onTimeout:"<<socket.peerAddress().toString()<<endl<<std::flush;
}

void BFSTcpServiceHandler::onError(
    const Poco::AutoPtr<Poco::Net::ErrorNotification>& pNf) {
  cout<<"onError:"<<socket.peerAddress().toString()<<endl<<std::flush;
}

void BFSTcpServiceHandler::onIdle(
    const Poco::AutoPtr<Poco::Net::IdleNotification>& pNf) {
  cout<<"onIdle:"<<socket.peerAddress().toString()<<endl<<std::flush;
}

void BFSTcpServiceHandler::onReadRequest(u_char *_packet) {

  ReadReqPacket *reqPacket = (ReadReqPacket*)_packet;

  reqPacket->offset = be64toh(reqPacket->offset);
  reqPacket->size = be64toh(reqPacket->size);
  string fileName = string(reqPacket->fileName);

  ReadResPacket resPacket;
  //resPacket.statusCode = htobe64(200);
  //resPacket.readSize = htobe64(reqPacket->size);
  resPacket.statusCode = htobe64(-1);
  resPacket.readSize = htobe64(0);

  //Find node
  FileNode* fNode = FileSystem::getInstance().findAndOpenNode(fileName);
  long read = 0;
  char *buffer = nullptr;
  if(fNode!=nullptr) {
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    //First check how much we can read!
    buffer = new char[reqPacket->size];
    read = fNode->read(buffer,reqPacket->offset,reqPacket->size);
    //Now we can close the file
    fNode->close(inodeNum);
    //Set result
    if(read>=0) {
      resPacket.statusCode = htobe64(200);
      resPacket.readSize = htobe64(read);
    }
  }


  try {
    socket.sendBytes((void*)&resPacket,sizeof(ReadResPacket));
    if(read > 0){
      //Now send Actual Data
      uint64_t left = read;
      uint64_t total = read;

      FUSESwift::Timer t;
      t.begin();
      do {
        int sent = socket.sendBytes(buffer+(total-left),left);
        left -= sent;
        if(sent <= 0){//Error
          cout<<"ERROR in sending data, asked to send:"<<left<<" but sent:"<<sent<<endl<<std::flush;
          break;
        }
      }while(left);

      t.end();
      cout<<"Sent:"<<total<<" bytes in:"<<t.elapsedMillis()<<" milli"<<endl<<flush;
      //cout<<"Sent:"<<be64toh(resPacket.readSize)<<" bytes."<<endl<<std::flush;
    }
  } catch(...){
    cout<<"Error in sending read response:"<<fileName<<endl<<std::flush;
    delete this;
  }



  if(buffer)
    delete []buffer;

}

void BFSTcpServiceHandler::onWriteRequest(u_char *_packet) {
  WriteReqPacket *reqPacket = (WriteReqPacket*)_packet;

  reqPacket->offset = be64toh(reqPacket->offset);
  reqPacket->size = be64toh(reqPacket->size);
  string fileName = string(reqPacket->fileName);

  //Write Res
  WriteResPacket writeResPacket;
  writeResPacket.statusCode = htobe64(-1);
  writeResPacket.writtenSize = 0;

  char* buff = new char[reqPacket->size];
  try {
    int count = 0;
    uint64_t total = 0;
    uint64_t left = reqPacket->size;
    do {
      count = socket.receiveBytes((void*)((char*)buff+total),left);
      total += count;
      left -= count;
      //cout<<"total:"<<total<<" count:"<<count<<" left:"<<left<<" Avail:"<<socket.available()<<endl<<flush;
    } while(left > 0 && count);
    if(total == reqPacket->size){//Success
      writeResPacket.statusCode = htobe64(200);
      writeResPacket.writtenSize = htobe64(total);
    }
  }catch(Exception &e){
    cout<<"Error in receiving write data "<<reqPacket->fileName<<": "<<e.message()<<endl<<std::flush;
    delete this;
  }
  //Send response packet
  try {
    socket.sendBytes((void*)&writeResPacket,sizeof(WriteResPacket));
  }catch(Exception &e){
    cout<<"Error in sending write response packet "<<reqPacket->fileName<<": "<<e.message()<<endl<<std::flush;
    delete this;
  }
}

void BFSTcpServiceHandler::onAttribRequest(u_char *_packet) {
  AttribReqPacket *reqPacket = (AttribReqPacket*)_packet;
  string fileName = string(reqPacket->fileName);

  AttribResPacket resPacket;
  resPacket.statusCode = htobe64(-1);
  resPacket.attribSize = 0;
  struct stat data;

  //Find node
  FileNode* fNode = FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode!=nullptr) {
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    //Offset is irrelevant here and we use it for indicating success for failure
    resPacket.statusCode = htobe64(200);
    resPacket.attribSize = htobe64(sizeof(struct stat));
    fNode->getStat(&data);
    fNode->close(inodeNum);
  }

  try {
    socket.sendBytes((void*)&resPacket,sizeof(AttribResPacket));
  } catch(...){
    cout<<"Error in sending attrib response:"<<fileName<<endl<<std::flush;
    delete this;
  }

  if(fNode!=nullptr) {//Success
    try{
      //Now send Actual Data
      uint64_t left = be64toh(resPacket.attribSize);
      uint64_t total = left;
      do{
        int sent = socket.sendBytes((char*)&data+(total-left),left);
        left -= sent;
        if(sent <= 0){//Error
          cout<<"ERROR in sending data, asked to send:"<<left<<" but sent:"<<sent<<endl<<std::flush;
          break;
        }
      }while(left);
    } catch(...){
      cout<<"Error in sending attrib response:"<<fileName<<endl<<std::flush;
      delete this;
    }
  }
}

void BFSTcpServiceHandler::onDeleteRequest(u_char *_packet) {
}

void BFSTcpServiceHandler::onTruncateRequest(u_char *_packet) {
}

void BFSTcpServiceHandler::onCreateRequest(u_char *_packet) {
}

void BFSTcpServiceHandler::onMoveRequest(u_char *_packet) {
}

