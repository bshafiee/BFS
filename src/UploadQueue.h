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

#ifndef UPLOADQUEUE_H_
#define UPLOADQUEUE_H_
#include "Global.h"
#include "SyncQueue.h"

namespace FUSESwift {

class UploadQueue: public SyncQueue{
  //Process Events
  void processEvent(const SyncEvent* _event);
  static void syncLoopWrapper();
  void syncLoop();
  //Private Constructor
  UploadQueue();
public:
  static UploadQueue& getInstance();
  virtual ~UploadQueue();
  //Start/stop Upload Thread
  void startSynchronization();
  void stopSynchronization();
  //Check if this event is still valid
  /**
   * returns true in case the event should be performed
   */
  bool checkEventValidity(const SyncEvent& _event);
};

} /* namespace FUSESwift */
#endif /* UPLOADQUEUE_H_ */
