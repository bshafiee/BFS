/*
 * BFSNetwork.cpp
 *
 *  Created on: Oct 8, 2014
 *      Author: behrooz
 */

#include "BFSNetwork.h"
#include <iostream>
#include "ZeroNetwork.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <arpa/inet.h>
extern "C" {
	#include <pfring.h>
}
#include "../filesystem.h"
#include "../filenode.h"
#include "../SettingManager.h"
#include <sys/time.h>

namespace FUSESwift {

using namespace std;

/** static members **/
string BFSNetwork::DEVICE= "eth0";
unsigned char BFSNetwork::MAC[6];
taskMap<uint32_t,ReadRcvTask*> BFSNetwork::readRcvTasks;
taskMap<uint32_t,WriteDataTask> BFSNetwork::writeDataTasks;
taskMap<uint32_t,ReadRcvTask*> BFSNetwork::attribRcvTasks;
atomic<bool> BFSNetwork::isRunning(true);
Queue<SndTask*> BFSNetwork::sendQueue;
thread * BFSNetwork::rcvThread = nullptr;
thread * BFSNetwork::sndThread = nullptr;
uint32_t BFSNetwork::fileIDCounter = 0;
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
	//Start rcvLoop
	rcvThread = new thread(rcvLoop);
	//Start Send Loop
  sndThread = new thread(sendLoop);

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
    const std::string& _remoteFile,unsigned char _dstMAC[6]) {
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
	//task.m = new mutex();
	//task.cv = new condition_variable();
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
		return result;
	}

	//Now wait for it!
  unique_lock<std::mutex> lk(task.m);
	while(!task.ready) {
		task.cv.wait(lk);
		if (!task.ready) {
			fprintf(stderr,"HOLY SHIT! Spurious wake up!\n");
			readRcvTasks.erase(resPair.first);
			result = -1;
			return result;
		}
	}
	result = task.totalRead;

	//remove it from queue
	readRcvTasks.erase(resPair.first);
	//fprintf(stderr,"READREMOTE, size:%lu\n",_size);
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
	if(fNode == nullptr || _task.size == 0) { //error just send a packet of size 0
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
  //fprintf(stderr,"PROCESSED READ REQ:%s offset:%lu size:%lu\n",_task.localFile.c_str(),_task.offset,total);
  //close file
  fNode->close();
  _task.size = total;

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
			return BFS_OPERATION::ATTRIB_REQEUST;
		case 7:
			return BFS_OPERATION::ATTRIB_RESPONSE;
	}
	return BFS_OPERATION::UNKNOWN;
}

void BFSNetwork::rcvLoop() {
	while(isRunning){
		//Get a packet
		struct pfring_pkthdr _header;
		//unsigned char _packet[SEND_LENGTH];
		u_char *_packet = nullptr;
		int res = pfring_recv((pfring*)pd,&_packet,0,&_header,1);
		if(res <= 0){
			fprintf(stderr,"Error in pfring_recv:%d\n",res);
			continue;
		}

		//return if it's not of our size
		if(_header.len != (unsigned int)MTU) {
			//fprintf(stderr,"Fragmentation! dropping. CapLen:%u Len:%u\n",
			//		_header.caplen,_header.len);
			continue;
		}

		//Return not a valid packet of our protocol!
		if(_packet[PROTO_BYTE_INDEX1] != BFS_PROTO_BYTE1 ||
			 _packet[PROTO_BYTE_INDEX2] != BFS_PROTO_BYTE2 ){
			printf("Byte[%d]=%.2x,Byte[%d]=%.2x\n",PROTO_BYTE_INDEX1,
					_packet[PROTO_BYTE_INDEX1],PROTO_BYTE_INDEX2,_packet[PROTO_BYTE_INDEX2]);
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
			case BFS_OPERATION::ATTRIB_REQEUST:
				onAttribReqPacket(_packet);
				break;
			case BFS_OPERATION::ATTRIB_RESPONSE:
				onAttribResPacket(_packet);
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
 **/
void FUSESwift::BFSNetwork::onReadResPacket(const u_char* _packet) {
	ReadResPacket *resPacket = (ReadResPacket*)_packet;
	//Parse Packet Network order first!
	resPacket->offset = be64toh(resPacket->offset);
	resPacket->size = be64toh(resPacket->size);
	resPacket->fileID = ntohl(resPacket->fileID);
	//Get RcvTask back using file id!
	auto taskIt = readRcvTasks.find(resPacket->fileID);
	if(taskIt == readRcvTasks.end()) {
		fprintf(stderr,"onReadResPacket():No valid Task for this Packet. FileID:%d\n",resPacket->fileID);
		cerr<<"  Size:"<<readRcvTasks.size()<<endl;
		return;
	}

	//Exist! so fill task buffer
	ReadRcvTask* task = taskIt->second;
	if(resPacket->size)
		memcpy((char*)task->dstBuffer+task->totalRead,resPacket->data,resPacket->size);
	//Check if finished, signal conditional variable!
	//If total amount of required data was read or a package of size 0 is received!
	task->totalRead += resPacket->size;
	if(resPacket->offset+resPacket->size == task->offset+task->size ||
			resPacket->size == 0) {
    unique_lock<std::mutex> lk(task->m);
    task->ready = true;
    task->cv.notify_one();
	}

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

	//fprintf(stderr,"GOT READ REQ:%s offset:%lu size:%lu\n",task->localFile.c_str(),task->offset,task->size);
	//Push it on the Queue
	sendQueue.push(task);
}


long BFSNetwork::writeRemoteFile(const void* _srcBuffer, size_t _size,
    size_t _offset, const std::string& _remoteFile, unsigned char _dstMAC[6]) {
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
	//task.m = new mutex();
	//task.cv = new condition_variable();
	task.ack_ready = false;
	//task.ack_m = new mutex();
	//task.ack_cv = new condition_variable();

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

  struct timespec bef,after;
  //fprintf(stderr,"BEFORE SLEEP TIME: %ld\n",now.tv_nsec);
  //fflush(stderr);
  //Now wait for the ACK!
  unique_lock<mutex> ack_lk(_task.ack_m);
	while(!_task.ack_ready) {
	  clock_gettime(CLOCK_MONOTONIC, &bef);
		_task.ack_cv.wait_for(ack_lk,chrono::milliseconds(ACK_TIMEOUT));
		clock_gettime(CLOCK_MONOTONIC, &after);
		uint64_t nanoDiff = after.tv_nsec - bef.tv_nsec;
		if(!_task.ack_ready && (nanoDiff >= ACK_TIMEOUT*1000ll*1000ll))
		  break;
	}
	clock_gettime(CLOCK_MONOTONIC, &after);
	if(!_task.acked) {
	  fprintf(stderr,"Befor TIME: %zu\n",bef.tv_nsec);
    fprintf(stderr,"After TIME: %zu\n",after.tv_nsec);
    uint64_t diff = after.tv_nsec - bef.tv_nsec;
    fprintf(stderr,"Delta TIME: %zu nanoseconds\n",diff);
    fflush(stderr);
	}

	if(!_task.acked)
		printf("WriteRequest not Acked:%d ACK_Ready:%d\n",_task.fileID,_task.ack_ready);

  //GOT ACK So Signal Caller to wake up
  unique_lock<mutex> lk(_task.m);
	_task.ready = true;
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

/*static std::thread *md5Thread;
static void md5CalculatorAndReleaseMemory(unsigned char* buffer,unsigned long long size){
	MD5 md5;
	cout<<"MD5:"<<md5.digestMemory(buffer,size)<<endl;
	delete []buffer;
	buffer = nullptr;
}*/

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
    fprintf(stderr,"onWriteDataPacket(), File Not foudn:%s!\n",taskIt->second.remoteFile.c_str());
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
  if(fNode == nullptr) {
    fprintf(stderr,"onWriteDataPacket(), File Not foudn:%s!\n",writeTask.remoteFile.c_str());
    return;
  }
  //Open file
  fNode->open();

	//Put it on the rcvTask map!
	auto resPair = writeDataTasks.insert(writeTask.fileID,writeTask);
	if(!resPair.second) {
		fprintf(stderr,"onWriteReqPacket(), error in inserting task to writeRcvTasks!size:%lu\n",writeDataTasks.size());
//		delete writeTask;
//		writeTask = nullptr;
		return;
	}

	//fprintf(stderr,"Got a writeReq: offset:%lu size:%lu\n",writeTask.offset,writeTask.size);
}

void FUSESwift::BFSNetwork::onWriteAckPacket(const u_char* _packet) {

	WriteDataPacket *reqPacket = (WriteDataPacket*)_packet;
	//Find write sendTask
	uint32_t fileID = ntohl(reqPacket->fileID);
	uint64_t size = be64toh(reqPacket->size);

	struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  //fprintf(stderr,"ACK   TIME: %ld\n",now.tv_nsec);
  //fflush(stderr);

	for(auto it = sendQueue.begin();it!=sendQueue.end();it++) {
		SndTask *task = *it;
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

	fprintf(stderr,"ACK   TIME: %zu\n",now.tv_nsec);
	//Error
	printf("onWriteAckPacket():got ack for:%u but no task found!\n",fileID);
	fflush(stderr);
}


const unsigned char* BFSNetwork::getMAC() {
	return MAC;
}

bool BFSNetwork::readRemoteFileAttrib(struct stat *attBuff,
		const string &remoteFile,unsigned char _dstMAC[6]) {
	//Create a new task
	ReadRcvTask task;
	task.dstBuffer = attBuff;
	task.fileID = getNextFileID();
	task.remoteFile = remoteFile;
	task.ready = false;
	char buffer[MTU];
	ReadReqPacket *reqPacket = (ReadReqPacket*)buffer;
	fillReadReqPacket(reqPacket,_dstMAC,task,task.fileID);
	reqPacket->opCode = htonl((uint32_t)BFS_OPERATION::ATTRIB_REQEUST);
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

	//Now wait for it!
  unique_lock<std::mutex> lk(task.m);
	while(!task.ready) {
		task.cv.wait_for(lk,chrono::milliseconds(ACK_TIMEOUT));
		if (!task.ready) {
			fprintf(stderr,"readRemoteAttrib(), HOLY SHIT! Spurious wake up!\n");
			attribRcvTasks.erase(resPair.first);
			return false;
		}
	}

	//remove it from queue
	attribRcvTasks.erase(resPair.first);
	if(task.offset == 1)//success
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
	if(taskIt == attribRcvTasks.end()) {
		fprintf(stderr,"onAttribResPacket():No valid Task for this Packet. FileID:%d\n",
				attribresPacket->fileID);
		return;
	}

	//Exist! so fill task buffer
	ReadRcvTask* task = taskIt->second;
	if(attribresPacket->offset == 1) {//Success
		task->offset = 1;//Indicate success
		memcpy((char*)task->dstBuffer,attribresPacket->data,sizeof(struct stat));
	}
	else
		task->offset = 0;//Indicate Failure

	unique_lock<std::mutex> lk(task->m);
	task->ready = true;
	task->cv.notify_one();
}

} /* namespace FUSESwift */

