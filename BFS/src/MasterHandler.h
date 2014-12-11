/*
 * MasterHandler.h
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#ifndef MASTERHANDLER_H_
#define MASTERHANDLER_H_
#include "Global.h"
#include <atomic>
#include <vector>
#include "BackendManager.h"
#include "ZooNode.h"

namespace FUSESwift {

#define UPDATE_INTERVAL 1000//MICROSECONDS

class MasterHandler {
private:
  static std::vector<ZooNode> existingNodes;
  static std::atomic<bool> isRunning;
  MasterHandler();
  static std::vector<BackendItem> getExistingAssignments();
  static void leadershipLoop();
  static bool divideTaskAmongNodes(std::vector<BackendItem> *listFiles,std::vector<ZooNode> &globalView);
  static void removeDuplicates(std::vector<BackendItem> &newList,std::vector<BackendItem> &oldList);
  static bool cleanAssignmentFolder();
public:
  virtual ~MasterHandler();
  static void startLeadership();
  static void stopLeadership();
};

} /* namespace FUSESwift */

#endif /* MASTERHANDLER_H_ */
