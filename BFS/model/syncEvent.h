/*
 * syncEvent.h
 *
 *  Created on: 2014-07-14
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef SYNCEVENT_H_
#define SYNCEVENT_H_

#include "filenode.h"

namespace FUSESwift {

enum class SyncEventType {RENAME,DELETE,UPDATE_CONTENT,UPDATE_METADATA};

struct SyncEvent {
  SyncEventType type;
  FileNode* node;
  SyncEvent (SyncEventType _type,FileNode* _node):type(_type),node(_node) {}
};

} /* namespace FUSESwift */
#endif /* SYNCEVENT_H_ */
