/*
 * BFSNetwork.cpp
 *
 *  Created on: Oct 8, 2014
 *      Author: behrooz
 */

#include "ZeroNetwork.h"
#include "BFSNetwork.h"
#include "ZooHandler.h"
#include "filesystem.h"
#include "filenode.h"
#include "SettingManager.h"
#include "MemoryController.h"
#include "Timer.h"
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <arpa/inet.h>
extern "C" {
	#include <pfring.h>
}


namespace FUSESwift {

using namespace std;

/** static members **/
string BFSNetwork::DEVICE= "eth0";
unsigned char BFSNetwork::MAC[6];
taskMap<uint32_t,ReadRcvTask*> BFSNetwork::readRcvTasks(2000);
taskMap<uint32_t,WriteDataTask> BFSNetwork::writeDataTasks(2000);
taskMap<uint32_t,ReadRcvTask*> BFSNetwork::attribRcvTasks(2000);
taskMap<uint32_t,WriteSndTask*> BFSNetwork::deleteSendTasks(2000);
taskMap<uint32_t,WriteSndTask*> BFSNetwork::truncateSendTasks(2000);
taskMap<uint32_t,WriteSndTask*> BFSNetwork::createSendTasks(2000);
taskMap<uint32_t,MoveConfirmTask*> BFSNetwork::moveConfirmSendTasks(1000);
atomic<bool> BFSNetwork::isRunning(true);
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
		fprintf(stderr,"No device specified in the config file!\n");
	//Get Mac Address
	int mtu = -1;
	getMacAndMTU(DEVICE,MAC,mtu);
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
		fprintf(stderr,"Filename too long!\n");
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
		fprintf(stderr,"error in inserting task to the rcvQueue!,"
				"Cannot handle more than uint32_t.MAX concurrent reads\n");
		result = -1;
		return result;
	}

	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU)) {
		fprintf(stderr,"Failed to send readReqpacket.\n");
		result = -1;
		readRcvTasks.erase(resPair.first);
		return result;
	}


	//Now wait for it!
  unique_lock<std::mutex> lk(task.m);
	while(!task.ready) {
		task.cv.wait(lk);
		if (!task.ready) {
			fprintf(stderr,"HOLY SHIT! Spurious wake up!\n");
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
}

void FUSESwift::BFSNetwork::processReadSendTask(ReadSndTask& _task) {
	uint64_t total = _task.size;
	uint64_t left = _task.size;

	//Find file and lock it!
	FileNode* fNode = FileSystem::getInstance().getNode(_task.localFile);
	if(fNode == nullptr || _task.size == 0|| fNode->isRemote()) { //error just send a packet of size 0
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

		if(fNode->isRemote()){
		  fprintf(stderr,"processReadSendTask(), Request to read a "
		      "remote file(%s) from me!\n",_task.localFile.c_str());
		  fflush(stderr);
		}

		return;
	}
	uint64_t localOffset = 0;
	//Now we have the file node!
	//1)Open the file
	fNode->open();
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
  		fprintf(stderr, "Failed to send packet through ZeroNetwork:%s.\n",
  				_task.localFile.c_str());
  		//fclose(fd);
  		return;
  	}
  	//Increment info
  	_task.offset += howMuch;
  	localOffset += howMuch;
  	left -= howMuch;
  }
//  static atomic<uint64_t> counter = 0;
//  fprintf(stderr," Counter:%lu\n",++counter);
  //fprintf(stderr,"PROCESSED READ REQ:%s offset:%lu size:%lu\n",_task.localFile.c_str(),_task.offset,total);
  //close file
  fNode->close();
  _task.size = total;

}

void BFSNetwork::moveLoop() {
  while(isRunning) {
    MoveTask* front = moveQueue.front();
    processMoveTask(*front);
    delete front;
    front = nullptr;
    moveQueue.pop();
  }
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
  FileNode* file = FileSystem::getInstance().getNode(_moveTask.fileName);
  if(file != nullptr) {
    //2) Open file
    file->open();
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
          if(read != file->write(buffer,offset,read))//error in writing
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
            fprintf(stderr,"processMoveTask(): Failed to delete remote file:%s.\n",_moveTask.fileName.c_str());
            fflush(stderr);
            file->makeRemote();
            file->deallocate();//Release memory allocated to the file
          }
        } else {
          fprintf(stderr,"processMoveTask():reading remote File/writing to local one failed:%s\n",_moveTask.fileName.c_str());
          fflush(stderr);
        }
      }
    } else {
      fprintf(stderr,"processMoveTask():Get Remote File Stat FAILED:%s\n",_moveTask.fileName.c_str());
      fflush(stderr);
    }

    file->close();
  } else {
    fprintf(stderr,"processMoveTask():Cannot find fileNode:%s\n",_moveTask.fileName.c_str());
    fflush(stderr);
  }


  if(res)//Success
    moveAck->size = htobe64(1);
  else
    moveAck->size = 0;

  //Send the ack on the wire
  if(!ZeroNetwork::send(buffer,MTU)) {
    fprintf(stderr,"Failed to send createAckpacket.\n");
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
			//fprintf(stderr,"Error in pfring_recv:%d\n",res);
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
			fprintf(stderr,"Fragmentation! dropping. CapLen:%u Len:%u\n",
					_header.caplen,_header.len);
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


		/** Seems we got a relative packet! **/
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
				fprintf(stderr,"UNKNOWN OPCODE:%ul\n",opCode);
		}
	}
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
    fprintf(stderr,"\n\n\n\n\n\n\n\n\n\ndrop happened:%d\n\n\n\n\n\n\n\n\n\n\n",counter);
    fprintf(stderr,"coutner:%d",counter);
    fflush(stderr);
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
		//fprintf(stderr,"onReadResPacket():No valid Task for this Packet. FileID:%u\n",resPacket->fileID);
		//fflush(stderr);
	  readRcvTasks.unlock();
		return;
	}

	//Exist! so fill task buffer
	ReadRcvTask* task = taskIt->second;
	if(resPacket->size) {
	  uint64_t delta = resPacket->offset - task->offset;
	  if(taskIt->first != task->fileID|| taskIt->first!= resPacket->fileID|| task->fileID!= resPacket->fileID)
	    fprintf(stderr,"Key:%u elementID:%u  packetID:%u  second:%p\n",taskIt->first,task->fileID,resPacket->fileID,task);

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
		fprintf(stderr,"Filename too long!\n");
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
		fprintf(stderr,"Failed to send writeReqpacket.\n");
		return;
	}

	//Now send write_data
	uint64_t left = _task.size;
  while(left) {
  	unsigned long howMuch = (left > DATA_LENGTH)?DATA_LENGTH:left;
  	//Make a response packet
  	_task.size = howMuch;
  	char buffer[MTU];
  	//memset(buffer,0,MTU);
  	WriteDataPacket *packet = (WriteDataPacket*)buffer;
  	fillWriteDataPacket(packet,_task);
  	memcpy(packet->data,(unsigned char*)_task.srcBuffer+(total-left),_task.size);
  	//Now send it
  	int retry = 3;
  	while(retry > 0) {
  		if(send(buffer,MTU) == MTU)
  			break;
  		retry--;
  	}
  	if(!retry) {
  		fprintf(stderr, "(processWriteSendTask)Failed to send packet "
  				"through ZeroNetwork:%s.\n",
  				_task.remoteFile.c_str());
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
		printf("WriteRequest Timeout: fileID:%d ElapsedMILLIS:%f\n",_task.fileID,t.elapsedMillis());

	if(!_task.acked && _task.ack_ready)
    printf("WriteRequest failed:%s\n",_task.remoteFile.c_str());
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
    const WriteSndTask& _writeTask) {
	//First fill header
	fillBFSHeader((char*)_packet,_writeTask.dstMac);
	_packet->opCode = htonl((uint32_t)BFS_OPERATION::WRITE_DATA);
	/** Now fill request packet fields **/
	//File id
	_packet->fileID = htonl(_writeTask.fileID);
	// Offset
	_packet->offset = htobe64(_writeTask.offset);
	// size
	_packet->size = htobe64(_writeTask.size);
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
		fprintf(stderr,"onWriteDataPacket:No valid Task found. fileID:%d\n",fileID);
		return;
	}

  //Find file and lock it!
  FileNode* fNode = FileSystem::getInstance().getNode(taskIt->second.remoteFile);
  if(fNode == nullptr) {
    fprintf(stderr,"onWriteDataPacket(), File Not found:%s!\n",taskIt->second.remoteFile.c_str());
    return;
  }
  //Write data to file
	//memcpy(testWriteBuffer+offset,dataPacket->data,size);
  long result = fNode->write(dataPacket->data,offset,size);
  if(result <= 0 || (unsigned long)result != size){ //send a NACK
    taskIt->second.failed = true;
    fprintf(stderr,"onWriteDataPacket(), write failed:%s!\n",taskIt->second.remoteFile.c_str());
  }


	//printf("Packetoffset:%lu  , Packetsize:%lu , packetSum:%lu , taskOffset:%lu , taskSize:%lu , taskSum:%lu\n",offset,size,offset+size,taskIt->second.offset,taskIt->second.size,taskIt->second.offset+taskIt->second.size);
	//Check if we received the write data completely
	if(offset+size  == taskIt->second.size+taskIt->second.offset) {
	  //set sync flag and Close file
	  fNode->setNeedSync(true);
	  fNode->close();
		//Send ACK
		char buffer[MTU];
		WriteDataPacket *writeAck = (WriteDataPacket *)buffer;
		fillBFSHeader((char*)writeAck,taskIt->second.requestorMac);
		writeAck->opCode = htonl((uint32_t)BFS_OPERATION::WRITE_ACK);
		//set fileID
		writeAck->fileID = htonl(fileID);
		//Set size, indicating success or failure
		if(taskIt->second.failed)
		  writeAck->size = 0;
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
			fprintf(stderr,"(onWriteDataPacket)Failed to send ACKPacket,fileID:%d\n",fileID);

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

  //Find file and lock it!
  FileNode* fNode = FileSystem::getInstance().getNode(writeTask.remoteFile);
  if(fNode == nullptr)
    fprintf(stderr,"onWriteDataPacket(), File Not found:%s!\n",writeTask.remoteFile.c_str());
  else {
    //Open file
    fNode->open();//Will be closed when the write operation is finished.

    //Put it on the rcvTask map!
    auto resPair = writeDataTasks.insert(writeTask.fileID,writeTask);
    if(resPair.second){
      return;
    }
    else
      fprintf(stderr,"onWriteReqPacket(), error in inserting task to writeRcvTasks!size:%lu\n",writeDataTasks.size());
  }

  fNode->close();//Close file
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
      fprintf(stderr,"Failed to send writeNACKPacket.\n");
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
				else
				  writeTask->acked = false;
				writeTask->ack_ready = true;
				writeTask->ack_cv.notify_one();
				return;
			}
	}

	//Error
	printf("onWriteAckPacket():got ack for:%u but no task found!\n",fileID);
	fflush(stderr);
}

uint32_t BFSNetwork::getNextFileID() {
  if(fileIDCounter>4294967290)
    fprintf(stderr,"\n\n\n\n\n\n\n\n\nFILE ID GONE WITH THE FUCK\n\n\n\n\n\n\n");
  return fileIDCounter++;
}

const unsigned char* BFSNetwork::getMAC() {
	return MAC;
}

bool BFSNetwork::readRemoteFileAttrib(struct stat *attBuff,
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
		fprintf(stderr,"readReamoteAttrib insert to attribRcvTasks failed\n");
		return false;
	}

	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU)) {
		fprintf(stderr,"Failed to send attribReqpacket.\n");
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
	  fprintf(stderr,"readRemoteFileAttrib() Timeout: fileID:%d elapsedTime:%f\n",
	      task.fileID,t.elapsedMillis());

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
	FileNode* fNode = FileSystem::getInstance().getNode(fileName);
	if(fNode) {
		//Offset is irrelevant here and we use it for indicating success for failure
		attribResPacket->offset = be64toh(1);
		fNode->getStat((struct stat*)attribResPacket->data);
	}
	else{
		attribResPacket->offset = be64toh(0);//zero is zero anyway...
		fprintf(stderr,"Attrib Req Failed:%s\n",fileName.c_str());
	}
	//Fill header
	fillBFSHeader((char*)attribResPacket,reqPacket->srcMac);
	attribResPacket->opCode = htonl((uint32_t)BFS_OPERATION::ATTRIB_RESPONSE);
	attribResPacket->fileID = htonl(ntohl(reqPacket->fileID));//This is actually redundant

	//Now send packet on the wire
	if(!ZeroNetwork::send(buffer,MTU))
		fprintf(stderr,"onAttribRePacket():Failed to send attribRespacket.\n");
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
		fprintf(stderr,"onAttribResPacket():No valid Task for this Packet. FileID:%d\n",
				attribresPacket->fileID);
		attribRcvTasks.unlock();
		return;
	}

	//Exist! so fill task buffer
	ReadRcvTask* task = taskIt->second;
	if(task == nullptr||task->fileID!=attribresPacket->fileID)
	  fprintf(stderr,"CRAPPPPPPPPPPPPPP PacketFileID:%d taskFileID:%d\n",
	          attribresPacket->fileID,task->fileID);
	if(attribresPacket->offset == 1) {//Success
		task->offset = 1;//Indicate success
		memcpy((char*)task->dstBuffer,attribresPacket->data,sizeof(struct stat));
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
  FileNode* fNode = FileSystem::getInstance().getNode(fileName);
  if(fNode == nullptr) {
    //Now send packet on the wire
    if(!ZeroNetwork::send(buffer,MTU)) {
      fprintf(stderr,"Failed to send deleteAckpacket.\n");
    }
    fprintf(stderr,"onDeleteReqPacket(), File Not found:%s!\n",fileName.c_str());
    return;
  }

  if(fNode->signalDelete())
    deleteAck->size = htobe64(1);
  //Send the ack on the wires
  if(!ZeroNetwork::send(buffer,MTU)) {
    fprintf(stderr,"Failed to send deleteAckpacket.\n");
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
    fprintf(stderr,"onWriteDataPacket:No valid Task found. fileID:%d\n",fileID);
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
    fprintf(stderr,"Filename too long!\n");
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
    fprintf(stderr,"deleteRemoteFile(), error in inserting task to deleteSendTas! size:%lu\n",deleteSendTasks.size());
    return false;
  }

  //Now send packet on the wire
  if(!ZeroNetwork::send(buffer,MTU)) {
    fprintf(stderr,"Failed to send deleteReqpacket.\n");
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

  if(!task.ack_ready)
    printf("DeleteRequest Timeout: fileID%d ElapsedMILLIS:%f\n",task.fileID,t.elapsedMillis());
  if(!task.acked && task.size == 0)
    printf("DeleteRequest failed: %s\n",task.remoteFile.c_str());

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
  FileNode* fNode = FileSystem::getInstance().getNode(fileName);
  if(fNode == nullptr) {
    //Now send packet on the wire
    if(!ZeroNetwork::send(buffer,MTU)) {
      fprintf(stderr,"Failed to send truncateAckpacket.\n");
    }
    fprintf(stderr,"onTrucnateReqPacket(), File Not found:%s!\n",fileName.c_str());
    return;
  }

  fprintf(stderr,"AVAILBLE MEMORY BEF: %lld\n",(long long)MemoryContorller::getInstance().getAvailableMemory());
  fNode->open();
  bool res = fNode->truncate(newSize);
  fNode->close();

  if(res)//Success
    truncateAck->size = htobe64(newSize);
  fprintf(stderr,"AVAILBLE MEMORY AFT: %lld\n",(long long)MemoryContorller::getInstance().getAvailableMemory());
  //Send the ack on the wires
  if(!ZeroNetwork::send(buffer,MTU)) {
    fprintf(stderr,"Failed to send truncateAckpacket.\n");
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
    fprintf(stderr,"onTruncateResPacket:No valid Task found. fileID:%d\n",fileID);
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
    fprintf(stderr, "Filename too long!\n");
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
    fprintf(stderr,
        "truncateRemoteFile(), error in inserting task to truncateSendTask!size:%lu\n",
        truncateSendTasks.size());
    return false;
  }

  //Now send packet on the wire
  if (!ZeroNetwork::send(buffer, MTU)) {
    fprintf(stderr, "Failed to send truncateReqpacket.\n");
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
    printf("TruncateRequest Timeout: fileID:%d ElapsedTimeMILLIS:%f\n", task.fileID,
        t.elapsedMillis());
  if (!task.acked && task.size != _newSize)
    printf("TruncateRequest failed: %s\n", task.remoteFile.c_str());

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


  bool res = false;

  //Manipulate file
  FileNode* existing = FileSystem::getInstance().getNode(fileName);
  if(existing) {
    if(existing->isRemote()) {//I'm not responsible! I should not have seen this.
      fprintf(stderr,"onCreateReqPacket(): Going to move file:%s to here!\n",fileName.c_str());
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
      existing->open();
      res = existing->truncate(0);
      existing->close();
    }
  } else {
    res = (FileSystem::getInstance().mkFile(fileName,false)!=nullptr)?true:false;
  }

  if(res)//Success
    createAck->size = htobe64(1);

  //Send the ack on the wire
  if(!ZeroNetwork::send(buffer,MTU)) {
    fprintf(stderr,"Failed to send createAckpacket.\n");
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
    fprintf(stderr,"onCreateResPacket:No valid Task found. fileID:%d\n",fileID);
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
    fprintf(stderr, "Filename too long!\n");
    return false;
  }
  fprintf(stderr, "CreateRemote FILE!\n");
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
    fprintf(stderr,
        "createRemoteFile(), error in inserting task to createSendTask!size:%lu\n",
        createSendTasks.size());
    return false;
  }

  //Now send packet on the wire
  if (!ZeroNetwork::send(buffer, MTU)) {
    fprintf(stderr, "createRemoteFile(): Failed to send createReqpacket.\n");
    createSendTasks.erase(resPair.first);
    return false;
  }

  //Try to figure out if this is going to be a new file remotely or if it's a
  //truncate or a move operation!
  FileNode *fNode = FileSystem::getInstance().getNode(_remoteFile);
  if(fNode != nullptr && !fNode->isRemote()) { //This is a move operation
    unique_lock < mutex > ack_lk(task.ack_m);
    while (!task.ack_ready) {
      task.ack_cv.wait(ack_lk);
    }

    if (!task.acked) {
      fprintf(stderr,"Move failed: %s\n", task.remoteFile.c_str());
      fflush(stderr);
    } else {//Successful move
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
      printf("CreateRequest Timeout: fileID:%d ElapsedTimeMILLI:%f\n", task.fileID,t.elapsedMillis());
    if (!task.acked && task.size > 0)//0 indicates failed operation
      printf("CreateRequest failed: %s\n", task.remoteFile.c_str());

    ack_lk.unlock();
    createSendTasks.erase(resPair.first);
    return task.acked;
  }
}

} /* namespace FUSESwift */

