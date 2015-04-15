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

#ifndef SYNCEVENT_H_
#define SYNCEVENT_H_
#include "Global.h"
#include "cstring"
#include "Filenode.h"

namespace BFS {

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

} /* namespace BFS */
#endif /* SYNCEVENT_H_ */
