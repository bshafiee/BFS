/*
 * MasterHandler.h
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#ifndef MASTERHANDLER_H_
#define MASTERHANDLER_H_

#include <atomic>
#include <vector>
#include "../BackendManager.h"

namespace FUSESwift {

#define UPDATE_INTERVAL 1000//MICROSECONDS

class MasterHandler {
private:
  static std::atomic<bool> isRunning;
  //Remained files to be assigned to nodes
  static std::vector<BackendItem> remainedFiles;
  static std::vector<BackendItem> *oldFiles;
  MasterHandler();
  static void leadershipLoop();
  static bool divideTaskAmongNodes(std::vector<BackendItem> *listFiles);
  static void removeDuplicates(std::vector<BackendItem> &newList,std::vector<BackendItem> &oldList);
public:
  virtual ~MasterHandler();
  static void startLeadership();
  static void stopLeadership();
};

} /* namespace FUSESwift */

#endif /* MASTERHANDLER_H_ */
