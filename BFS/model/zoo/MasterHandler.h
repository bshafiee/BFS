/*
 * MasterHandler.h
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#ifndef MASTERHANDLER_H_
#define MASTERHANDLER_H_

//#include "ZooHandler.h"

namespace FUSESwift {

class MasterHandler {
private:
  static bool isRunning;
  MasterHandler();
  static void leadershipLoop();
public:
  virtual ~MasterHandler();
  static void startLeadership();
  static void stopLeadership();
};

} /* namespace FUSESwift */

#endif /* MASTERHANDLER_H_ */
