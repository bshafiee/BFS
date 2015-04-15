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

#ifndef DOWNLOADQUEUE_H_
#define DOWNLOADQUEUE_H_
#include "Global.h"
#include "SyncQueue.h"
#include "Backend.h"

namespace BFS {

class DownloadQueue: public SyncQueue{
  //Process Events
  void processEvent(const SyncEvent* _event);
  static void syncLoopWrapper();
  void syncLoop();
  bool processDownloadContent(const SyncEvent* _event);
  void processDownloadMetadata(const SyncEvent* _event);
  //Private constructor
  DownloadQueue();
public:
  static DownloadQueue& getInstance();
  virtual ~DownloadQueue();
  //Start Downlaod Thread
  void startSynchronization();
  //Stop Downlaod Thread
  void stopSynchronization();
  //Add Download assignment from zoo
  void addZooTask(const vector<string> &assignments);
  //Process Backend for Standalone mode!
  void checkBackendInStandalonMode();
};

} /* namespace BFS */
#endif /* DOWNLOADQUEUE_H_ */
