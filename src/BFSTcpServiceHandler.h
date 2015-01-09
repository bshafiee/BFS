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

namespace BFSTCPNetworkTypes {
/** Request Packet Structures **/
struct __attribute__ ((__packed__)) ReqPacket {
  uint32_t opCode;//4 byte
  char fileName[1024];//MTU-38 Bytes
};
typedef ReqPacket DeleteReqPacket;
typedef ReqPacket FlushReqPacket;
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

typedef ResPacket WriteResPacket;
typedef ResPacket DeleteResPacket;
typedef ResPacket FlushResPacket;
typedef ResPacket TruncateResPacket;
typedef ResPacket CreateResPacket;
typedef ResPacket MoveResPacket;

struct __attribute__ ((__packed__)) ReadResPacket: public ResPacket {
  uint64_t readSize;
};

/** BFS REMOTE OPERATION **/
enum class BFS_REMOTE_OPERATION {
  READ = 1, WRITE = 2, ATTRIB = 3,
  DELETE = 4, TRUNCATE = 5, CREATE = 6,
  MOVE = 7, FLUSH = 8, UNKNOWN = 0
};


struct __attribute__ ((__packed__)) packed_stat_info
{
  uint64_t st_dev;   /* Device.  */
  uint64_t st_ino;   /* File serial number.  */
  uint64_t st_nlink;   /* Link count.  */
  uint32_t st_mode;   /* File mode.  */
  uint32_t st_uid;   /* User ID of the file's owner. */
  uint32_t st_gid;   /* Group ID of the file's group.*/
  uint64_t st_rdev;    /* Device number, if device.  */
  uint64_t st_size;      /* Size of file, in bytes.  */
  int64_t st_blksize; /* Optimal block size for I/O.  */
  int64_t st_blocks;   /* Number 512-byte blocks allocated. */
  int64_t st_atim;    /* Time of last access.  */
  int64_t st_mtim;    /* Time of last modification.  */
  int64_t st_ctim;    /* Time of last status change.  */

};

}//end of BFSTCPNetworkTypes

using namespace BFSTCPNetworkTypes;

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
  void onFlushRequest(u_char *_packet);
  void onTruncateRequest(u_char *_packet);
  void onCreateRequest(u_char *_packet);
  void onMoveRequest(u_char *_packet);
  int64_t processMoveRequest(std::string _fileName);
  /** Helper **/
  BFS_REMOTE_OPERATION toBFSRemoteOperation(uint32_t _opCode);
public:
  BFSTcpServiceHandler(Poco::Net::StreamSocket& _socket,
        Poco::Net::SocketReactor& _reactor);
  virtual ~BFSTcpServiceHandler();
};
//}//namespace
#endif /* BFSTCPSERVICEHANDLER_H_ */
