/*
 * BFSNetwork.h
 *
 *  Created on: Oct 8, 2014
 *      Author: behrooz
 */

#ifndef BFSNETWORK_H_
#define BFSNETWORK_H_


#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <atomic>
#include <vector>
#include <queue>
#include <sys/stat.h>
#include "ZeroNetwork.h"
#include "filenode.h"
#include "LoggerInclude.h"

namespace FUSESwift {

/**
 * BFS Network Packet on wire 1500 Bytes
 *  0 _________________________________HEADER__________________________________
 * |                  |                  |                  |                  |
 * |  6 Byte DST_MAC  |  6 Byte SRC_MAC  |  1 Byte POROTO_1 |  1 Byte POROTO_2 |
 * |  Index: [0->5]   |  Index: [6->11]  | Index: [12]=0XAA | Index: [13]=0XAA |
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

#define ACK_TIMEOUT 1000l//miliseconds
#define HEADER_LEN 38


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


template <typename T>
class Queue {
private:
  std::vector<T> queue;
  std::mutex mutex;
  std::condition_variable cond;
 public:
  T pop() {
    std::unique_lock<std::mutex> mlock(mutex);
    while (queue.empty()) {
      cond.wait(mlock);
    }
    auto item = queue.front();
    queue.erase(queue.begin());
    return item;
  }

  T front() {
		std::unique_lock<std::mutex> mlock(mutex);
		while (queue.empty()) {
			cond.wait(mlock);
		}
		auto item = queue.front();
		return item;
	}

  template <class... Args>
  void emplace(Args&&... args) {
  	std::unique_lock<std::mutex> mlock(mutex);
		queue.empalce_back(std::forward<Args>(args)...);
		mlock.unlock();
		cond.notify_one();
  }

  void push(const T& item) {
    std::unique_lock<std::mutex> mlock(mutex);
    queue.push_back(item);
    mlock.unlock();
    cond.notify_one();
  }

  void push(T&& item) {
    std::unique_lock<std::mutex> mlock(mutex);
    queue.push_back(std::move(item));
    mlock.unlock();
    cond.notify_one();
  }

  inline auto at(uint64_t index) {
    return queue[index];
  }

  auto begin() const{
		return queue.begin();
	}

  auto end() const{
		return queue.end();
	}

  const auto size() {
    std::lock_guard<std::mutex> lock(mutex);
  	return queue.size();
  }
};

enum class BFS_OPERATION {READ_REQUEST = 1, READ_RESPONSE = 2,
                          WRITE_REQUEST = 3,WRITE_DATA = 4,
                          WRITE_ACK = 5, ATTRIB_REQUEST = 6,
													ATTRIB_RESPONSE = 7, DELETE_REQUEST = 8,
													DELETE_RESPONSE = 9, TRUNCATE_REQUEST = 10,
													TRUNCATE_RESPONSE = 11, CREATE_REQUEST = 12,
													CREATE_RESPONSE = 13, UNKNOWN = 0};

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
	static const unsigned char BFS_PROTO_BYTE1 = 0xaa;
	static const unsigned char BFS_PROTO_BYTE2 = 0xaa;
	static unsigned char MAC[6];
	//Variables
	static std::atomic<bool> isRunning;
	static taskMap<uint32_t,ReadRcvTask*> readRcvTasks;
	static taskMap<uint32_t,ReadRcvTask*> attribRcvTasks;
	static taskMap<uint32_t,WriteDataTask> writeDataTasks;
	static taskMap<uint32_t,WriteSndTask*> writeSendTasks;
	static taskMap<uint32_t,WriteSndTask*> deleteSendTasks;
	static taskMap<uint32_t,WriteSndTask*> truncateSendTasks;
	static taskMap<uint32_t,WriteSndTask*> createSendTasks;
	static taskMap<uint32_t,MoveConfirmTask*> moveConfirmSendTasks;
	static Queue<SndTask*> sendQueue;
	static Queue<MoveTask*> moveQueue;
	static std::thread *rcvThread;
	static std::thread *sndThread;
	static std::thread *moveThread;

	static std::atomic<uint32_t> fileIDCounter;
	static uint32_t getNextFileID();
	/** packet processing callback **/
	static void rcvLoop();
	static void sendLoop();
	static void moveLoop();
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
			const WriteSndTask &_writeTask);
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
	/** Truncate Operation **/
	static void onTruncateReqPacket(const u_char *_packet);
  static void onTruncateResPacket(const u_char *_packet);
	/** Delete Operation **/
  static void onDeleteReqPacket(const u_char *_packet);
  static void onDeleteResPacket(const u_char *_packet);
	/** Rename Operation **/
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

	static bool readRemoteFileAttrib(struct stat *attBuff,
			const std::string &remoteFile, const unsigned char _dstMAC[6]);

	static bool deleteRemoteFile(const std::string &_remoteFile, const unsigned char _dstMAC[6]);

	static bool truncateRemoteFile(const std::string &_remoteFile, size_t _newSize, const unsigned char _dstMAC[6]);

	static bool createRemoteFile(const std::string &_remoteFile, const unsigned char _dstMAC[6]);

	static const unsigned char* getMAC();

};

} /* namespace FUSESwift */

#endif /* BFSNETWORK_H_ */
