/*
 * MasterHandler.cpp
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#include "MasterHandler.h"
#include "ZooHandler.h"
#include <thread>

using namespace std;

namespace FUSESwift {

atomic<bool> MasterHandler::isRunning;
vector<BackendItem> *MasterHandler::fileList = nullptr;

MasterHandler::MasterHandler() {
}

void MasterHandler::leadershipLoop() {
  long maxSleep = 5000*UPDATE_INTERVAL;
  long interval = UPDATE_INTERVAL;
  /**
   * Algorithm:
   * 1)Fetch list of files from backend
   * 2)Check if there is any new change
   * 3)Fetch list of avail nodes, their free space
   * 4)Divide tasks among nodes
   * 5)Clean all previous assignments
   * 6)Publish assignments
   */
  while(isRunning) {
    //1)Fetch list of files from backend
    Backend* backend = BackendManager::getActiveBackend();
    if(backend == nullptr) {
      printf("leadershipLoop(): No active backend!\n");
      interval *= 10;
      if(interval > maxSleep)
      	interval = maxSleep;
      usleep(interval);
    }
    vector<BackendItem> *newList = backend->list();
    //2)Check if there is any new change
    //TODO implement this...
    if(fileList != nullptr) {
      delete fileList;
      fileList = nullptr;
    }
    fileList = newList;
    //3)Fetch list of avail nodes, their free space
    //4)Divide tasks among nodes
    //5)Clean all previous assignments
    //6)Publish assignments

    //Adaptive sleep
    usleep(interval);
  }
}

MasterHandler::~MasterHandler() {
  // TODO Auto-generated destructor stub
}

void MasterHandler::startLeadership() {
  isRunning = true;
  new thread(leadershipLoop);
}

void MasterHandler::stopLeadership() {
  isRunning = false;
}

} /* namespace FUSESwift */
