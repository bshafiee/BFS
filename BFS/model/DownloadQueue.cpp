/*
 * UploadQueue.cpp
 *
 *  Created on: 2014-07-17
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "DownloadQueue.h"
#include <thread>
#include "BackendManager.h"
#include "../log.h"
#include "filesystem.h"
#include <iostream>

using namespace std;

namespace FUSESwift {

//Static members
DownloadQueue* DownloadQueue::mInstance = new DownloadQueue();

DownloadQueue::DownloadQueue():SyncQueue() {
	// TODO Auto-generated constructor stub

}

DownloadQueue::~DownloadQueue() {
	// TODO Auto-generated destructor stub
}

void DownloadQueue::updateFromBackend() {
	//Try to query backend for list of files
	Backend* backend = BackendManager::getActiveBackend();
	if (backend == nullptr) {
		log_msg("No active backend for Download Queue\n");
		return;
	}
	vector<string>* listFiles = backend->list();
	if(listFiles == nullptr || listFiles->size() == 0)
		return;
	//Now we have actully some files to sync(download)
	for(auto it=listFiles->begin();it!=listFiles->end();it++) {
		push(new SyncEvent(SyncEventType::DOWNLOAD_CONTENT,nullptr,*it));
		push(new SyncEvent(SyncEventType::DOWNLOAD_METADATA,nullptr,*it));
		//log_msg("DOWNLOAD QUEUE: pushed %s Event.\n",it->c_str());
	}
	log_msg("DOWNLOAD QUEUE: Num of Events: %zu .\n",list.size());
}

void DownloadQueue::processDownloadContent(SyncEvent* _event) {
	if(_event == nullptr || _event->fullPathBuffer.length()==0)
		return;
	FileNode* fileNode = FileSystem::getInstance()->getNode(_event->fullPathBuffer);
	//If File exist then we won't download it!
	if(fileNode!=nullptr)
	  return;
	//Ask backend to downnload the file for us
	Backend *backend = BackendManager::getActiveBackend();
	istream *iStream = backend->get(_event);
	if(iStream == nullptr) {
	  log_msg("Error in Downloading file:%s\n",_event->fullPathBuffer.c_str());
	  return;
	}
	//Now create a file in FS
	FileNode *newFile = FileSystem::getInstance()->mkFile(_event->fullPathBuffer);
	newFile->open();
	//and write the content
	char buff[FileSystem::blockSize];
	size_t offset = 0;
	while(iStream->eof() == false) {
	  iStream->read(buff,FileSystem::blockSize);
	  newFile->write(buff,offset,iStream->gcount());
	  offset += iStream->gcount();
	}
	newFile->close();
}

void DownloadQueue::processDownloadMetadata(SyncEvent* _event) {
}

DownloadQueue* DownloadQueue::getInstance() {
  return mInstance;
}

void DownloadQueue::syncLoopWrapper() {
  DownloadQueue::getInstance()->syncLoop();
}

void DownloadQueue::startSynchronization() {
	syncThread = new thread(syncLoopWrapper);
}

void DownloadQueue::processEvent(SyncEvent* &_event) {
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
	//Do clean up! delete event
	if(_event != nullptr)
		delete _event;
	_event = nullptr;
}

void DownloadQueue::syncLoop() {
	const long maxDelay = 10000; //Milliseconds
	const long minDelay = 10; //Milliseconds
	long delay = 10; //Milliseconds

	while (true) {
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
		//reset delay
		delay = minDelay;
	}
}

} /* namespace FUSESwift */
