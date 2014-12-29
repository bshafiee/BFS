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
