/*
 * UploadQueue.cpp
 *
 *  Created on: 2014-07-17
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "DownloadQueue.h"
#include "UploadQueue.h"
#include <thread>
#include "BackendManager.h"
#include "../log.h"
#include "filesystem.h"
#include <iostream>

using namespace std;

namespace FUSESwift {

DownloadQueue::DownloadQueue():SyncQueue() {
	// TODO Auto-generated constructor stub

}

DownloadQueue::~DownloadQueue() {
}

bool DownloadQueue::shouldDownload(string path) {
  for(string toBeDelete:deletedFiles)
    if(toBeDelete == path)
      return false;
  return true;
}

void DownloadQueue::updateFromBackend() {
	//Try to query backend for list of files
	Backend* backend = BackendManager::getActiveBackend();
	if (backend == nullptr) {
		log_msg("No active backend for Download Queue\n");
		deletedFiles.clear();
		return;
	}
	vector<string>* listFiles = backend->list();
	if(listFiles == nullptr || listFiles->size() == 0) {
	  deletedFiles.clear();
		return;
	}
	//Now we have actully some files to sync(download)
	for(auto it=listFiles->begin();it!=listFiles->end();it++) {
	  if(!shouldDownload(*it))
	    continue;
		push(new SyncEvent(SyncEventType::DOWNLOAD_CONTENT,nullptr,*it));
		push(new SyncEvent(SyncEventType::DOWNLOAD_METADATA,nullptr,*it));
		//log_msg("DOWNLOAD QUEUE: pushed %s Event.\n",it->c_str());
	}
	log_msg("DOWNLOAD QUEUE: Num of Events: %zu .\n",list.size());
	deletedFiles.clear();
}

void DownloadQueue::processDownloadContent(const SyncEvent* _event) {
	if(_event == nullptr || _event->fullPathBuffer.length()==0)
		return;
	FileNode* fileNode = FileSystem::getInstance().getNode(_event->fullPathBuffer);
	//If File exist then we won't download it!
	if(fileNode!=nullptr)
	  return;
	//Ask backend to download the file for us
	Backend *backend = BackendManager::getActiveBackend();
	istream *iStream = backend->get(_event);
	if(iStream == nullptr) {
	  log_msg("Error in Downloading file:%s\n",_event->fullPathBuffer.c_str());
	  return;
	}
	//Now create a file in FS
	//handle directories
	FileNode* parent = FileSystem::getInstance().createHierarchy(_event->fullPathBuffer);
	string fileName = FileSystem::getInstance().getFileNameFromPath(_event->fullPathBuffer);
	FileNode *newFile = FileSystem::getInstance().mkFile(parent, fileName);
	newFile->lockDelete();
	newFile->open();
	newFile->unlockDelete();
	//Make a fake event to check if the file has been deleted
	SyncEvent fakeDeleteEvent(SyncEventType::DELETE,nullptr,_event->fullPathBuffer);
	//and write the content
	char buff[FileSystem::blockSize];
	size_t offset = 0;
	while(iStream->eof() == false) {
	  iStream->read(buff,FileSystem::blockSize);
	  //CheckEvent validity
    if(!UploadQueue::getInstance().checkEventValidity(fakeDeleteEvent)) return;
    //get lock delete so file won't be deleted
    newFile->lockDelete();
	  newFile->write(buff,offset,iStream->gcount());
	  newFile->unlockDelete();
	  offset += iStream->gcount();
	}
	newFile->lockDelete();
	newFile->close();
	printf("DONWLOAD FINISHED:%s\n",newFile->getName().c_str());
	newFile->unlockDelete();
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
		log_msg("No active backend\n");
		return;
	}
	switch (_event->type) {
	case SyncEventType::DOWNLOAD_CONTENT:
		log_msg("Event:DOWNLOAD_CONTENT fullpath:%s\n",
				_event->fullPathBuffer.c_str());
		processDownloadContent(_event);
		break;
	case SyncEventType::DOWNLOAD_METADATA:
		log_msg("Event:DOWNLOAD_METADATA fullpath:%s\n",
				_event->fullPathBuffer.c_str());
		processDownloadMetadata(_event);
		break;
	default:
		log_msg("INVALID Event: file:%s TYPE:%S\n",
				_event->node->getFullPath().c_str(),
				SyncEvent::getEnumString(_event->type).c_str());
	}
}

void DownloadQueue::syncLoop() {
	const long maxDelay = 10000; //Milliseconds
	const long minDelay = 10; //Milliseconds
	long delay = 10; //Milliseconds

	while (running) {
		//Empty list
		if (!list.size()) {
			//log_msg("DOWNLOADQUEUE: I will sleep for %zu milliseconds\n", delay);
			this_thread::sleep_for(chrono::milliseconds(delay));
			delay *= 2;
			if (delay > maxDelay)
				delay = maxDelay;
			//Update list
			updateFromBackend();
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

void DownloadQueue::informDeletedFiles(std::vector<std::string> list) {
  for(string item:list)
    deletedFiles.push_back(item);
}

} /* namespace FUSESwift */
