/*
 * MasterHandler.cpp
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#include <thread>
#include <algorithm>
#include <iostream>
#include "MasterHandler.h"
#include "ZooHandler.h"

using namespace std;

namespace FUSESwift {

atomic<bool> MasterHandler::isRunning;
vector<ZooNode> MasterHandler::existingNodes;

MasterHandler::MasterHandler() {
}


template<typename T>
static bool contains(const vector<T> &vec,const T &item) {
	for(T elem:vec)
		if(elem == item)
			return true;
	return false;
}
template static bool contains<ZooNode>(const vector<ZooNode> &vec,const ZooNode &item);

void MasterHandler::removeDuplicates(vector<BackendItem> &newList,vector<BackendItem> &oldList) {
	for(auto iter=newList.begin();iter!=newList.end();) {
		bool found = false;
		for(BackendItem item:oldList)
			if(iter->name == item.name)
				found = true;
		if(found)
			iter = newList.erase(iter);
		else
			iter++;
	}
}

vector<BackendItem> MasterHandler::getExistingAssignments() {
	vector<BackendItem> result;
	if (ZooHandler::getInstance().sessionState != ZOO_CONNECTED_STATE) {
		printf("getExistingAssignments(): invalid sessionstate \n");
		return result;
	}

	//1)get list of (assignmentznode)/BFSElection children and set a watch for changes in these folder
	String_vector children;
	int callResult = zoo_get_children(ZooHandler::getInstance().zh, ZooHandler::getInstance().assignmentZNode.c_str(),0, &children);
	if (callResult != ZOK) {
		printf("getExistingAssignments(): zoo_get_children failed:%s\n",
				zerror(callResult));
		return result;
	}
	//2)get content of each node
	for (int i = 0; i < children.count; i++) {
		string node(children.data[i]);
		//Allocate 1MB data
		const int length = 1024 * 1024;
		char *buffer = new char[length];
		int len = length;
		int callResult =
					zoo_get(ZooHandler::getInstance().zh,
						(ZooHandler::getInstance().assignmentZNode + "/" + node).c_str(),
						0, buffer, &len, nullptr);
		if (callResult != ZOK) {
			printf("getExistingAssignments(): zoo_get:%s\n", zerror(callResult));
			delete[] buffer;
			buffer = nullptr;
			continue;
		}
		if(len >= 0 && len <= length-1)
			buffer[len] = '\0';

		//3)parse node content to a znode
		char *tok = strtok(buffer, "\n");
		while (tok != NULL) {
			string file(tok);
			result.push_back(BackendItem(file,-1l,"",""));
			tok = strtok(NULL, "\n");
		}
		//Release memory
		delete[] buffer;
		buffer = nullptr;
	}

	return result;
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
      continue;
    }
    vector<BackendItem> *backendList = backend->list();
    if(backendList == nullptr) {
    	printf("leadershipLoop(): backendList is null!\n");
			interval *= 10;
			if(interval > maxSleep)
				interval = maxSleep;
			usleep(interval);
			continue;
    }


    //3)Fetch list of avail nodes, their free space
    vector<ZooNode> globalView = ZooHandler::getInstance().getGlobalView();
    //Check if there is any change in nodes(only checks freespace name and mac)
    bool nodesChanged = false;
    if(globalView.size()!=existingNodes.size()) {
      //Get rid of already assigned ones because some of them might be dead
      cleanAssignmentFolder();
      nodesChanged = true;
    }
    else {
      for(ZooNode node:globalView)
        if(!contains<ZooNode>(existingNodes,node)) {
          nodesChanged = true;
          break;
        }
    }

    //2)Check if there is any new change or any change in nodes!
    vector<BackendItem> oldAssignments = getExistingAssignments();
    //cerr<<"oldAssignments:";printVector(oldAssignments);
    if(oldAssignments.size() > 0)
      removeDuplicates(*backendList,oldAssignments);

    if(nodesChanged) { //Get a copy in existing nodes
      existingNodes.clear();
      existingNodes.insert(existingNodes.end(),globalView.begin(),globalView.end());
    }

    //4)Divide tasks among nodes
		//5)Clean all previous assignments
		//6)Publish assignments
		bool change = false;
		if(backendList->size() || nodesChanged)
			change = divideTaskAmongNodes(backendList,globalView);

		//Release Memory
		backendList->clear();
		delete backendList;

    //Adaptive sleep
		if(!change)
			interval *= 10;
		if(interval > maxSleep)
			interval = maxSleep;
    usleep(interval);
  }
  LOG(ERROR)<<"MASTERHANDLER LOOP DEAD!";
}

MasterHandler::~MasterHandler() {
}

void MasterHandler::startLeadership() {
	if(isRunning)
		return;
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
bool MasterHandler::divideTaskAmongNodes(std::vector<BackendItem> *listFiles,vector<ZooNode> &globalView) {
	//1)First check which files already exist in nodes
	for(auto iter = listFiles->begin(); iter != listFiles->end();) {
		bool found = false;
		for(ZooNode node : globalView) {
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
			iter = listFiles->erase(iter);
		else
			++iter;
	}
	//Now FileList contains files which don't exist at any node!
	if(listFiles->size() == 0){//Nothing to do :) Bye!
		//printf("Filelist empty\n");
		return false;
	}

	if(globalView.size() == 0)//Nothing to do if we don't have any node yet!
		return false;
	//Now get rid of filelist overhead in global view
	//to figureout what to assign to which node
	for(ZooNode node:globalView)
		node.containedFiles.clear();
	//Now sort ourZoo by Free Space descendingly!
	std::sort(globalView.begin(),globalView.end(),ZooNode::CompByFreeSpaceDes);
	//Now sort fileList by their size descendingly!
	std::sort(listFiles->begin(),listFiles->end(),BackendItem::CompBySizeDes);
	//No Compute Average freespace, total free space and total amount of required
	unsigned long totalFree = 0;
	unsigned long totalRequired = 0;
	//unsigned long avgFree = 0;
	for(ZooNode node:globalView)
		totalFree += node.freeSpace;
	//avgFree = totalFree / ourZoo.size();
	for(BackendItem bItem: *listFiles)
		totalRequired += bItem.length;
	//Where to keep results <node,list of assignments>
	vector<pair<string,vector<string> > > assignments;
	//A flag to indicate if we could assign anything new
	bool couldAssign = false;
	for(ZooNode node:globalView) {
		pair<string,vector<string> > task;
		task.first = node.hostName;
		for(auto iter=listFiles->begin();iter != listFiles->end();) {
			if(iter->length < node.freeSpace) {
				node.freeSpace -= iter->length;
				task.second.push_back(iter->name);
				iter = listFiles->erase(iter);
				couldAssign = true;//an assignment happened :)
			}
			else {
			  //fprintf(stderr,"%s can't be assigned to any node.\n",iter->name.c_str());
				iter++;
			}
		}
		assignments.push_back(task);
	}

	//First Delete all existing assignment nodes
	if(!cleanAssignmentFolder()){
	  fprintf(stderr, "divideTaskAmongNodes(): cleaning up assignment folder failed:\n");
	  fflush(stderr);
	  return true;//reschedule
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
		//int callRes = zoo_set(ZooHandler::getInstance().zh,path.c_str(),value.c_str(),value.length(),-1);
		//if(callRes == ZNONODE) {//Does not exist!
			char buffer[1000];
			strcpy(buffer,path.c_str());
			int createRes = zoo_create(ZooHandler::getInstance().zh,path.c_str(),value.c_str(),
																 value.length(),&ZOO_OPEN_ACL_UNSAFE,ZOO_EPHEMERAL,buffer,sizeof(buffer));
			if(createRes != ZOK){
				fprintf(stderr, "divideTaskAmongNodes(): zoo_create failed:%s\n",zerror(createRes));
				continue;
			}
//		} else if(callRes != ZOK) {
//			fprintf(stderr, "divideTaskAmongNodes(): zoo_set failed:%s\n",zerror(callRes));
//			continue;
//		}

		//printf("Published tasks for %s:{%s}\n",item.first.c_str(),value.c_str());
	}
	return couldAssign;
}

bool MasterHandler::cleanAssignmentFolder() {
  String_vector children;
  int callResult = zoo_wget_children(ZooHandler::getInstance().zh,
      ZooHandler::getInstance().assignmentZNode.c_str(),
      nullptr,nullptr, &children);
  if (callResult != ZOK) {
    printf("cleanAssignmentFolder(): zoo_wget_children failed:%s\n",
        zerror(callResult));
    return false;
  }

  zoo_op_t *opArr = new zoo_op[children.count];
  for(int i = 0;i<children.count;i++) {
    zoo_op_t op;
    string path = ZooHandler::getInstance().assignmentZNode + "/" +string(children.data[i]);
    zoo_delete_op_init(&op,path.c_str(),-1);//No versioning
    opArr[i] = op;
  }

/*  for(int i=0;i<children.count;i++)
    fprintf(stderr, "removing:%s\n",opArr[i].delete_op.path);*/

  zoo_op_result_t *results = new zoo_op_result_t[children.count];

  //int res = zoo_multi(ZooHandler::getInstance().zh,children.count,opArr,results);
  for(int i = 0;i<children.count;i++) {
    string path = ZooHandler::getInstance().assignmentZNode + "/" +string(children.data[i]);
    int res = zoo_delete(ZooHandler::getInstance().zh,path.c_str(),-1);
    if(res != ZOK)
      return false;
  }

  delete []opArr;
  delete []results;
  //return res == ZOK;
  return true;
}

} /* namespace FUSESwift */
