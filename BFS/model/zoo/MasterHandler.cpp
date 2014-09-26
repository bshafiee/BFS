/*
 * MasterHandler.cpp
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#include "MasterHandler.h"
#include "ZooHandler.h"
#include <thread>
#include <algorithm>
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
    //2)Check if there is any new change
    //TODO implement this...
    if(fileList != nullptr) {
      delete fileList;
      fileList = nullptr;
    }
    fileList  = backend->list();
    //3)Fetch list of avail nodes, their free space
    //4)Divide tasks among nodes
		//5)Clean all previous assignments
		//6)Publish assignments
		bool change = divideTaskAmongNodes();

    //Adaptive sleep
		if(!change)
			interval *= 10;
		if(interval > maxSleep)
			interval = maxSleep;
    usleep(interval);
  }
}

MasterHandler::~MasterHandler() {
	if(fileList != nullptr) {
		delete fileList;
		fileList = nullptr;
	}
}

void MasterHandler::startLeadership() {
  isRunning = true;
  new thread(leadershipLoop);
}

void MasterHandler::stopLeadership() {
  isRunning = false;
}

/**
 * Things to be considered for assignment
 * Load per node!
 * Free space on each node
 * popularity of each file
 *
 * return true if a new assignment happens
 */
bool MasterHandler::divideTaskAmongNodes() {
	//1)First check which files already exist in nodes
	for(auto iter = fileList->begin(); iter != fileList->end();) {
		bool found = false;
		for(ZooNode node : ZooHandler::getInstance().globalView) {
			for(string file:node.containedFiles)
				if(file == iter->name) {
					found = true;
					break;
				}
			if(found)
				break;
		}
		//Remove the file from list of files if already found in any of nodes
		if(found)
			iter = fileList->erase(iter);
		else
			++iter;
	}
	//Now FileList contains files which don't exist at any node!
	if(fileList->size() == 0){//Nothing to do :) Bye!
		//printf("Filelist empty\n");
		return false;
	}

	//Now make a list of znode without their filelist overhead
	//to figureout what to assign to which node
	vector<ZooNode> ourZoo;
	for(ZooNode node:ZooHandler::getInstance().globalView)
		ourZoo.push_back(ZooNode(node.hostName,node.freeSpace,vector<string>()));
	if(ourZoo.size() == 0)//Nothing to do if we don't have any node yet!
		return false;
	//Now sort ourZoo by Free Space descendingly!
	std::sort(ourZoo.begin(),ourZoo.end(),ZooNode::CompByFreeSpaceDes);
	//Now sort fileList by their size descendingly!
	std::sort(fileList->begin(),fileList->end(),BackendItem::CompBySizeDes);
	//No Compute Average freespace, total free space and total amount of required
	unsigned long totalFree = 0;
	unsigned long totalRequired = 0;
	unsigned long avgFree = 0;
	for(ZooNode node:ourZoo)
		totalFree += node.freeSpace;
	avgFree = totalFree / ourZoo.size();
	for(BackendItem bItem: *fileList)
		totalRequired += bItem.length;
	//Where to keep results <node,list of assignments>
	vector<pair<string,vector<string> > > assignments;
	for(ZooNode node:ourZoo) {
		pair<string,vector<string> > task;
		task.first = node.hostName;
		for(auto iter=fileList->begin();iter != fileList->end();) {
			if(iter->length < node.freeSpace) {
				node.freeSpace -= iter->length;
				task.second.push_back(iter->name);
				iter = fileList->erase(iter);
			}
			else
				iter++;
		}
		assignments.push_back(task);
	}
	//having all task determined we need to publish them!
	for(pair<string,vector<string>> item: assignments) {
		string path = ZooHandler::getInstance().assignmentZNode+"/"+item.first;
		string value = "";
		for(unsigned int i=0;i<item.second.size();i++) {
			if(i != item.second.size()-1)
				value += item.second[i]+"\n";
			else
				value += item.second[i];
		}

		//First try to set if does not exist create it!
		int callRes = zoo_set(ZooHandler::getInstance().zh,path.c_str(),value.c_str(),value.length(),-1);
		if(callRes == ZNONODE) {//Does not exist!
			char buffer[100];
			strcpy(buffer,path.c_str());
			int createRes = zoo_create(ZooHandler::getInstance().zh,path.c_str(),value.c_str(),
																 value.length(),&ZOO_OPEN_ACL_UNSAFE,0,buffer,sizeof(buffer));
			if(createRes != ZOK)
				fprintf(stderr, "divideTaskAmongNodes(): zoo_create failed:%s\n",zerror(createRes));
				continue;
		} else if(callRes != ZOK) {
			fprintf(stderr, "divideTaskAmongNodes(): zoo_set failed:%s\n",zerror(callRes));
			continue;
		}

		//printf("Published tasks for %s:{%s}\n",item.first.c_str(),value.c_str());
	}
	return true;
}

} /* namespace FUSESwift */
