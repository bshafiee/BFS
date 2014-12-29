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

#include "BFSTcpServiceHandler.h"
#include "BFSTcpServer.h"
#include <Poco/NObserver.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Exception.h>
#include "ZooHandler.h"
#include "MemoryController.h"
#include <sys/stat.h>
#include <endian.h>
#include <string>
#include <iostream>

#include "Filenode.h"
#include "Filesystem.h"
#include "Timer.h"
#include "LoggerInclude.h"

using namespace Poco;
using namespace Poco::Net;
using namespace std;
//using namespace FUSESwift;



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
  //LOG(ERROR)<<"onReadable:"<<socket.peerAddress().toString();

  //Read First Packet to Figure out
  int len = 2000;
  u_char _packet[len];
  ReqPacket reqPacket;
  try {
    //IMPORTANT ONLY READ AS BIG AS OPCODE
    int read = socket.receiveBytes(_packet,sizeof(reqPacket.opCode));
    if(read == 0){
      //
      //LOG(ERROR)<<"onReadable size of 0, Means the other peer of this connection is dead!";
      delete this;
      return;
    }

    ReqPacket* reqPacketPtr = reinterpret_cast<ReqPacket*>(_packet);
    //Check opcode
    uint32_t opCode = ntohl(reqPacketPtr->opCode);


    switch(toBFSRemoteOperation(opCode)) {
      case BFS_REMOTE_OPERATION::READ:
        //Read the rest of request packet
        socket.receiveBytes(_packet+sizeof(reqPacket.opCode),sizeof(ReadReqPacket)-sizeof(reqPacket.opCode));
        onReadRequest(_packet);
        //LOG(ERROR)<<"READ SERVERD IN :"<<t.elapsedMicro()<<" Micro T1:"<<t1<<" Total:"<<t1+t2;
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
        LOG(ERROR)<<"UNKNOWN OPCODE:"<<opCode;
    }
  } catch(Exception &e){
    LOG(ERROR)<<"Error in reading data loop:"<<e.message();
    delete this;
  }
  //So after a connection is served just close it!
  delete this;
}

void BFSTcpServiceHandler::onShutdown(
    const Poco::AutoPtr<Poco::Net::ShutdownNotification>& pNf) {
  LOG(ERROR)<<"onShutdown:"<<socket.peerAddress().toString();

  //Call destructor of this class
  delete this;
}

void BFSTcpServiceHandler::onWriteable(
    const Poco::AutoPtr<Poco::Net::WritableNotification>& pNf) {
  static bool once = true;
  if(once) {
    LOG(ERROR)<<"onWritable:"<<socket.peerAddress().toString()<<" keepAlive?"<<socket.getKeepAlive()<<" isBlocking?"<<socket.getBlocking()<<" noDeley?"<<socket.getNoDelay();
    once = false;
  }
}

void BFSTcpServiceHandler::onTimeout(
    const Poco::AutoPtr<Poco::Net::TimeoutNotification>& pNf) {
  LOG(ERROR)<<"onTimeout:"<<socket.peerAddress().toString();
}

void BFSTcpServiceHandler::onError(
    const Poco::AutoPtr<Poco::Net::ErrorNotification>& pNf) {
  LOG(ERROR)<<"onError:"<<socket.peerAddress().toString();
}

void BFSTcpServiceHandler::onIdle(
    const Poco::AutoPtr<Poco::Net::IdleNotification>& pNf) {
  LOG(ERROR)<<"onIdle:"<<socket.peerAddress().toString();
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
  FUSESwift::FileNode* fNode = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  long read = 0;
  char *buffer = nullptr;
  if(fNode!=nullptr) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
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

      /*FUSESwift::Timer t;
      t.begin();*/
      do {
        int sent = socket.sendBytes(buffer+(total-left),left);
        left -= sent;
        if(sent <= 0){//Error
          LOG(ERROR)<<"ERROR in sending data, asked to send:"<<left<<" but sent:"<<sent;
          break;
        }
      }while(left);

      //t.end();
      //LOG(ERROR)<<"Sent:"<<total<<" bytes in:"<<t.elapsedMillis()<<" milli";
      //LOG(ERROR)<<"Sent:"<<be64toh(resPacket.readSize)<<" bytes.";
    }
  } catch(...){
    LOG(ERROR)<<"Error in sending read response:"<<fileName;
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

  //Find node
  FUSESwift::FileNode* fNode = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode!=nullptr) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    char* buff = new char[reqPacket->size];
    try {
      int count = 0;
      uint64_t total = 0;
      uint64_t left = reqPacket->size;
      do {
        count = socket.receiveBytes((void*)((char*)buff+total),left);
        total += count;
        left -= count;
        //LOG(ERROR)<<"total:"<<total<<" count:"<<count<<" left:"<<left<<" Avail:"<<socket.available();
      } while(left > 0 && count);
      if(total == reqPacket->size){//Success
        FUSESwift::FileNode* afterMove = nullptr;//If it does not fit in out memory
        long result = fNode->writeHandler(buff,reqPacket->offset,reqPacket->size,afterMove);
        if(afterMove)
          fNode = afterMove;
        writeResPacket.statusCode = htobe64(result);
      }else
        LOG(ERROR)<<"reading write data failed:total:"<<total<<" count:"<<count<<" left:"<<left<<" Avail:"<<socket.available();
    }catch(Exception &e){
      LOG(ERROR)<<"Error in receiving write data "<<reqPacket->fileName<<": "<<e.message();
      delete this;
    }
    delete []buff;
    buff = nullptr;
    //Close file
    fNode->close(inodeNum);
  }

  //Send response packet
  try {
    socket.sendBytes((void*)&writeResPacket,sizeof(WriteResPacket));
  }catch(Exception &e){
    LOG(ERROR)<<"Error in sending write response packet "<<reqPacket->fileName<<": "<<e.message();
    delete this;
  }
}

void BFSTcpServiceHandler::onAttribRequest(u_char *_packet) {
  AttribReqPacket *reqPacket = (AttribReqPacket*)_packet;
  string fileName = string(reqPacket->fileName);

  AttribResPacket resPacket;
  resPacket.statusCode = htobe64(-1);
  resPacket.attribSize = 0;
  struct packed_stat_info data;

  //Find node
  FUSESwift::FileNode* fNode = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode!=nullptr) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    //Offset is irrelevant here and we use it for indicating success for failure
    resPacket.statusCode = htobe64(200);
    resPacket.attribSize = htobe64(sizeof(struct packed_stat_info));
    fNode->fillPackedStat(data);

    fNode->close(inodeNum);
  }

  try {
    socket.sendBytes((void*)&resPacket,sizeof(AttribResPacket));
  } catch(...){
    LOG(ERROR)<<"Error in sending attrib response:"<<fileName;
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
          LOG(ERROR)<<"ERROR in sending data, asked to send:"<<left<<" but sent:"<<sent;
          break;
        }
      }while(left);
    } catch(...){
      LOG(ERROR)<<"Error in sending attrib response:"<<fileName;
      delete this;
    }
  }
}

void BFSTcpServiceHandler::onDeleteRequest(u_char *_packet) {
  DeleteReqPacket *reqPacket = (DeleteReqPacket*)_packet;
  string fileName = string(reqPacket->fileName);

  DeleteResPacket resPacket;
  resPacket.statusCode = htobe64(-1);

  //Find node
  FUSESwift::FileNode* fNode = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode!=nullptr && !fNode->isRemote()) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    LOG(DEBUG)<<"SIGNAL DELETE FROM DELETE TCP REQUEST:"<<fNode->getFullPath();
    FUSESwift::FileSystem::getInstance().signalDeleteNode(fNode,false);
    fNode->close(inodeNum);
    resPacket.statusCode = htobe64(200);
  } else if(fNode!=nullptr && fNode->isRemote()) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    fNode->close(inodeNum);
    LOG(ERROR)<<"Error! got a delete request for a node which "
        "I'm not responsible for:"<<fileName;
  } else {
    LOG(ERROR)<<"Error! got a delete request for a non existent node"
        <<fileName;
  }

  try {
    socket.sendBytes((void*)&resPacket,sizeof(DeleteResPacket));
  } catch(...){
    LOG(ERROR)<<"Error in sending Delete response:"<<fileName;
    delete this;
  }
}

void BFSTcpServiceHandler::onTruncateRequest(u_char *_packet) {
  TruncReqPacket *reqPacket = (TruncReqPacket*)_packet;
  string fileName = string(reqPacket->fileName);
  uint64_t newSize = be64toh(reqPacket->size);

  TruncateResPacket resPacket;
  resPacket.statusCode = htobe64(-1);

  //Find node
  FUSESwift::FileNode* fNode = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode!=nullptr && !fNode->isRemote()) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    bool res = fNode->truncate(newSize);
    fNode->close(inodeNum);
    if(res)
      resPacket.statusCode = htobe64(200);
    else
      resPacket.statusCode = htobe64(-2);
  } else if(fNode!=nullptr && fNode->isRemote()) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    fNode->close(inodeNum);
    LOG(ERROR)<<"Error! got a delete request for a node which "
        "I'm not responsible for:"<<fileName;
  } else {
    LOG(ERROR)<<"Error! got a truncate request for a non existent node"
        <<fileName;
  }

  try {
    socket.sendBytes((void*)&resPacket,sizeof(TruncateResPacket));
  } catch(...){
    LOG(ERROR)<<"Error in sending Truncate response:"<<fileName;
    delete this;
  }
}

void BFSTcpServiceHandler::onCreateRequest(u_char *_packet) {
  CreateReqPacket *reqPacket = (CreateReqPacket*)_packet;
  string fileName = string(reqPacket->fileName);

  CreateResPacket resPacket;
  resPacket.statusCode = htobe64(-1);

  //First update your view of work then decide!
  FUSESwift::ZooHandler::getInstance().requestUpdateGlobalView();

  //Find node
  FUSESwift::FileNode* existing = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  if(existing) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)existing);
    if(existing->isRemote()) {
      //Close it!
      existing->close(inodeNum);
      LOG(ERROR)<<"Error! got a delete request for a node which "
          "I'm not responsible for:"<<fileName;
    } else { //overwrite file=>truncate to 0
      bool res = existing->truncate(0);
      existing->close(inodeNum);
      if(res)
        resPacket.statusCode = htobe64(200);
      else
        resPacket.statusCode = htobe64(-2);
    }
  } else {
    bool res = (FUSESwift::FileSystem::getInstance().mkFile(fileName,false,false)!=nullptr)?true:false;
    if(res)
      resPacket.statusCode = htobe64(200);
    else
      resPacket.statusCode = htobe64(-3);
  }

  try {
    socket.sendBytes((void*)&resPacket,sizeof(TruncateResPacket));
  } catch(...){
    LOG(ERROR)<<"Error in sending Create response:"<<fileName;
    delete this;
  }
}

void BFSTcpServiceHandler::onMoveRequest(u_char *_packet) {
  MoveReqPacket *reqPacket = (MoveReqPacket*)_packet;
  string fileName = string(reqPacket->fileName);

  MoveResPacket resPacket;
  resPacket.statusCode = htobe64(-1);

  //First update your view of work then decide!
  FUSESwift::ZooHandler::getInstance().requestUpdateGlobalView();

  //Manipulate file
  FUSESwift::FileNode* existing = FUSESwift::FileSystem::getInstance().findAndOpenNode(fileName);
  if(existing) {
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)existing);
    if(existing->isRemote()) {//I'm not responsible! I should not have seen this.
      //Close it!
      existing->close(inodeNum);
      LOG(ERROR)<<"Going to move file:"<<fileName<<" to here!";

      //Handle Move file to here!
      resPacket.statusCode = htobe64(processMoveRequest(fileName));
    } else { //overwrite file=>truncate to 0
      existing->close(inodeNum);
      LOG(ERROR)<<"EXIST and LOCAL, This should be a create request not a move!:"<<fileName;
      resPacket.statusCode = htobe64(-3);
    }
  } else {
    LOG(ERROR)<<"DOES NOT EXIST, This should be a create request not a move!:"<<fileName;
    resPacket.statusCode = htobe64(-4);
  }


  try {
    socket.sendBytes((void*)&resPacket,sizeof(MoveResPacket));
  } catch(...){
    LOG(ERROR)<<"Error in sending Move response:"<<fileName;
    delete this;
  }
}

int64_t BFSTcpServiceHandler::processMoveRequest(std::string _fileName) {
  //1) find file
  FUSESwift::FileNode* file = FUSESwift::FileSystem::getInstance().findAndOpenNode(_fileName);
  if(file != nullptr) { //2) Open file
    uint64_t inodeNum = FUSESwift::FileSystem::getInstance().assignINodeNum((intptr_t)file);
    //3)check size
    struct stat st;
    if(file->getStat(&st)) {
      //If we have enough space (2 times of current space
      if(st.st_size * 2 < FUSESwift::MemoryContorller::getInstance().getAvailableMemory()) {
        //4) we have space to start to read the file
        uint64_t bufferLen = 1024ll*1024ll*100ll;//100MB
        char *buffer = new char[bufferLen];
        uint64_t left = st.st_size;
        uint64_t offset = 0;
        while(left > 0) {
          long read = file->readRemote(buffer,offset,bufferLen);
          if(read <= 0 )
            break;
          FUSESwift::FileNode* afterMove;//This won't happen(should not)
          if(read != file->writeHandler(buffer,offset,read,afterMove))//error in writing
            break;
          left -= read;
          offset += read;
        }
        delete []buffer;//Release memory

        if(left == 0) {//Successful read
          //First make file local because the other side removes it and zookeeper will try to remove it!
          //if failed we will return it to remote
          file->makeLocal();
          //We remotely delete that file
          if(BFSTcpServer::deleteRemoteFile(_fileName,this->socket.peerAddress().host().toString(),BFSTcpServer::getPort())) {
            //Everything went well
            FUSESwift::ZooHandler::getInstance().publishListOfFiles();//Inform rest of world
            return 200;
          } else {
            LOG(ERROR) <<"Failed to delete remote file:"<<_fileName;
            LOG(ERROR) <<"DEALLOCATE deallocate"<<_fileName;
            file->makeRemote();
            file->deallocate();//Release memory allocated to the file
            return -2;
          }
        } else {
          LOG(ERROR) <<"reading remote File/writing to local one failed:"<<_fileName;
          return -3;
        }
      }
      else {
        LOG(ERROR) <<"Not enough space to move: "<<_fileName<<" here";
        return -4;
      }
    } else {
      LOG(ERROR) <<"Get Remote File Stat FAILED:"<<_fileName;
      return -5;
    }

    file->close(inodeNum);
  } else {
    LOG(ERROR) <<"Cannot find fileNode:"<<_fileName;
    return -6;
  }
  return -7;
}

