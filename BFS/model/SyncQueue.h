/*
 * SyncQueue.h
 *
 *  Created on: Jul 12, 2014
 *      Author: behrooz
 */

#ifndef SYNCQUEUE_H_
#define SYNCQUEUE_H_

#include <vector>
#include "filenode.h"

using namespace std;

namespace FUSESwift {

class SyncQueue {
  vector<FileNode*> list;
  //singleton instance
  static SyncQueue *mInstance;
  //Private constructor
  SyncQueue();
public:
  static SyncQueue* getInstance();
  virtual ~SyncQueue();
  bool push(FileNode* _node);
  FileNode* pop();
  long size();
  size_t workloadSize();
};

} /* namespace FUSESwift */

#endif /* SYNCQUEUE_H_ */
