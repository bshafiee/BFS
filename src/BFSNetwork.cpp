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

#include "ZeroNetwork.h"
#include "BFSNetwork.h"
#include "ZooHandler.h"
#include "SettingManager.h"
#include "MemoryController.h"
#include "LoggerInclude.h"
#include "Timer.h"
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <arpa/inet.h>

#include "Filenode.h"
#include "Filesystem.h"
extern "C" {
	#include <pfring.h>
}


namespace FUSESwift {

using namespace std;

/** static members **/
string BFSNetwork::DEVICE= "eth0";
unsigned char BFSNetwork::MAC[6] = {0,0,0,0,0,0};
atomic<bool> BFSNetwork::macInitialized(false);
taskMap<uint32_t,ReadRcvTask*> BFSNetwork::readRcvTasks(2000);
taskMap<uint32_t,WriteDataTask> BFSNetwork::writeDataTasks(2000);
taskMap<uint32_t,ReadRcvTask*> BFSNetwork::attribRcvTasks(2000);
taskMap<uint32_t,WriteSndTask*> BFSNetwork::deleteSendTasks(2000);
taskMap<uint32_t,WriteSndTask*> BFSNetwork::truncateSendTasks(2000);
taskMap<uint32_t,WriteSndTask*> BFSNetwork::createSendTasks(2000);
taskMap<uint32_t,MoveConfirmTask*> BFSNetwork::moveConfirmSendTasks(1000);
atomic<bool> BFSNetwork::isRunning(true);
atomic<bool> BFSNetwork::rcvLoopDead(false);
Queue<SndTask*> BFSNetwork::sendQueue;
Queue<MoveTask*> BFSNetwork::moveQueue;
thread * BFSNetwork::rcvThread = nullptr;
thread * BFSNetwork::sndThread = nullptr;
thread * BFSNetwork::moveThread = nullptr;
atomic<uint32_t> BFSNetwork::fileIDCounter(0);
unsigned int BFSNetwork::DATA_LENGTH = -1;

BFSNetwork::BFSNetwork(){}

BFSNetwork::~BFSNetwork() {}

bool BFSNetwork::startNetwork() {
	string devName = SettingManager::get(CONFIG_KEY_ZERO_NETWORK_DEV);
	if(devName.length() > 0)
		DEVICE = devName;
	else
	  LOG(DEBUG) <<"No device specified in the config file!\n";
	//Get Mac Address
	int mtu = -1;
	getMacAndMTU(DEVICE,MAC,mtu);
	macInitialized.store(true);
	DATA_LENGTH = mtu - HEADER_LEN;
	if(!ZeroNetwork::initialize(DEVICE,mtu))
		return false;
	isRunning.store(true);
	//Cleanup first
	if(rcvThread != nullptr) {
		delete rcvThread;
		rcvThread = nullptr;
	}
	if(sndThread != nullptr) {
		delete sndThread;
		sndThread = nullptr;
	}
  if(moveThread != nullptr) {
    delete moveThread;
    moveThread = nullptr;
  }
	//Start rcv Loop
	rcvThread = new thread(rcvLoop);
	//Start Send Loop
  sndThread = new thread(sendLoop);
  //Start Move Loop
  moveThread = new thread(moveLoop);

	return true;
}

void BFSNetwork::stopNetwork() {
	isRunning.store(false);
	moveQueue.stop();
	sendQueue.stop();
	pfring_breakloop((pfring*)pd);
	while(!rcvLoopDead);//Wait until rcvloop is dead
	shutDown();
}

void BFSNetwork::fillBFSHeader(char* _packet,const unsigned char _dstMAC[6]) {
	//Fill dst and src MAC
	for(unsigned int i=0;i<6;i++) {
		_packet[i] = _dstMAC[i];
		_packet[i+6] = MAC[i];//Simultaneously fill src_mac as well
	}
	//Fill Poroto
	_packet[PROTO_BYTE_INDEX1] = BFS_PROTO_BYTE1;
	_packet[PROTO_BYTE_INDEX2] = BFS_PROTO_BYTE2;
}

/*
* READ_REQUEST Payload:
* 15__________________________________PAYLOAD_________________________________
* |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
* | 4 Byte Op-Code   |    Offset    |     size     |        FILE NAME         |
* | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
* | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
* | Index: [18-21]   |              |              |                          |
* |__________________|______________|______________|_____________________MTU-1|
**/
void BFSNetwork::fillReadReqPacket(ReadReqPacket* _packet,
    const unsigned char _dstMAC[6], const ReadRcvTask& _rcvTask, uint32_t _fileID) {
	//First fill header
	fillBFSHeader((char*)_packet,_dstMAC);
	/** Now fill request packet fields **/
	_packet->opCode = htonl((uint32_t)BFS_OPERATION::READ_REQUEST);
	//File ID
	_packet->fileID = htonl(_fileID);
	// Offset
	_packet->offset = htobe64(_rcvTask.offset);
	// size
	_packet->size = htobe64(_rcvTask.size);
	//Remote File name
	strncpy(_packet->fileName,_rcvTask.remoteFile.c_str(),_rcvTask.remoteFile.length());
	_packet->fileName[_rcvTask.remoteFile.length()]= '\0';
}

/*
* READ_RESPONSE Payload:
* 15__________________________________PAYLOAD_________________________________
* |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
* | 4 Byte Op-Code   |    Offset    |     size     |        READ DATA         |
* | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
* | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
* | Index: [18-21]   |              |              |                          |
* |__________________|______________|______________|_____________________MTU-1|
**/
void BFSNetwork::fillReadResPacket(ReadResPacket* _packet,const ReadSndTask& _sndTask){
	//First fill header
	fillBFSHeader((char*)_packet,_sndTask.requestorMac);
	/** Now fill request packet fields **/
	_packet->opCode = htonl((uint32_t)BFS_OPERATION::READ_RESPONSE);
	/** Now fill request packet fields **/
	//File id
	_packet->fileID = htonl(_sndTask.fileID);
	// Offset
	_packet->offset = htobe64(_sndTask.offset);
	// size
	_packet->size = htobe64(_sndTask.size);
	/*//Data
	memcpy(_packet->data,(unsigned char*)_sndTask.data+_sndTask.offset,_sndTask.size);*/
}


long BFSNetwork::readRemoteFile(void* _dstBuffer, size_t _size, size_t _offset,
    const std::string& _remoteFile, const unsigned char _dstMAC[6]) {

	long result = -1;
	if(_remoteFile.length() > DATA_LENGTH){
	  LOG(ERROR) <<"Filename too long:"<<_remoteFile;
		result = -1;
		return result;
	}

	//Create a new task
	ReadRcvTask task;
	task.dstBuffer = _dstBuffer;
	task.size = _size;
	task.offset = _offset;
	task.fileID = getNextFileID();
	task.remoteFile = _remoteFile;
	task.ready = false;
	task.totalRead = 0;//reset total read

	//Send file request
	char buffer[MTU];
	ReadReqPacket *reqPacket = (ReadReqPacket*)buffer;
	fillReadReqPacket(reqPacket,_dstMAC,task,task.fileID);

	//Put it on the rcvTask map!
	auto resPair = readRcvTasks.insert(task.fileID,&task);
	if(!resPair.second) {
	  LOG(ERROR) <<"error in inserting task to the rcvQueue!,"
				"Cannot handle more than uint32_t.MAX concurrent operations";
		result = -1;
		return result;
	}

	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU)) {
	  LOG(ERROR) <<"Failed to send readReqpacket.";
		result = -1;
		readRcvTasks.erase(resPair.first);
		return result;
	}


	//Now wait for it!
  unique_lock<std::mutex> lk(task.m);
	while(!task.ready) {
		task.cv.wait(lk);
		if (!task.ready) {
		  LOG(ERROR) <<"HOLY SHIT! Spurious wake up!";
			lk.unlock();
			readRcvTasks.erase(resPair.first);
			result = -1;
			return result;
		}
	}
	result = task.totalRead;
	lk.unlock();


	//remove it from queue
	readRcvTasks.erase(resPair.first);
	return result;
}

void BFSNetwork::sendLoop() {

	while(isRunning) {
		//First element
		SndTask* firstTask = sendQueue.front();

		if(unlikely(!isRunning))
		  break;

		switch(firstTask->type) {
			case SEND_TASK_TYPE::SEND_READ:
				processReadSendTask(*(ReadSndTask*)firstTask);
				sendQueue.pop();
				delete firstTask;firstTask = nullptr;
				break;
			case SEND_TASK_TYPE::SEND_WRITE:
				processWriteSendTask(*(WriteSndTask*)firstTask);
				sendQueue.pop();
				break;
		}
	}
	LOG(ERROR)<<"SEND LOOP DEAD!";
}

void FUSESwift::BFSNetwork::processReadSendTask(ReadSndTask& _task) {
	uint64_t total = _task.size;
	uint64_t left = _task.size;

	//Find file and lock it!
	FileNode* fNode = FileSystem::getInstance().findAndOpenNode(_task.localFile);
	if(fNode == nullptr || _task.size == 0|| fNode->isRemote()) {
	  //error just send a packet of size 0
		char buffer[MTU];
		ReadResPacket *packet = (ReadResPacket*)buffer;
		fillReadResPacket(packet,_task);
		packet->size = 0;
		packet->offset = 0;
		//Now send it
		int retry = 3;
		while(retry > 0) {
			if(send(buffer,MTU) == MTU)
				break;
			retry--;
		}

		if(fNode!=nullptr && fNode->isRemote()){
			uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
			fNode->close(inodeNum);
		  LOG(ERROR)<<"Request to read a remote file from a non responsible node.";
		}

		return;
	}

	//File is Open so assign inode number
	uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);

	uint64_t localOffset = 0;
	//Now we have the file node!
  while(left > 0) {
	unsigned long howMuch = (left > DATA_LENGTH)?DATA_LENGTH:left;
  	//Make a response packet
  	_task.size = howMuch;
  	char buffer[MTU];
  	ReadResPacket *packet = (ReadResPacket*)buffer;
  	//fillReadResPacket(packet,_task);
  	char *dstBuf = (char*)(packet->data);
  	long readCount = fNode->read(dstBuf,(size_t)_task.offset,(size_t)howMuch);
  	//memcpy(_packet->data,(unsigned char*)_sndTask.data+_sndTask.offset,_sndTask.size);
  	if(readCount <= 0){
  		_task.size = 0;
  		packet->size = 0;
  		left = 0;
  		howMuch = 0;
  	}
  	else {
  		_task.size = readCount;
  		howMuch = readCount;
  	}
  	//Update packet content
  	fillReadResPacket(packet,_task);
  	//Update packet offset with local offset
  	//packet->offset = htobe64(localOffset);

  	//Now send it
  	int retry = 3;
  	while(retry > 0) {
  		if(send(buffer,MTU) == MTU)
  			break;
  		retry--;
  	}
  	if(!retry) {
  		LOG(ERROR) <<"Failed to send packet through ZeroNetwork:"<<_task.localFile;
  	  fNode->close(inodeNum);
  		return;
  	}
  	//Increment info
  	_task.offset += howMuch;
  	localOffset += howMuch;
  	left -= howMuch;
  }

  //close file
  fNode->close(inodeNum);
  _task.size = total;

}

void BFSNetwork::moveLoop() {
  while(isRunning) {
    MoveTask* front = moveQueue.front();

    if(unlikely(!isRunning))
      break;

    processMoveTask(*front);
    delete front;
    front = nullptr;
    moveQueue.pop();
  }
  LOG(ERROR)<<"MOVE LOOP DEAD!";
}

void BFSNetwork::processMoveTask(const MoveTask &_moveTask) {
  //Response Packet
  char buffer[MTU];
  WriteDataPacket *moveAck = (WriteDataPacket *)buffer;
  fillBFSHeader((char*)moveAck,_moveTask.requestorMac);
  moveAck->opCode = htonl((uint32_t)BFS_OPERATION::CREATE_RESPONSE);
  //set fileID
  moveAck->fileID = htonl(_moveTask.fileID);
  //Size == 0 means failed! if create successful size will be > 0
  moveAck->size = 0;//zero is zero anyway

  bool res = false;

  //1) find file
  FileNode* file = FileSystem::getInstance().findAndOpenNode(_moveTask.fileName);
  if(file != nullptr) { //2) Open file
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)file);
    //3)check size
    struct stat st;
    if(file->getStat(&st)) {
      //If we have enough space (2 times of current space
      if(st.st_size * 2 < MemoryContorller::getInstance().getAvailableMemory()) {
        //4) we have space to start to read the file
        uint64_t bufferLen = 1024ll*1024ll*100ll;//100MB
        char *buffer = new char[bufferLen];
        uint64_t left = st.st_size;
        uint64_t offset = 0;
        while(left > 0) {
          long read = file->readRemote(buffer,offset,bufferLen);
          if(read <= 0 )
            break;
          FileNode* afterMove;//This won't happen(should not)
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
          if(deleteRemoteFile(_moveTask.fileName,_moveTask.requestorMac)) {
            //Everything went well
            ZooHandler::getInstance().publishListOfFiles();//Inform rest of world
            res = true;
          } else {
            LOG(ERROR) <<"Failed to delete remote file:"<<_moveTask.fileName;
            LOG(ERROR) <<endl<<"DEALLOCATE deallocate"<<_moveTask.fileName<<endl;
            file->makeRemote();
            file->deallocate();//Release memory allocated to the file
          }
        } else {
          LOG(ERROR) <<"reading remote File/writing to local one failed:"<<_moveTask.fileName;
        }
      }
      else {
        LOG(ERROR) <<"Not enough space to move: "<<_moveTask.fileName<<" here";
      }
    } else {
      LOG(ERROR) <<"Get Remote File Stat FAILED:"<<_moveTask.fileName;
    }

    file->close(inodeNum);
  } else {
    LOG(ERROR) <<"Cannot find fileNode:"<<_moveTask.fileName;
  }


  if(res){//Success
    moveAck->size = htobe64(1);
    LOG(ERROR) <<"MOVE SUCCESS TO HERE:"<<_moveTask.fileName;
  }
  else {
    moveAck->size = 0;
    LOG(ERROR) <<"MOVE FAILED TO HERE:"<<_moveTask.fileName;
  }

  //Send the ack on the wire
  if(!ZeroNetwork::send(buffer,MTU)) {
    LOG(ERROR) <<"Failed to send createAckpacket.";
  }
}

//BFS_OPERATION {REQUEST_FILE = 1, RESPONSE = 2};
static inline BFS_OPERATION toBFSOperation(uint32_t op){
	switch (op) {
		case 1:
			return BFS_OPERATION::READ_REQUEST;
		case 2:
			return BFS_OPERATION::READ_RESPONSE;
		case 3:
			return BFS_OPERATION::WRITE_REQUEST;
		case 4:
			return BFS_OPERATION::WRITE_DATA;
		case 5:
			return BFS_OPERATION::WRITE_ACK;
		case 6:
			return BFS_OPERATION::ATTRIB_REQUEST;
		case 7:
			return BFS_OPERATION::ATTRIB_RESPONSE;
    case 8:
      return BFS_OPERATION::DELETE_REQUEST;
    case 9:
      return BFS_OPERATION::DELETE_RESPONSE;
    case 10:
      return BFS_OPERATION::TRUNCATE_REQUEST;
    case 11:
      return BFS_OPERATION::TRUNCATE_RESPONSE;
    case 12:
      return BFS_OPERATION::CREATE_REQUEST;
    case 13:
      return BFS_OPERATION::CREATE_RESPONSE;
	}
	return BFS_OPERATION::UNKNOWN;
}

void BFSNetwork::rcvLoop() {
  //Increase Priority of rcvLoop to max
  /*int policy;
  struct sched_param param;
  pthread_getschedparam(pthread_self(), &policy, &param);
  param.sched_priority = sched_get_priority_max(policy);
  pthread_setschedparam(pthread_self(), policy, &param);*/


  //unsigned char _buffer[MTU];

	while(isRunning){
		//Get a packet
		struct pfring_pkthdr _header;
		u_char *_packet = nullptr;
		int res = pfring_recv((pfring*)pd,&_packet,0,&_header,1);
		/*static uint64_t rcv = 0;
		static uint64_t notmine = 0;*/

		if(res <= 0){
		  if(isRunning)
		    LOG(ERROR)<<"Error in pfring_recv:"<<res;
		  else
		    break;
			continue;
		}
		//rcv++;
    //Return not a valid packet of our protocol!
    if(_packet[PROTO_BYTE_INDEX1] != BFS_PROTO_BYTE1 ||
       _packet[PROTO_BYTE_INDEX2] != BFS_PROTO_BYTE2 ){
      /*printf("Byte[%d]=%.2x,Byte[%d]=%.2x\n",PROTO_BYTE_INDEX1,
          _packet[PROTO_BYTE_INDEX1],PROTO_BYTE_INDEX2,_packet[PROTO_BYTE_INDEX2]);*/
      //notmine++;
      continue;
    }

		//return if it's not of our size
		if(_header.len != (unsigned int)MTU) {
		  LOG(ERROR)<<"Fragmentation! dropping. CapLen:"<<_header.caplen <<"Len:"<<_header.len;
			continue;
		}

		//Check if it is for us! MAC
		bool validMac = true;
		for(unsigned int i=DST_MAC_INDEX;i<DST_MAC_INDEX+6;i++)
			if(_packet[i] != MAC[i-DST_MAC_INDEX]){
//				fprintf(stderr,"NOT for me: myMac:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\trcv"
//												"MAC:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
//												MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5],
//												_packet[DST_MAC_INDEX],_packet[DST_MAC_INDEX+1],
//												_packet[DST_MAC_INDEX+2],_packet[DST_MAC_INDEX+3],
//												_packet[DST_MAC_INDEX+4],_packet[DST_MAC_INDEX+5]);
				validMac = false;
				break;
			}
		if(!validMac)
			continue;

		pfring_stat stats;
		pfring_stats((pfring*)pd,&stats );
		static uint64_t dropped = 0;
		if(stats.drop - dropped > 0) {
		  onReadResPacket(nullptr);
		  dropped = stats.drop;
		}


		/** Seems we got a relavent packet! **/
		//Check opcode
		//NO MATTER PACKET TYPE< OPCODE INDEX IS FIXED
		uint32_t opCode = ((WriteReqPacket*)_packet)->opCode;;

		switch(toBFSOperation(ntohl(opCode))){
			case BFS_OPERATION::READ_REQUEST:
				onReadReqPacket(_packet);
				break;
			case BFS_OPERATION::READ_RESPONSE:
				onReadResPacket(_packet);
				break;
			case BFS_OPERATION::WRITE_REQUEST:
				onWriteReqPacket(_packet);
				break;
			case BFS_OPERATION::WRITE_DATA:
				onWriteDataPacket(_packet);
				break;
			case BFS_OPERATION::WRITE_ACK:
				onWriteAckPacket(_packet);
				break;
			case BFS_OPERATION::ATTRIB_REQUEST:
				onAttribReqPacket(_packet);
				break;
			case BFS_OPERATION::ATTRIB_RESPONSE:
				onAttribResPacket(_packet);
				break;
      case BFS_OPERATION::DELETE_REQUEST:
        onDeleteReqPacket(_packet);
        break;
      case BFS_OPERATION::DELETE_RESPONSE:
        onDeleteResPacket(_packet);
        break;
      case BFS_OPERATION::TRUNCATE_REQUEST:
        onTruncateReqPacket(_packet);
        break;
      case BFS_OPERATION::TRUNCATE_RESPONSE:
        onTruncateResPacket(_packet);
        break;
      case BFS_OPERATION::CREATE_REQUEST:
        onCreateReqPacket(_packet);
        break;
      case BFS_OPERATION::CREATE_RESPONSE:
        onCreateResPacket(_packet);
        break;
			default:
			  LOG(ERROR)<<"UNKNOWN OPCODE:"<<opCode;
		}
	}
	LOG(ERROR)<<"RCV LOOP DEAD!";
	rcvLoopDead.store(true);
}

/**
 * READ_RESPONSE Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        READ DATA         |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
 *
 **/
void FUSESwift::BFSNetwork::onReadResPacket(const u_char* _packet) {
  if(_packet == nullptr) { //a drop has happened!
    int counter = 0;
    readRcvTasks.lock();
    for ( auto it = readRcvTasks.begin(); it != readRcvTasks.end(); ++it ) {
      ReadRcvTask* task = it->second;
      task->totalRead = 0;

      unique_lock<std::mutex> lk(task->m);
      task->ready = true;
      lk.unlock();
      task->cv.notify_one();
      counter++;
    }
    readRcvTasks.unlock();
    LOG(ERROR)<<"\n\n\n\n\n\n\n\n\n\ndrop happened:"<<counter<<"\n\n\n\n\n\n\n\n\n\n\n";
    return;
  }

	ReadResPacket *resPacket = (ReadResPacket*)_packet;
	//Parse Packet Network order first!
	resPacket->offset = be64toh(resPacket->offset);
	resPacket->size = be64toh(resPacket->size);
	resPacket->fileID = ntohl(resPacket->fileID);

	//Get RcvTask back using file id!
	auto taskIt = readRcvTasks.find(resPacket->fileID);
	readRcvTasks.lock();
	if(taskIt == readRcvTasks.end()) {
	  LOG(ERROR)<<"No valid Task for this Packet. FileID:"<<resPacket->fileID;
	  readRcvTasks.unlock();
		return;
	}

	//Exist! so fill task buffer
	ReadRcvTask* task = taskIt->second;
	if(resPacket->size) {
	  uint64_t delta = resPacket->offset - task->offset;
	  if(taskIt->first != task->fileID|| taskIt->first!= resPacket->fileID|| task->fileID!= resPacket->fileID)
	    LOG(ERROR)<<"Key:"<< taskIt->first<<" elementID:"<<task->fileID<<" packetID:"<<resPacket->fileID<<" second:%p"<<task;

		memcpy((char*)task->dstBuffer+delta,resPacket->data,resPacket->size);

	}

	//Check if finished, signal conditional variable!
	//If total amount of required data was read or a package of size 0 is received!
	if(resPacket->offset+resPacket->size == task->offset+task->size ||
			resPacket->size == 0) {
	  task->totalRead = resPacket->offset - task->offset + resPacket->size;
    unique_lock<std::mutex> lk(task->m);
    task->ready = true;
    lk.unlock();
    task->cv.notify_one();
	}
	readRcvTasks.unlock();
}

/**
 * READ_REQUEST Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        FILE NAME         |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
 **/
void FUSESwift::BFSNetwork::onReadReqPacket(const u_char* _packet) {
	ReadReqPacket *reqPacket = (ReadReqPacket*)_packet;
	//create a send packet
	ReadSndTask *task = new ReadSndTask();
	task->localFile = string(reqPacket->fileName);
	task->offset = be64toh(reqPacket->offset);
	task->size = be64toh(reqPacket->size);
	task->fileID = ntohl(reqPacket->fileID);
	memcpy(task->requestorMac,reqPacket->srcMac,6);

	//Push it on the Queue
	sendQueue.push(task);
}


long BFSNetwork::writeRemoteFile(const void* _srcBuffer, size_t _size,
    size_t _offset, const std::string& _remoteFile, const unsigned char _dstMAC[6]) {
	if(_remoteFile.length() > DATA_LENGTH){
	  LOG(ERROR)<<"Filename too long:"<<_remoteFile;
		return false;
	}

	//Create a new task
	WriteSndTask task;
	task.srcBuffer = _srcBuffer;
	task.size = _size;
	task.offset = _offset;
	task.fileID = getNextFileID();
	task.remoteFile = _remoteFile;
	task.acked = false;
	memcpy(task.dstMac,_dstMAC,6);
	task.ready = false;
	task.ack_ready = false;

	//Push it to the send Queue
	sendQueue.push(&task);

	//Now wait for it!
  unique_lock<std::mutex> lk(task.m);
  while(!task.ready) {
		task.cv.wait(lk);
		break;
	}

	if(task.acked)
		return _size;
	else
		return 0;
}


void FUSESwift::BFSNetwork::processWriteSendTask(WriteSndTask& _task) {
	long total = _task.size;

	//First Send a write request
	char buffer[MTU];
	WriteReqPacket *writeReqPkt = (WriteReqPacket*)buffer;
	fillWriteReqPacket(writeReqPkt,_task.dstMac,_task);
	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU)) {
	  LOG(ERROR)<<"Failed to send writeReqpacket.";
		return;
	}

	//Now send write_data
	uint64_t left = _task.size;
  while(left) {
  	unsigned long howMuch = (left > DATA_LENGTH)?DATA_LENGTH:left;
  	//Make a response packet
  	//_task.size = howMuch;
  	char buffer[MTU];
  	//memset(buffer,0,MTU);
  	WriteDataPacket *packet = (WriteDataPacket*)buffer;
  	fillWriteDataPacket(packet,_task,howMuch);
  	memcpy(packet->data,(unsigned char*)_task.srcBuffer+(total-left),howMuch);
  	//Now send it
  	int retry = 3;
  	while(retry > 0) {
  		if(send(buffer,MTU) == MTU)
  			break;
  		retry--;
  	}
  	if(!retry) {
  	  LOG(ERROR)<<"(processWriteSendTask)Failed to send packet "
  				"through ZeroNetwork:"<<_task.remoteFile;
  		return;
  	}
  	//Increment info
  	_task.offset += howMuch;
  	left -= howMuch;
  }
  //revert back the task size.
  _task.size = total;

  Timer t;
  unique_lock<mutex> ack_lk(_task.ack_m);
  t.begin();
  //Now wait for the ACK!
	while(!_task.ack_ready) {
		_task.ack_cv.wait_for(ack_lk,chrono::milliseconds(ACK_TIMEOUT));
		t.end();
		if(!_task.ack_ready && (t.elapsedMillis() >= ACK_TIMEOUT))
		  break;
	}


	if(!_task.ack_ready)
	  LOG(ERROR)<<"WriteRequest Timeout: fileID:"<<_task.fileID<<" ElapsedMILLIS:"<<t.elapsedMillis();

	if(!_task.acked && _task.ack_ready)
	  LOG(ERROR)<<"WriteRequest failed:%s"<<_task.remoteFile<<" acked?"<<_task.acked<<" ack_ready?"<<_task.ack_ready<<"FileID:"<<_task.fileID<<" Size:"<<_task.size<<" Offset:"<<_task.offset;
	ack_lk.unlock();

  //GOT ACK So Signal Caller to wake up
  unique_lock<mutex> lk(_task.m);
	_task.ready = true;
	lk.unlock();
	_task.cv.notify_one();
}


/*
 * WRITE_DATA Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        WRITE DATA        |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
* */
void BFSNetwork::fillWriteDataPacket(WriteDataPacket* _packet,
    const WriteSndTask& _writeTask,uint64_t _size) {
	//First fill header
	fillBFSHeader((char*)_packet,_writeTask.dstMac);
	_packet->opCode = htonl((uint32_t)BFS_OPERATION::WRITE_DATA);
	/** Now fill request packet fields **/
	//File id
	_packet->fileID = htonl(_writeTask.fileID);
	// Offset
	_packet->offset = htobe64(_writeTask.offset);
	// size
	_packet->size = htobe64(_size);
	//Data
	//memcpy(_packet->data,(unsigned char*)_writeTask.srcBuffer+_writeTask.offset,_writeTask.size);
}


/**
 * WRITE_REQUEST Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        FILE NAME         |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
**/
void BFSNetwork::fillWriteReqPacket(WriteReqPacket* _packet,
    const unsigned char _dstMAC[6], const WriteSndTask& _writeTask) {
	//First fill header
	fillBFSHeader((char*)_packet,_writeTask.dstMac);
	_packet->opCode = htonl((uint32_t)BFS_OPERATION::WRITE_REQUEST);
	_packet->fileID = htonl(_writeTask.fileID);
	/** Now fill request packet fields **/
	// Offset
	_packet->offset = htobe64(_writeTask.offset);
	// size
	_packet->size = htobe64(_writeTask.size);
	//Remote File name
	strncpy(_packet->fileName,_writeTask.remoteFile.c_str(),_writeTask.remoteFile.length());
	_packet->fileName[_writeTask.remoteFile.length()] = '\0';
}

/*
* WRITE_DATA Payload:
* 15__________________________________PAYLOAD_________________________________
* |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
* | 4 Byte Op-Code   |    Offset    |     size     |        WRITE DATA        |
* | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
* | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
* | Index: [18-21]   |              |              |                          |
* |__________________|______________|______________|_____________________MTU-1|
**/
void FUSESwift::BFSNetwork::onWriteDataPacket(const u_char* _packet) {
	WriteDataPacket *dataPacket = (WriteDataPacket*)_packet;
	//create a send packet
	uint64_t offset = be64toh(dataPacket->offset);
	uint64_t size = be64toh(dataPacket->size);
	uint32_t fileID = ntohl(dataPacket->fileID);
	//Find task
	//Get RcvTask back using file id!
	auto taskIt = writeDataTasks.find(fileID);
	if(taskIt == writeDataTasks.end()) {
	  LOG(ERROR)<<"No valid Task found. fileID:"<<fileID;
		return;
	}

  //Find file and lock it!
  FileNode* fNode = FileSystem::getInstance().findAndOpenNode(taskIt->second.remoteFile);
  if(fNode == nullptr) {
    LOG(ERROR)<<"File Not found:"<<taskIt->second.remoteFile;
    return;
  }
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
  fNode->close(inodeNum);//this file has been already opened

  //Write data to file
  FileNode* afterMove = nullptr;
  long result = fNode->writeHandler(dataPacket->data,offset,size,afterMove);

  if(afterMove)
    fNode = afterMove;
  if(result == -1) // -1 Moving
    return onWriteDataPacket(_packet);

  if(result <= 0 ){ //send a NACK
    taskIt->second.failed = true;
    LOG(ERROR)<<"write failed NOT ENOUGH MEMORY:"<< taskIt->second.remoteFile<<" MEMUTILIZATION:"<<MemoryContorller::getInstance().getMemoryUtilization();
  }

  if((unsigned long)result != size){ //send a NACK
    taskIt->second.failed = true;
    LOG(ERROR)<<"write failed size:"<<size<<" written:"<<result<<" file:"<<taskIt->second.remoteFile;
  }

	//Check if we received the write data completely
	if(offset+size  == taskIt->second.size+taskIt->second.offset) {
	  //LOG(ERROR)<<"Write FINISH, size:"<<size<<" FileID:"<<fileID<<" written:"<<result<<" file:"<<taskIt->second.remoteFile;
	  //set sync flag and Close file
	  fNode->setNeedSync(true);
	  fNode->close(taskIt->second.inodeNum);
		//Send ACK
		char buffer[MTU];
		WriteDataPacket *writeAck = (WriteDataPacket *)buffer;
		fillBFSHeader((char*)writeAck,taskIt->second.requestorMac);
		writeAck->opCode = htonl((uint32_t)BFS_OPERATION::WRITE_ACK);
		//set fileID
		writeAck->fileID = htonl(fileID);
		//Set size, indicating success or failure
		if(taskIt->second.failed){
		  LOG(ERROR)<<"Write failed, size:"<<size<<" FileID:"<<fileID<<" written:"<<result<<" file:"<<taskIt->second.remoteFile;
		  writeAck->size = 0;
		}
		else
		  writeAck->size = htobe64(taskIt->second.size);
		//Send it overNetwork
		int retry = 3;
		while(retry > 0) {
			if(send(buffer,MTU) == MTU)
				break;
			retry--;
		}
		if(!retry)
		  LOG(ERROR)<<"Failed to send ACKPacket,fileID:"<<fileID;

		//printf("SentACK\n");
		writeDataTasks.erase(taskIt);
		return;
	}
}



void FUSESwift::BFSNetwork::onWriteReqPacket(const u_char* _packet) {
	WriteReqPacket *reqPacket = (WriteReqPacket*)_packet;

	//Create a writeDataTask
	WriteDataTask writeTask;
	writeTask.fileID = ntohl(reqPacket->fileID);
	writeTask.offset = be64toh(reqPacket->offset);
	writeTask.size = be64toh(reqPacket->size);
	writeTask.failed = false;
	memcpy(writeTask.requestorMac,reqPacket->srcMac,6);
	writeTask.remoteFile = string(reqPacket->fileName);

	//LOG(ERROR)<<"GOT write Request:"<<writeTask.remoteFile<<" FileID:"<<writeTask.fileID<<" size:"<<writeTask.size;

  //Find file and lock it!
  FileNode* fNode = FileSystem::getInstance().findAndOpenNode(writeTask.remoteFile);
  if(fNode == nullptr)
    LOG(ERROR)<<"File Not found:"<<writeTask.remoteFile;
  else {
    //file is open and Will be closed when the write operation is finished.
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
    writeTask.inodeNum = inodeNum;
    //Put it on writeDataTask!
    auto resPair = writeDataTasks.insert(writeTask.fileID,writeTask);
    if(resPair.second){
      return;
    }
    else{
      LOG(ERROR)<<"error in inserting task to writeRcvTasks!size:"<<writeDataTasks.size();
      fNode->close(inodeNum);
    }

  }

  LOG(ERROR)<<"Write Request failed, Cannot find file:"<<writeTask.remoteFile<<" FileID:"<<writeTask.fileID<<" size:"<<writeTask.size;
  //fNode->close();//Close file
  //FAILED! send NACK
  //Send ACK
  char buffer[MTU];
  WriteDataPacket *writeAck = (WriteDataPacket *)buffer;
  fillBFSHeader((char*)writeAck,writeTask.requestorMac);
  writeAck->opCode = htonl((uint32_t)BFS_OPERATION::WRITE_ACK);
  //set fileID
  writeAck->fileID = htonl(writeTask.fileID);
  //Set size, indicating success or failure
  writeAck->size = 0;
  int retry = 3;
  while(retry) {
    //Now send packet on the wire
    if(!ZeroNetwork::send(buffer,MTU))
      LOG(ERROR)<<"Failed to send writeNACKPacket.";
    else
      break;
  }
}

void FUSESwift::BFSNetwork::onWriteAckPacket(const u_char* _packet) {

	WriteDataPacket *reqPacket = (WriteDataPacket*)_packet;
	//Find write sendTask
	uint32_t fileID = ntohl(reqPacket->fileID);
	uint64_t size = be64toh(reqPacket->size);

  uint64_t queueSize = sendQueue.size();
	for(uint64_t i=0; i<queueSize;i++) {
		SndTask *task = sendQueue.at(i);
		if(task->type == SEND_TASK_TYPE::SEND_WRITE)
			if(((WriteSndTask*)task)->fileID == fileID){
				WriteSndTask* writeTask = (WriteSndTask*)task;
				//Signal
				unique_lock<mutex> lk(writeTask->ack_m);
				if(writeTask->size == size)
				  writeTask->acked = true;
				else{
				  LOG(ERROR)<<"Remote Write failed, expected:"<<writeTask->size<<" but got:"<<size<<".";
				  writeTask->acked = false;
				}
				writeTask->ack_ready = true;
				writeTask->ack_cv.notify_one();
				return;
			}
	}

	//Error
	LOG(ERROR)<<"got ack for:"<<fileID<<" but no task found!";
}

uint32_t BFSNetwork::getNextFileID() {
  if(fileIDCounter>4294967290)
    LOG(ERROR)<<"\n\n\n\n\n\n\n\n\nFILE ID GONE WITH THE FUCK\n\n\n\n\n\n\n";
  return fileIDCounter++;
}

const unsigned char* BFSNetwork::getMAC() {
  if(!macInitialized){
    getMacAndMTU(DEVICE,MAC,MTU);
    macInitialized.store(true);
  }
	return MAC;
}

bool BFSNetwork::readRemoteFileAttrib(struct packed_stat_info *attBuff,
		const string &remoteFile, const unsigned char _dstMAC[6]) {
	//Create a new task
	ReadRcvTask task;
	task.dstBuffer = attBuff;
	task.fileID = getNextFileID();
	task.remoteFile = remoteFile;
	task.ready = false;
	char buffer[MTU];
	ReadReqPacket *reqPacket = (ReadReqPacket*)buffer;
	fillReadReqPacket(reqPacket,_dstMAC,task,task.fileID);
	reqPacket->opCode = htonl((uint32_t)BFS_OPERATION::ATTRIB_REQUEST);
	//Put it on the rcvTask map!
	auto resPair = attribRcvTasks.insert(task.fileID,&task);
	if(!resPair.second) {
	  LOG(ERROR)<<"readReamoteAttrib insert to attribRcvTasks failed";
		return false;
	}

	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU)) {
	  LOG(ERROR)<<"Failed to send attribReqpacket.";
		return false;
	}

	Timer t;
	unique_lock<std::mutex> lk(task.m);
	t.begin();
	//Now wait for it!
	while(!task.ready) {
		task.cv.wait_for(lk,chrono::milliseconds(ACK_TIMEOUT));
		t.end();
		if (!task.ready && (t.elapsedMillis() > ACK_TIMEOUT))
			break;
	}

	if(!task.ready)
	  LOG(ERROR)<<"readRemoteFileAttrib() Timeout: fileID:"<<task.fileID
	    <<"elapsedTime:",t.elapsedMillis();

	lk.unlock();
	//remove it from queue
	attribRcvTasks.erase(resPair.first);
	if(task.ready && task.offset == 1)//success
		return true;
	else
		return false;
}

void BFSNetwork::onAttribReqPacket(const u_char *_packet) {
	ReadReqPacket *reqPacket = (ReadReqPacket*)_packet;

	//create a send packet
	char buffer[MTU];
	ReadResPacket *attribResPacket = (ReadResPacket*)buffer;

	string fileName(reqPacket->fileName);
	FileNode* fNode = FileSystem::getInstance().findAndOpenNode(fileName);
	if(fNode!=nullptr) {
	  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
		//Offset is irrelevant here and we use it for indicating success for failure
		attribResPacket->offset = be64toh(1);
		struct packed_stat_info* packedStatInfoPtr = reinterpret_cast<struct packed_stat_info*>(attribResPacket->data);
		fNode->fillPackedStat(    *(packedStatInfoPtr)      );
		fNode->close(inodeNum);
	}
	else{
		attribResPacket->offset = be64toh(0);//zero is zero anyway...
		LOG(ERROR)<<"Attrib Req Failed:"<<fileName;
	}
	//Fill header
	fillBFSHeader((char*)attribResPacket,reqPacket->srcMac);
	attribResPacket->opCode = htonl((uint32_t)BFS_OPERATION::ATTRIB_RESPONSE);
	attribResPacket->fileID = htonl(ntohl(reqPacket->fileID));//This is actually redundant

	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU))
	  LOG(ERROR)<<"Failed to send attribRespacket.";
}

void BFSNetwork::onAttribResPacket(const u_char *_packet) {
	ReadResPacket *attribresPacket = (ReadResPacket*)_packet;
	//Parse Packet Network order first!
	attribresPacket->offset = be64toh(attribresPacket->offset);
	//attribresPacket->size = be64toh(attribresPacket->size);//Irrelevent here
	attribresPacket->fileID = ntohl(attribresPacket->fileID);
	//Get RcvTask back using file id!
	auto taskIt = attribRcvTasks.find(attribresPacket->fileID);
	attribRcvTasks.lock();
	if(taskIt == attribRcvTasks.end()) {
	  LOG(ERROR)<<"No valid Task for this Packet. FileID:"<<attribresPacket->fileID;
		attribRcvTasks.unlock();
		return;
	}

	//Exist! so fill task buffer
	ReadRcvTask* task = taskIt->second;
	if(task == nullptr||task->fileID!=attribresPacket->fileID)
	  LOG(ERROR)<<"CRAPPPPPPPPP PacketFileID:"<<attribresPacket->fileID <<
	    " taskFileID:"<<task->fileID;
	if(attribresPacket->offset == 1) {//Success
		task->offset = 1;//Indicate success
		memcpy((char*)task->dstBuffer,attribresPacket->data,sizeof(struct packed_stat_info));
	}
	else
		task->offset = 0;//Indicate Failure

	unique_lock<std::mutex> lk(task->m);
	task->ready = true;
	lk.unlock();
	task->cv.notify_one();
	attribRcvTasks.unlock();
}

void BFSNetwork::onDeleteReqPacket(const u_char* _packet) {
  WriteReqPacket *reqPacket = (WriteReqPacket*)_packet;
  //Local file
  string fileName = string(reqPacket->fileName);
  uint32_t fileID = ntohl(reqPacket->fileID);

  //Response Packet
  char buffer[MTU];
  WriteDataPacket *deleteAck = (WriteDataPacket *)buffer;
  fillBFSHeader((char*)deleteAck,reqPacket->srcMac);
  deleteAck->opCode = htonl((uint32_t)BFS_OPERATION::DELETE_RESPONSE);
  //set fileID
  deleteAck->fileID = htonl(fileID);

  //Size == 0 means failed! greater than 0 means how many were removed.
  deleteAck->size = 0;//zero is zero anyway


  //Find file and lock it!
  FileNode* fNode = FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode == nullptr) {
    //Now send packet on the wire
    if(!ZeroNetwork::send(buffer,MTU)) {
      LOG(ERROR)<<"Failed to send deleteAckpacket.";
    }
    LOG(ERROR)<<"File Not found:"<<fileName;
    return;
  }
  //Assign inode and close it
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
  LOG(DEBUG)<<"SIGNAL DELETE FROM BFSNetwork:"<<fNode->getFullPath();
  fNode->close(inodeNum);
  if(FileSystem::getInstance().signalDeleteNode(fNode,true)){
    deleteAck->size = htobe64(1);
  }
  //Send the ack on the wires
  if(!ZeroNetwork::send(buffer,MTU)) {
    LOG(ERROR)<<"Failed to send deleteAckpacket.";
  }
}

void BFSNetwork::onDeleteResPacket(const u_char* _packet) {
  WriteDataPacket *reqPacket = (WriteDataPacket*)_packet;
  //Find write sendTask
  uint32_t fileID = ntohl(reqPacket->fileID);
  uint64_t size = be64toh(reqPacket->size);

  auto taskIt = deleteSendTasks.find(fileID);
  deleteSendTasks.lock();
  if(taskIt == deleteSendTasks.end()) {
    LOG(ERROR)<<"No valid Task found. fileID:"<<fileID;
    deleteSendTasks.unlock();
    return;
  }
  WriteSndTask* deleteTask = (WriteSndTask*)taskIt->second;

  //Signal
  unique_lock<mutex> lk(deleteTask->ack_m);
  if(size > 0)
    deleteTask->acked = true;
  else
    deleteTask->acked = false;

  deleteTask->ack_ready = true;
  lk.unlock();
  deleteTask->ack_cv.notify_one();

  deleteSendTasks.unlock();
}

bool BFSNetwork::deleteRemoteFile(const std::string& _remoteFile,
    const unsigned char _dstMAC[6]) {

  if(_remoteFile.length() > DATA_LENGTH){
    LOG(ERROR)<<"Filename too long!";
    return false;
  }

  //Create a new task
  WriteSndTask task;
  task.srcBuffer = nullptr;
  task.size = 0;
  task.offset = 0;
  task.fileID = getNextFileID();
  task.remoteFile = _remoteFile;
  task.acked = false;
  memcpy(task.dstMac,_dstMAC,6);
  task.ready = false;
  task.ack_ready = false;



  //First Send a write request
  char buffer[MTU];
  WriteReqPacket *deleteReqPkt = (WriteReqPacket*)buffer;
  fillWriteReqPacket(deleteReqPkt,task.dstMac,task);
  deleteReqPkt->opCode = htonl((uint32_t)BFS_OPERATION::DELETE_REQUEST);

  //Put it on the deleteSendTask map!
  auto resPair = deleteSendTasks.insert(task.fileID,&task);
  if(!resPair.second) {
    LOG(ERROR)<<"error in inserting task to deleteSendTas! size:"<<deleteSendTasks.size();
    return false;
  }

  //Now send packet on the wire
  if(!ZeroNetwork::send(buffer,MTU)) {
    LOG(ERROR)<<"Failed to send deleteReqpacket.";
    deleteSendTasks.erase(resPair.first);
    return false;
  }

  Timer t;
  unique_lock<mutex> ack_lk(task.ack_m);
  t.begin();
  //Now wait for the ACK!
  while(!task.ack_ready) {
    task.ack_cv.wait_for(ack_lk,chrono::milliseconds(ACK_TIMEOUT));
    t.end();

    if(!task.ack_ready && (t.elapsedMillis() >= ACK_TIMEOUT))
      break;
  }

  if(!task.ack_ready){
    //printf("DeleteRequest Timeout: fileID%d ElapsedMILLIS:%f\n",task.fileID,t.elapsedMillis());
  }
  if(!task.acked && task.size == 0)
    LOG(ERROR)<<"DeleteRequest failed: "<<task.remoteFile;

  ack_lk.unlock();
  deleteSendTasks.erase(resPair.first);
  return task.acked;
}

void BFSNetwork::onTruncateReqPacket(const u_char* _packet) {
  WriteReqPacket *reqPacket = (WriteReqPacket*)_packet;
  //Local file
  string fileName = string(reqPacket->fileName);
  uint32_t fileID = ntohl(reqPacket->fileID);
  uint64_t newSize = be64toh(reqPacket->size);

  //Response Packet
  char buffer[MTU];
  WriteDataPacket *truncateAck = (WriteDataPacket *)buffer;
  fillBFSHeader((char*)truncateAck,reqPacket->srcMac);
  truncateAck->opCode = htonl((uint32_t)BFS_OPERATION::TRUNCATE_RESPONSE);
  //set fileID
  truncateAck->fileID = htonl(fileID);
  //Size == 0 means failed! if truncate successful size will be newSize
  truncateAck->size = 0;//zero is zero anyway


  //Find file and lock it!
  FileNode* fNode = FileSystem::getInstance().findAndOpenNode(fileName);
  if(fNode == nullptr) {
    //Now send packet on the wire
    if(!ZeroNetwork::send(buffer,MTU)) {
      LOG(ERROR)<<"Failed to send truncateAckpacket.";
    }
    LOG(ERROR)<<" File Not found:"<<fileName;
    return;
  }
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);

  LOG(ERROR)<<"AVAILBLE MEMORY BEF: "<<MemoryContorller::getInstance().getAvailableMemory();

  bool res = fNode->truncate(newSize);
  fNode->close(inodeNum);

  if(res)//Success
    truncateAck->size = htobe64(newSize);
  LOG(ERROR)<<"AVAILBLE MEMORY AFT:"<<MemoryContorller::getInstance().getAvailableMemory();
  //Send the ack on the wires
  if(!ZeroNetwork::send(buffer,MTU)) {
    LOG(ERROR)<<"Failed to send truncateAckpacket.";
  }
}

void BFSNetwork::onTruncateResPacket(const u_char* _packet) {
  WriteDataPacket *resPacket = (WriteDataPacket*)_packet;
  //Find write sendTask
  uint32_t fileID = ntohl(resPacket->fileID);
  uint64_t size = be64toh(resPacket->size);

  auto taskIt = truncateSendTasks.find(fileID);
  truncateSendTasks.lock();
  if(taskIt == truncateSendTasks.end()) {
    LOG(ERROR)<<"No valid Task found. fileID:"<<fileID;
    truncateSendTasks.unlock();
    return;
  }
  WriteSndTask* truncateTask = (WriteSndTask*)taskIt->second;

  //Signal
  unique_lock<mutex> lk(truncateTask->ack_m);
  if(size == truncateTask->size)
    truncateTask->acked = true;
  else
    truncateTask->acked = false;

  truncateTask->ack_ready = true;
  lk.unlock();
  truncateTask->ack_cv.notify_one();

  truncateSendTasks.unlock();
}

bool BFSNetwork::truncateRemoteFile(const std::string& _remoteFile,
    size_t _newSize, const unsigned char _dstMAC[6]) {
  if (_remoteFile.length() > DATA_LENGTH) {
    LOG(ERROR)<<"Filename too long!";
    return false;
  }

  //Create a new task
  WriteSndTask task;
  task.srcBuffer = nullptr;
  task.size = _newSize;
  task.offset = 0;
  task.fileID = getNextFileID();
  task.remoteFile = _remoteFile;
  task.acked = false;
  memcpy(task.dstMac, _dstMAC, 6);
  task.ready = false;
  task.ack_ready = false;

  //First Send a write request
  char buffer[MTU];
  WriteReqPacket *truncateReqPkt = (WriteReqPacket*) buffer;
  fillWriteReqPacket(truncateReqPkt, task.dstMac, task);
  truncateReqPkt->opCode = htonl((uint32_t) BFS_OPERATION::TRUNCATE_REQUEST);

  //Put it on the truncateSendTask map!
  auto resPair = truncateSendTasks.insert(task.fileID, &task);
  if (!resPair.second) {
    LOG(ERROR)<<" error in inserting task to truncateSendTask! size:"<<
        truncateSendTasks.size();
    return false;
  }

  //Now send packet on the wire
  if (!ZeroNetwork::send(buffer, MTU)) {
    LOG(ERROR)<<"Failed to send truncateReqpacket.";
    truncateSendTasks.erase(resPair.first);
    return false;
  }

  //Truncate can be a lengthy operation
  uint64_t timeout = ACK_TIMEOUT * 10;

  Timer t;
  unique_lock < mutex > ack_lk(task.ack_m);
  t.begin();
  //Now wait for the ACK!
  while (!task.ack_ready) {
    task.ack_cv.wait_for(ack_lk, chrono::milliseconds(timeout));
    t.end();
    if (!task.ack_ready && (t.elapsedMillis() >= timeout))
      break;
  }

  if (!task.ack_ready)
    LOG(ERROR)<<"TruncateRequest Timeout: fileID:"<<task.fileID
      <<" ElapsedTimeMILLIS:"<<t.elapsedMillis();
  if (!task.acked && task.size != _newSize)
    LOG(ERROR)<<"TruncateRequest failed: "<< task.remoteFile.c_str();

  ack_lk.unlock();
  truncateSendTasks.erase(resPair.first);
  return task.acked;
}

void BFSNetwork::onCreateReqPacket(const u_char* _packet) {
  WriteReqPacket *reqPacket = (WriteReqPacket*)_packet;
  //Local file to be created
  string fileName = string(reqPacket->fileName);
  uint32_t fileID = ntohl(reqPacket->fileID);
  //uint64_t size = be64toh(reqPacket->size);

  //Response Packet
  char buffer[MTU];
  WriteDataPacket *createAck = (WriteDataPacket *)buffer;
  fillBFSHeader((char*)createAck,reqPacket->srcMac);
  createAck->opCode = htonl((uint32_t)BFS_OPERATION::CREATE_RESPONSE);
  //set fileID
  createAck->fileID = htonl(fileID);
  //Size == 0 means failed! if truncate successful size will be newSize
  createAck->size = 0;//zero is zero anyway

  LOG(ERROR)<<"CREATE REQ PACKET:"<<fileName;

  bool res = false;

  //First update your view of work then decide!
  ZooHandler::getInstance().requestUpdateGlobalView();

  //Manipulate file
  FileNode* existing = FileSystem::getInstance().findAndOpenNode(fileName);
  if(existing) {
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)existing);
    if(existing->isRemote()) {//I'm not responsible! I should not have seen this.
      //Close it!
      existing->close(inodeNum);
      LOG(ERROR)<<"Going to move file:"<<fileName<<" to here!";

      //Handle Move file to here!
      MoveTask *mvTask = new MoveTask();
      mvTask->fileName = fileName;
      mvTask->fileID = fileID;
      mvTask->acked = false;
      mvTask->ready = false;
      memcpy(mvTask->requestorMac,existing->getRemoteHostMAC(),6);
      moveQueue.push(mvTask);
      return;//The response will be sent later
    } else { //overwrite file=>truncate to 0
      LOG(ERROR)<<"EXIST and LOCAL, Going to truncate file:"<<fileName<<" here.";

      res = existing->truncate(0);
      existing->close(inodeNum);
    }
  } else {

    LOG(ERROR)<<"DOES NOT EXIST, Going to create file:"<<fileName<<" here.";
    res = (FileSystem::getInstance().mkFile(fileName,false,false)!=nullptr)?true:false;
  }

  if(res)//Success
    createAck->size = htobe64(1);

  //Send the ack on the wire
  if(!ZeroNetwork::send(buffer,MTU)) {
    LOG(ERROR)<<"Failed to send createAckpacket.";
  }
}

void BFSNetwork::onCreateResPacket(const u_char* _packet) {
  WriteDataPacket *resPacket = (WriteDataPacket*)_packet;
  //Find write sendTask
  uint32_t fileID = ntohl(resPacket->fileID);
  uint64_t size = be64toh(resPacket->size);

  auto taskIt = createSendTasks.find(fileID);
  createSendTasks.lock();
  if(taskIt == createSendTasks.end()) {
    LOG(ERROR)<<"No valid Task found. fileID:"<<fileID;
    createSendTasks.unlock();
    return;
  }
  WriteSndTask* createTask = (WriteSndTask*) taskIt->second;

  //Signal
  unique_lock<mutex> lk(createTask->ack_m);
  if(size == 0) //0 indicates failure
    createTask->acked = false;
  else
    createTask->acked = true;

  createTask->ack_ready = true;
  lk.unlock();
  createTask->ack_cv.notify_one();

  createSendTasks.unlock();
}

bool BFSNetwork::createRemoteFile(const std::string& _remoteFile,
    const unsigned char _dstMAC[6]) {
  if (_remoteFile.length() > DATA_LENGTH) {
    LOG(ERROR)<<"Filename too long:"<<_remoteFile;
    return false;
  }
  LOG(ERROR)<<"CreateRemote FILE:"<<_remoteFile;
  //Create a new task
  WriteSndTask task;
  task.srcBuffer = nullptr;
  task.size = 0;
  task.offset = 0;
  task.fileID = getNextFileID();
  task.remoteFile = _remoteFile;
  task.acked = false;
  memcpy(task.dstMac, _dstMAC, 6);
  task.ready = false;
  task.ack_ready = false;

  //First Send a write request
  char buffer[MTU];
  WriteReqPacket *createReqPkt = (WriteReqPacket*) buffer;
  fillWriteReqPacket(createReqPkt, task.dstMac, task);
  createReqPkt->opCode = htonl((uint32_t) BFS_OPERATION::CREATE_REQUEST);

  //Put it on the createSendTask map!
  auto resPair = createSendTasks.insert(task.fileID, &task);
  if (!resPair.second) {
    LOG(ERROR)<<
        "error in inserting task to createSendTask! size:"<<
        createSendTasks.size();
    return false;
  }

  //Now send packet on the wire
  if (!ZeroNetwork::send(buffer, MTU)) {
    LOG(ERROR)<<"Failed to send createReqpacket.";
    createSendTasks.erase(resPair.first);
    return false;
  }

  //Try to figure out if this is going to be a new file remotely or if it's a
  //truncate or a move operation!
  FileNode *fNode = FileSystem::getInstance().findAndOpenNode(_remoteFile);
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fNode);
  if(fNode!=nullptr)
  	fNode->close(inodeNum);//Close it! so it can be removed if needed
  if(fNode != nullptr && !fNode->isRemote()) { //This is a move operation
    unique_lock < mutex > ack_lk(task.ack_m);
    while (!task.ack_ready) {
      task.ack_cv.wait(ack_lk);
    }

    if (!task.acked) {
      LOG(ERROR)<<"Move failed: "<<task.remoteFile;
    } else {//Successful move
      LOG(ERROR)<<"Move to remote node successful: "<<task.remoteFile<< " Updating global view";
      ZooHandler::getInstance().requestUpdateGlobalView();
    }

    ack_lk.unlock();
    createSendTasks.erase(resPair.first);

    return task.acked;
  } else { //Truncate or new remote file
    Timer t;
    //Now wait for the ack!
    unique_lock < mutex > ack_lk(task.ack_m);
    t.begin();
    while (!task.ack_ready) {
      task.ack_cv.wait_for(ack_lk, chrono::milliseconds(ACK_TIMEOUT));
      t.end();
      if (!task.ack_ready && (t.elapsedMillis() >= ACK_TIMEOUT))
        break;
    }

    if (!task.ack_ready)
      LOG(ERROR)<<"CreateRequest Timeout: fileID:"<<task.fileID<<
        " ElapsedTimeMILLI:"<<t.elapsedMillis();
    if (!task.acked && task.size > 0)//0 indicates failed operation
      LOG(ERROR)<<"CreateRequest failed: "<<task.remoteFile;

    ack_lk.unlock();
    createSendTasks.erase(resPair.first);
    return task.acked;
  }
}

} /* namespace FUSESwift */

