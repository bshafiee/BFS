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
#include "log.h"
#include "filesystem.h"
#include <iostream>
#include <mutex>
#include "MemoryController.h"
#include "LoggerInclude.h"

using namespace std;

namespace FUSESwift {

DownloadQueue::DownloadQueue():SyncQueue() {
	// TODO Auto-generated constructor stub

}

DownloadQueue::~DownloadQueue() {
}

void DownloadQueue::addZooTask(vector<string>assignments) {
	//Try to query backend for list of files
	Backend* backend = BackendManager::getActiveBackend();
	if (backend == nullptr) {
		log_msg("No active backend for Download Queue\n");
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

void DownloadQueue::processDownloadContent(const SyncEvent* _event) {
	if(_event == nullptr || _event->fullPathBuffer.length()==0)
		return;
	FileNode* fileNode = FileSystem::getInstance().findAndOpenNode(_event->fullPathBuffer);
	//If File exist then we won't download it!
	if(fileNode!=nullptr){
	  //Close it! so it can be removed if needed
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fileNode);
    fileNode->close(inodeNum);
	  return;
	}
	//Ask backend to download the file for us
	Backend *backend = BackendManager::getActiveBackend();
	istream *iStream = backend->get(_event);
	if(iStream == nullptr) {
	  log_msg("Error in Downloading file:%s\n",_event->fullPathBuffer.c_str());
	  return;
	}
	//Now create a file in FS
	//handle directories
  FileSystem::getInstance().createHierarchy(_event->fullPathBuffer);
	FileNode *newFile = FileSystem::getInstance().mkFile(_event->fullPathBuffer,false,true);//open
	if(newFile == nullptr){
	  LOG(ERROR)<<"Failed to create a newNode:"<<_event->fullPathBuffer;
	  fprintf(stderr,"Failed to create a newNode:%s\n",_event->fullPathBuffer.c_str());
	  return;
	}
	uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)newFile);
	fprintf(stderr,"DOWNLOADING: %s\n",newFile->getFullPath().c_str());

	//Make a fake event to check if the file has been deleted
	//SyncEvent fakeDeleteEvent(SyncEventType::DELETE,nullptr,_event->fullPathBuffer);
	//and write the content
	char buff[FileSystem::blockSize];//TODO increase this
	size_t offset = 0;
	while(iStream->eof() == false) {
	  iStream->read(buff,FileSystem::blockSize);

	  if(newFile->mustBeDeleted()){
	    newFile->close(inodeNum);
	    return;
	  }

	  FileNode* afterMove = nullptr;
    int retCode = newFile->writeHandler(buff,offset,iStream->gcount(),afterMove);
    if(afterMove)
      newFile = afterMove;
    while(retCode == -1)//-1 means moving
      retCode = newFile->writeHandler(buff,offset,iStream->gcount(),afterMove);

    //Check space availability
	  if(retCode < 0) {
	    log_msg("Error in writing file:%s, probably no diskspace, Code:%d\n",newFile->getFullPath().c_str(),retCode);
	    newFile->close(inodeNum);
	    return;
	  }

	  offset += iStream->gcount();
	}
	newFile->setNeedSync(false);//We have just created this file so it's upload flag false
	newFile->close(inodeNum);
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
				_event->fullPathBuffer.c_str(),
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

} /* namespace FUSESwift */
