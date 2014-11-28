/*
 * syncEvent.h
 *
 *  Created on: 2014-07-14
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef SYNCEVENT_H_
#define SYNCEVENT_H_

#include "filenode.h"
#include "cstring"

namespace FUSESwift {

enum class SyncEventType {RENAME,DELETE,UPDATE_CONTENT,UPDATE_METADATA,DOWNLOAD_CONTENT,DOWNLOAD_METADATA};

struct SyncEvent {
  SyncEventType type;
  //FileNode* node;
  /**
   * this is used because when the job is posted to queue we still
   * have the name and node in memory, but lated it'll be removed
   * and node* pointer is invalid so, we have to keep track of full
   * path of the file to be deleted
   */
  std::string fullPathBuffer;

  SyncEvent (SyncEventType _type, std::string _fullPathBuffer = ""):type(_type),fullPathBuffer(_fullPathBuffer){}
  bool operator == (const SyncEvent& a) const {
    if(this->type == a.type &&
       this->fullPathBuffer == a.fullPathBuffer)
      return true;
    else
      return false;
  }

  static std::string getEnumString(SyncEventType _type) {
    switch (_type) {
      case SyncEventType::RENAME:
        return "RENAME";
      case SyncEventType::DELETE:
        return "DELETE";
      case SyncEventType::UPDATE_CONTENT:
        return "UPDATE_CONTENT";
      case SyncEventType::UPDATE_METADATA:
        return "UPDATE_METADATA";
      case SyncEventType::DOWNLOAD_CONTENT:
        return "DOWNLOAD_CONTENT";
      case SyncEventType::DOWNLOAD_METADATA:
              return "DOWNLOAD_METADATA";
      default:
        return "UNKNOWN";
    }
  }
  std::string print() {
    std::string output = "Type:" + getEnumString(type)
                    + " DeleteEventFullPath:" + fullPathBuffer;
    return output;
  }
};

} /* namespace FUSESwift */
#endif /* SYNCEVENT_H_ */
