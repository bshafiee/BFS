/*
 * BFSTcpServiceHandler.h
 *
 *  Created on: Dec 3, 2014
 *      Author: root
 */

#ifndef BFSTCPSERVICEHANDLER_H_
#define BFSTCPSERVICEHANDLER_H_
#include "Global.h"
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/AutoPtr.h>

//namespace FUSESwift {

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif

/** Request Packet Structures **/
struct __attribute__ ((__packed__)) ReqPacket {
  uint32_t opCode;//4 byte
  char fileName[1024];//MTU-38 Bytes
};
typedef ReqPacket DeleteReqPacket;
typedef ReqPacket AttribReqPacket;
typedef ReqPacket CreateReqPacket;
typedef ReqPacket MoveReqPacket;
struct __attribute__ ((__packed__)) ReadReqPacket: public ReqPacket {
  uint64_t offset;//8 byte
  uint64_t size;//8 byte
};
typedef ReadReqPacket WriteReqPacket ;
struct __attribute__ ((__packed__)) TruncReqPacket: public ReqPacket {
  uint64_t size;//8 byte
};
/** Status Response Packet **/
struct __attribute__ ((__packed__)) ResPacket {
  int64_t statusCode = -1;
};

struct __attribute__ ((__packed__)) AttribResPacket: public ResPacket {
  uint64_t attribSize;
};

typedef ResPacket DeleteResPacket;
typedef ResPacket TruncateResPacket;
typedef ResPacket CreateResPacket;

struct __attribute__ ((__packed__)) ReadResPacket: public ResPacket {
  uint64_t readSize;
};

struct __attribute__ ((__packed__)) WriteResPacket: public ResPacket {
  uint64_t writtenSize;
};

/** BFS REMOTE OPERATION **/
enum class BFS_REMOTE_OPERATION {
  READ = 1, WRITE = 2, ATTRIB = 3,
  DELETE = 4, TRUNCATE = 5, CREATE = 6,
  MOVE = 7, UNKNOWN = 0
};

class BFSTcpServiceHandler {
  Poco::Net::StreamSocket socket;
  Poco::Net::SocketReactor& reactor;

  void onReadable(const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf);
  void onWriteable(const Poco::AutoPtr<Poco::Net::WritableNotification>& pNf);
  void onShutdown(const Poco::AutoPtr<Poco::Net::ShutdownNotification>& pNf);
  void onTimeout(const Poco::AutoPtr<Poco::Net::TimeoutNotification>& pNf);
  void onError(const Poco::AutoPtr<Poco::Net::ErrorNotification>& pNf);
  void onIdle(const Poco::AutoPtr<Poco::Net::IdleNotification>& pNf);
  /** Remote Operation Functions **/
  void onReadRequest(u_char *_packet);
  void onWriteRequest(u_char *_packet);
  void onAttribRequest(u_char *_packet);
  void onDeleteRequest(u_char *_packet);
  void onTruncateRequest(u_char *_packet);
  void onCreateRequest(u_char *_packet);
  void onMoveRequest(u_char *_packet);
  /** Helper **/
  BFS_REMOTE_OPERATION toBFSRemoteOperation(uint32_t _opCode);
public:
  BFSTcpServiceHandler(Poco::Net::StreamSocket& _socket,
        Poco::Net::SocketReactor& _reactor);
  virtual ~BFSTcpServiceHandler();
};
//}//namespace
#endif /* BFSTCPSERVICEHANDLER_H_ */
