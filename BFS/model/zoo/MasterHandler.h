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
  static std::vector<BackendItem> *fileList;
  MasterHandler();
  static void leadershipLoop();
  static bool divideTaskAmongNodes();
public:
  virtual ~MasterHandler();
  static void startLeadership();
  static void stopLeadership();
};

} /* namespace FUSESwift */

#endif /* MASTERHANDLER_H_ */
