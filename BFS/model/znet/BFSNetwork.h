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
#include <unordered_map>
#include <atomic>
#include <vector>
#include <queue>
#include "ZeroNetwork.h"
#include <sys/stat.h>

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

#define ACK_TIMEOUT 5000//microseconds
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
	unsigned char requestorMac[6];
};


//Task Map
template <typename keyType,typename valueType>
struct taskMap {
private:
	std::unordered_map<keyType,valueType> readRcvTasks;
	std::mutex mutex;
public:
	auto inline find(keyType key) {
		return readRcvTasks.find(key);
	}
	auto inline insert(keyType key,valueType value) {
		std::lock_guard<std::mutex> lock(mutex);
		return readRcvTasks.insert(std::pair<keyType,valueType>(key,value));
	}
	auto inline erase(auto it) {
		std::lock_guard<std::mutex> lock(mutex);
		return readRcvTasks.erase(it);
	}

	auto inline end() {
		return readRcvTasks.end();
	}

	auto inline size() {
		return readRcvTasks.size();
	}
};


template <typename T>
class Queue
{
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

  auto begin() const{
		return queue.begin();
	}

  auto end() const{
		return queue.end();
	}

  const auto size() const {
  	return queue.size();
  }
};

enum class BFS_OPERATION {READ_REQUEST = 1, READ_RESPONSE = 2, WRITE_REQUEST = 3,
													WRITE_DATA = 4, WRITE_ACK = 5, ATTRIB_REQEUST = 6,
													ATTRIB_RESPONSE = 7, UNKNOWN = -1};

class BFSNetwork : ZeroNetwork{
private:
	BFSNetwork();
	static unsigned int DATA_LENGTH;
	static const std::string DEVICE;// = "eth0";
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
	static taskMap<uint8_t,ReadRcvTask*> readRcvTasks;
	static taskMap<uint8_t,ReadRcvTask*> attribRcvTasks;
	static taskMap<uint8_t,WriteDataTask*> writeDataTasks;
	static taskMap<uint8_t,WriteSndTask*> writeSendTasks;
	static Queue<SndTask*> sendQueue;
	static std::thread *rcvThread;
	static std::thread *sndThread;
	static uint32_t fileIDCounter;
	/** FileID **/
	static inline uint32_t getNextFileID() { return fileIDCounter++; }
	/** packet processing callback **/
	static void rcvLoop();
	static void sendLoop();
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
	/** Truncate Operation **/
	/** Delete Operation **/
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
	 * 	number of bytes read
	 * */
	static long readRemoteFile(void* _dstBuffer,size_t _size,size_t _offset,
				const std::string &_remoteFile,unsigned char _dstMAC[6]);
	/**
	 * Writes to _offset, _size bytes to _remoteFile@_dstMAC
	 * from _srcBuffer
	 * @return
	 * 	true success
	 * 	false failure
	 * */
	static bool writeRemoteFile(const void* _srcBuffer,size_t _size,size_t _offset,
				const std::string &_remoteFile,unsigned char _dstMAC[6]);

	static bool readRemoteFileAttrib(struct stat *attBuff,
			const std::string &remoteFile,unsigned char _dstMAC[6]);

	static const unsigned char* getMAC();

};

} /* namespace FUSESwift */

#endif /* BFSNETWORK_H_ */
