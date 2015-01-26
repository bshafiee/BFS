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

#ifndef BFSNETWORK_H_
#define BFSNETWORK_H_

#include "Global.h"
#include "Queue.h"
#include <thread>
#include <atomic>
#include <unordered_map>
#include <sys/stat.h>

#include "Filenode.h"
#include "ZeroNetwork.h"

namespace FUSESwift {

/**
 * BFS Network Packet on wire 1500 Bytes
 *  0 _________________________________HEADER__________________________________
 * |                  |                  |                  |                  |
 * |  6 Byte DST_MAC  |  6 Byte SRC_MAC  |  1 Byte POROTO_1 |  1 Byte POROTO_2 |
 * |  Index: [0->5]   |  Index: [6->11]  | Index: [12]=0XCC | Index: [13]=0XCC |
 * |-----------------------------------PAYLOAD---------------------------------|
 * |                                                                           |
 * |                               MTU-14 Bytes                                |
 * |                                                                           |
 * |______________________________________________________________________MTU-1|
 *
 * Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |      NOT USED or         |
 * | Index: [14-17]   |    Index:    |    Index:    |      FILE NAME or        |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |      DATA                |
 * | Index: [18-21]   |              |              |    Index: [38->MTU-1]    |
 * |__________________|______________|______________|_____________________MTU-1|
 *
 *
 *
 *--READ -----------------------------------------------------------------------
 * READ_REQUEST Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        FILE NAME         |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
 *
 * READ_RESPONSE Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        READ DATA         |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
 *
 * -----------------------------------------------------------------------------
 *
 *
 *--WRITE-----------------------------------------------------------------------
 *
 * WRITE_REQUEST Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        FILE NAME         |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
 *
 * WRITE_DATA Payload:
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |   8 Bytes    |   8 Bytes    |       MTU-38 Bytes       |
 * | 4 Byte Op-Code   |    Offset    |     size     |        WRITE DATA        |
 * | Index: [14-17]   |    Index:    |    Index:    |     Index: [38->MTU-1]   |
 * | 4 Byte File ID   |   [22->29]   |   [30->37]   |                          |
 * | Index: [18-21]   |              |              |                          |
 * |__________________|______________|______________|_____________________MTU-1|
 *
 * WRITE_ACK
 * 15__________________________________PAYLOAD_________________________________
 * |    8 Bytes       |                                                        |
 * | 4 Byte Op-Code   |                                                        |
 * | Index: [14-17]   |                      NOT USED                          |
 * | 4 Byte File ID   |                   Index: [22->MTU-1]                   |
 * | Index: [18-21]   |                                                        |
 * |__________________|___________________________________________________MTU-1|
 *
 * -----------------------------------------------------------------------------
 *
 */

#define ACK_TIMEOUT 5000l//miliseconds
#define HEADER_LEN 38

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif


struct __attribute__ ((__packed__)) ReadReqPacket {
	unsigned char dstMac[6];
	unsigned char srcMac[6];
	char protoByte1;
	char protoByte2;
	uint32_t opCode;//4 byte
	uint32_t fileID;//4 byte
	uint64_t offset;//8 byte
	uint64_t size;//8 byte
	char fileName[1];//MTU-38 Bytes
};
//Write_req packet is similar
typedef ReadReqPacket WriteReqPacket;

struct __attribute__ ((__packed__)) ReadResPacket {
	unsigned char dstMac[6];
	unsigned char srcMac[6];
	char protoByte1;
	char protoByte2;
	uint32_t opCode;//4 byte
	uint32_t fileID;//4 byte
	uint64_t offset;//8 byte
	uint64_t size;//8 byte
	char data[1];//MTU-38 Bytes
};
//Write packet is similar
typedef ReadResPacket WriteDataPacket;

struct ReadRcvTask {
	std::string remoteFile;
	uint32_t fileID;
	uint64_t offset;
	uint64_t size;
	void* dstBuffer;
	/**
	 * Indicates how much in total it was read from destination
	 * it's not always necessarily totalRead = size (check fuse read,they
	 * always read in multiples of page size no matter what's the file size)
	 **/
	uint64_t totalRead;
	/** Synchronization **/
	bool ready = false;
	std::mutex m;
	std::condition_variable cv;
};


struct MoveTask {
  std::string fileName;
  uint32_t fileID;
  unsigned char requestorMac[6];
  /** Synchronization **/
  bool ready = false;
  bool acked = false;
  std::mutex m;
  std::condition_variable cv;
};

struct MoveConfirmTask {
  uint32_t fileID;
  /** Synchronization **/
  bool ready = false;
  bool acked = false;
  std::mutex m;
  std::condition_variable cv;
};

/**
 * This is designed to handle file moving on write handler,
 * So we won't be blocking rcvloop while this file is being transfered
 * also avoids circular wait in move operation.
 */
struct TransferTask {
  TransferTask (uint64_t _size,uint64_t _iNodeNum):
    size(_size),inodeNum(_iNodeNum) {
    packet = new u_char[_size];
  }
  ~TransferTask() {
    delete []packet;
  }
  u_char *packet;
  uint64_t size;
  uint64_t inodeNum;
};

enum SEND_TASK_TYPE {SEND_READ, SEND_WRITE};
struct SndTask {
	SndTask(SEND_TASK_TYPE _type):type(_type){}
	SEND_TASK_TYPE type;
};

struct ReadSndTask : SndTask{
	ReadSndTask ():SndTask(SEND_TASK_TYPE::SEND_READ),size(-1),
			offset(-1),localFile(""),fileID(0),data(nullptr){}
	uint64_t size;
	uint64_t offset;
	std::string localFile;
	uint32_t fileID;
	unsigned char requestorMac[6];
	void* data;
};

struct WriteSndTask : SndTask{
	WriteSndTask():SndTask(SEND_TASK_TYPE::SEND_WRITE){}
	std::string remoteFile;
	uint32_t fileID;
	uint64_t offset;
	uint64_t size;
	const void* srcBuffer;
	unsigned char dstMac[6];
	bool acked = false;
	/** Ack Synchronization **/
	bool ack_ready = false;
	std::mutex ack_m;
	std::condition_variable ack_cv;
	/** Synchronization With Caller **/
	bool ready = false;
	std::mutex m;
	std::condition_variable cv;
};

struct WriteDataTask {
	std::string remoteFile;
	uint32_t fileID;
	uint64_t offset;
	uint64_t size;
	uint64_t inodeNum;
	bool failed;
	unsigned char requestorMac[6];
};


//Task Map
template <typename keyType,typename valueType>
struct taskMap {
private:
	std::unordered_map<keyType,valueType> tasks;
	std::mutex mutex;
public:
	taskMap(unsigned int initCapacity) {
	  tasks.reserve(initCapacity);
	}
	auto inline find(keyType key) {
	  std::lock_guard<std::mutex> lock(mutex);
		return tasks.find(key);
	}
	auto inline insert(keyType key,valueType value) {
		std::lock_guard<std::mutex> lock(mutex);
		auto res = tasks.insert(std::pair<keyType,valueType>(key,value));
		return res;
	}
	auto inline erase(auto it) {
	  std::lock_guard<std::mutex> lock(mutex);
		auto res = tasks.erase(it);
		return res;
	}

	auto inline end() {
		return tasks.end();
	}

	auto inline begin() {
    return tasks.begin();
  }

	auto inline size() {
	  std::lock_guard<std::mutex> lock(mutex);
		return tasks.size();
	}

  void lock() {
    mutex.lock();
  }

  void unlock() {
    mutex.unlock();
  }
};


enum class BFS_OPERATION {READ_REQUEST = 1, READ_RESPONSE = 2,
                          WRITE_REQUEST = 3,WRITE_DATA = 4,
                          WRITE_ACK = 5, ATTRIB_REQUEST = 6,
													ATTRIB_RESPONSE = 7, DELETE_REQUEST = 8,
													DELETE_RESPONSE = 9, TRUNCATE_REQUEST = 10,
													TRUNCATE_RESPONSE = 11, CREATE_REQUEST = 12,
													CREATE_RESPONSE = 13, FLUSH_REQUEST = 14,
													FLUSH_RESPONSE = 15, UNKNOWN = 0};

class BFSNetwork : ZeroNetwork{
private:
	BFSNetwork();
	static unsigned int DATA_LENGTH;
	static std::string DEVICE;// = "eth0";
	static const int SRC_MAC_INDEX = 6;
	static const int DST_MAC_INDEX = 0;
	static const int PROTO_BYTE_INDEX1 = 12;
	static const int PROTO_BYTE_INDEX2 = 13;
	static const int OP_CODE_INDEX = 14;
	static const unsigned char BFS_PROTO_BYTE1 = 0xCC;
	static const unsigned char BFS_PROTO_BYTE2 = 0xCC;
	static unsigned char MAC[6];
	static std::atomic<bool> macInitialized;
	//Variables
	static std::atomic<bool> isRunning;
	static std::atomic<bool> rcvLoopDead;
	static taskMap<uint32_t,ReadRcvTask*> readRcvTasks;
	static taskMap<uint32_t,ReadRcvTask*> attribRcvTasks;
	static taskMap<uint32_t,WriteDataTask> writeDataTasks;
	static taskMap<uint32_t,WriteSndTask*> writeSendTasks;
	static taskMap<uint32_t,WriteSndTask*> deleteSendTasks;
	static taskMap<uint32_t,WriteSndTask*> flushSendTasks;
	static taskMap<uint32_t,WriteSndTask*> truncateSendTasks;
	static taskMap<uint32_t,WriteSndTask*> createSendTasks;
	static taskMap<uint32_t,MoveConfirmTask*> moveConfirmSendTasks;
	static Queue<SndTask*> sendQueue;
	static Queue<MoveTask*> moveQueue;
	static Queue<TransferTask*> transferQueue;
	static std::thread *rcvThread;
	static std::thread *sndThread;
	static std::thread *moveThread;
	static std::thread *transferThread;


	static std::atomic<uint32_t> fileIDCounter;
	static uint32_t getNextFileID();
	/** packet processing callback **/
	static void rcvLoop();
	static void sendLoop();
	static void moveLoop();
	static void transferLoop();
	static void processTransfer(TransferTask* _task);
	static void fillBFSHeader(char *_packet,const unsigned char _dstMAC[6]);
	/** Read Operation **/
	static void onReadResPacket(const u_char *_packet);
	static void onReadReqPacket(const u_char *_packet);
	static void fillReadReqPacket(ReadReqPacket *_packet,const
			unsigned char _dstMAC[6], const ReadRcvTask &_rcvTask,uint32_t _fileID);
	static void fillReadResPacket(ReadResPacket *_packet, const ReadSndTask &_sndTask);
	static void processReadSendTask(ReadSndTask& _task);
	/** Write Operation **/
	static void processWriteSendTask(WriteSndTask& _task);
	static void fillWriteDataPacket(WriteDataPacket *_packet,
			const WriteSndTask &_writeTask,uint64_t _size);
	static void fillWriteReqPacket(WriteReqPacket* _packet, const unsigned
			char _dstMAC[6], const WriteSndTask& _sndTask);
	static void onWriteDataPacket(const u_char *_packet);
	static void onWriteReqPacket(const u_char *_packet);
	static void onWriteAckPacket(const u_char *_packet);
	/** Get Attributes **/
	static void onAttribReqPacket(const u_char *_packet);
	static void onAttribResPacket(const u_char *_packet);
	/** Create Operation **/
  static void onCreateReqPacket(const u_char *_packet);
  static void onCreateResPacket(const u_char *_packet);
  static void processMoveTask(const MoveTask &_moveTask);
  static void processMoveTask2(const MoveTask &_moveTask);
  static void sendMoveResponse(const MoveTask &_moveTask, int res);
	/** Truncate Operation **/
	static void onTruncateReqPacket(const u_char *_packet);
  static void onTruncateResPacket(const u_char *_packet);
	/** Delete Operation **/
  static void onDeleteReqPacket(const u_char *_packet);
  static void onDeleteResPacket(const u_char *_packet);
	/** Rename Operation **/
  /** Flush Operation **/
  static void onFlushReqPacket(const u_char *_packet);
  static void onFlushResPacket(const u_char *_packet);
public:
	virtual ~BFSNetwork();
	/** Public interface **/
	static bool startNetwork();
	static void stopNetwork();


	/** FILE Modification API **/

	/**
	 * Reads from _offset, _size bytes from _remoteFile@_dstMAC
	 * into _dstBuffer
	 * @return
	 * 	-1 error
	 * 	Number of bytes read
	 * */
	static long readRemoteFile(void* _dstBuffer,size_t _size,size_t _offset,
				const std::string &_remoteFile, const unsigned char _dstMAC[6]);
	/**
	 * Writes to _offset, _size bytes to _remoteFile@_dstMAC
	 * from _srcBuffer
	 * @return
	 * Number of bytes written
	 * */
	static long writeRemoteFile(const void* _srcBuffer,size_t _size,size_t _offset,
				const std::string &_remoteFile, const unsigned char _dstMAC[6]);

	static bool readRemoteFileAttrib(struct packed_stat_info *attBuff,
			const std::string &remoteFile, const unsigned char _dstMAC[6]);

	static bool deleteRemoteFile(const std::string &_remoteFile, const unsigned char _dstMAC[6]);

	static bool truncateRemoteFile(const std::string &_remoteFile, size_t _newSize, const unsigned char _dstMAC[6]);

	static bool createRemoteFile(const std::string &_remoteFile, const unsigned char _dstMAC[6]);

	static bool flushRemoteFile(const std::string &_remoteFile, const unsigned char _dstMAC[6]);

	static const unsigned char* getMAC();

};

} /* namespace FUSESwift */

#endif /* BFSNETWORK_H_ */
