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

#include "DownloadQueue.h"
#include "UploadQueue.h"
#include <thread>
#include "BackendManager.h"
#include <iostream>
#include <mutex>
#include "SettingManager.h"
#include "Filesystem.h"
#include "MemoryController.h"
#include "Timer.h"
#include "LoggerInclude.h"

using namespace std;

namespace BFS {

DownloadQueue::DownloadQueue():SyncQueue() {
	// TODO Auto-generated constructor stub

}

DownloadQueue::~DownloadQueue() {
}

void DownloadQueue::addZooTask(const vector<string> &assignments) {
	//Try to query backend for list of files
	Backend* backend = BackendManager::getActiveBackend();
	if (backend == nullptr) {
		LOG(ERROR)<<"No active backend for Download Queue";
		return;
	}

	if(assignments.size() == 0)
		return;

	//Now we have actully some files to sync(download)
	for(auto it=assignments.begin();it!=assignments.end();it++) {
		push(new SyncEvent(SyncEventType::DOWNLOAD_CONTENT,*it));
		push(new SyncEvent(SyncEventType::DOWNLOAD_METADATA,*it));
		//log_msg("DOWNLOAD QUEUE: pushed %s Event.\n",it->c_str());
	}
}

bool DownloadQueue::processDownloadContent(const SyncEvent* _event) {
	if(_event == nullptr || _event->fullPathBuffer.length()==0)
		return false;

	//Ask backend to download the file for us
	Backend *backend = BackendManager::getActiveBackend();
	if(!backend){
	  LOG(ERROR)<<"No active backend to download"<<_event->fullPathBuffer;
	  return false;
	}
  return backend->get(_event);
}

void DownloadQueue::processDownloadMetadata(const SyncEvent* _event) {
}

DownloadQueue& DownloadQueue::getInstance() {
  //Static members
  static DownloadQueue instance;
  return instance;
}

void DownloadQueue::syncLoopWrapper() {
  DownloadQueue::getInstance().syncLoop();
}

void DownloadQueue::startSynchronization() {
  running = true;
	syncThread = new thread(syncLoopWrapper);
}

void DownloadQueue::stopSynchronization() {
  running = false;
}

void DownloadQueue::processEvent(const SyncEvent* _event) {
	Backend *backend = BackendManager::getActiveBackend();
	if (backend == nullptr) {
		LOG(ERROR)<<"No active backend";
		return;
	}
	switch (_event->type) {
	case SyncEventType::DOWNLOAD_CONTENT:
		LOG(DEBUG)<<"Event:DOWNLOAD_CONTENT fullpath:"<<_event->fullPathBuffer;
		processDownloadContent(_event);
		break;
	case SyncEventType::DOWNLOAD_METADATA:
		LOG(DEBUG)<<"Event:DOWNLOAD_METADATA fullpath:"<<_event->fullPathBuffer;
		processDownloadMetadata(_event);
		break;
	default:
		LOG(ERROR)<<"INVALID Event: file:"<<_event->fullPathBuffer<<" TYPE:"<<
		  SyncEvent::getEnumString(_event->type);
	}
}

void DownloadQueue::syncLoop() {
	const long maxDelay = 10000; //Milliseconds
	const long minDelay = 10; //Milliseconds
	long delay = 10; //Milliseconds
	Timer backendCheckTimer;
	backendCheckTimer.begin();

	while (running) {
	  backendCheckTimer.end();
	  if(backendCheckTimer.elapsedSec()>1){
	    checkBackendInStandalonMode();
	    backendCheckTimer.begin();
	  }
		//Empty list
		if (!list.size()) {
			//log_msg("DOWNLOADQUEUE: I will sleep for %zu milliseconds\n", delay);
			this_thread::sleep_for(chrono::milliseconds(delay));
			delay *= 2;
			if (delay > maxDelay)
				delay = maxDelay;
			//Update list
			//updateFromBackend();
			continue;
		}
		//pop the first element and process it
		SyncEvent* event = pop();
		processEvent(event);
		//do cleanup! delete event
    if(event != nullptr)
      delete event;
    event = nullptr;
		//reset delay
		delay = minDelay;
	}
}

void DownloadQueue::checkBackendInStandalonMode() {
  if(SettingManager::runtimeMode() != RUNTIME_MODE::STANDALONE  ||
      SettingManager::getBackendType() == BackendType::NONE)
    return;
  //LOG(INFO)<<"INJA";
  //Ask backend to download the file for us
  Backend *backend = BackendManager::getActiveBackend();
  if(!backend)
    return;

  vector<BackendItem> items;
  backend->list(items);
  vector<string> task;
  for(BackendItem &item:items){
    FileNode* fileNode = FileSystem::getInstance().findAndOpenNode(item.name);
    //If File exist then we won't download it!
    if(fileNode!=nullptr){
      fileNode->close(0);
      continue;
    }
    SyncEvent dummy(SyncEventType::DOWNLOAD_CONTENT,item.name);
    if(!containsEvent(&dummy))
      task.emplace_back(item.name);
  }
  if(!task.empty())
    addZooTask(task);
}

} /* namespace BFS */
