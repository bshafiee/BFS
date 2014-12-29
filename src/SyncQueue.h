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

#ifndef SYNCQUEUE_H_
#define SYNCQUEUE_H_
#include "Global.h"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include "Filenode.h"
#include "SyncEvent.h"

using namespace std;

namespace FUSESwift {

class SyncQueue{
protected:
  //atomic stop/start condition
  atomic<bool> running;
  vector<SyncEvent*> list;
  //Mutex to protect queue
  std::mutex queueMutex;
  //Thread to run syncLoop
  std::thread *syncThread;
  //Private constructor
  SyncQueue();
  //Protected virtual methods
  virtual void processEvent(const SyncEvent* _event) = 0;
  virtual void syncLoop() = 0;
public:
  virtual ~SyncQueue();
  bool push(SyncEvent* _node);
  SyncEvent* pop();
  long size();
  virtual void startSynchronization() = 0;
  virtual void stopSynchronization() = 0;
  inline bool containsEvent(const SyncEvent* _event);
};

} /* namespace FUSESwift */

#endif /* SYNCQUEUE_H_ */
