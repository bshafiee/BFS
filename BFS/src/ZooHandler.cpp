/*
 * ZooHandler.cpp
 *
 *  Created on: Sep 22, 2014
 *      Author: behrooz
 */

#include "ZooHandler.h"
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <algorithm>    // std::sort
#include "LeaderOffer.h"
#include "ZooNode.h"
#include "MemoryController.h"
#include "filesystem.h"
#include "DownloadQueue.h"
#include "string.h"
#include "ZeroNetwork.h"
#include "SettingManager.h"
#include "BFSTcpServer.h"
#include "LoggerInclude.h"
#include <Poco/StringTokenizer.h>
#include "Timer.h"



using namespace std;

namespace FUSESwift {

static vector<string> split(const string &s, char delim) {
	vector<string> elems;
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

ZooHandler::ZooHandler() :
		zh(nullptr), sessionState(ZOO_EXPIRED_SESSION_STATE), electionState(
		    ElectionState::FAILED) {
	string str = SettingManager::get(CONFIG_KEY_ZOO_ELECTION_ZNODE);
	if(str.length()>0)
		electionZNode = str;
	else
		LOG(ERROR)<<"No ZOO_ELECTION_ZNODE specified in the config";
	str = SettingManager::get(CONFIG_KEY_ZOO_ASSIGNMENT_ZNODE);
	if(str.length()>0)
		assignmentZNode = str;
	else
	  LOG(ERROR)<<"No ZOO_ASSIGNMENT_ZNODE specified in the config";
	//Set Debug info
	zoo_set_log_stream(stderr);
	zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
}

ZooHandler::~ZooHandler() {
	if (this->zh)
	  free(this->zh);
}

string ZooHandler::sessiontState2String(int state) {
	if (state == 0)
		return "CLOSED_STATE";
	if (state == ZOO_CONNECTING_STATE)
		return "CONNECTING_STATE";
	if (state == ZOO_ASSOCIATING_STATE)
		return "ASSOCIATING_STATE";
	if (state == ZOO_CONNECTED_STATE)
		return "CONNECTED_STATE";
	if (state == ZOO_EXPIRED_SESSION_STATE)
		return "EXPIRED_SESSION_STATE";
	if (state == ZOO_AUTH_FAILED_STATE)
		return "AUTH_FAILED_STATE";

	return "INVALID_STATE";
}

string ZooHandler::zooEventType2String(int state) {
	if (state == ZOO_CREATED_EVENT)
		return "CREATED_EVENT";
	if (state == ZOO_DELETED_EVENT)
		return "DELETED_EVENT";
	if (state == ZOO_CHANGED_EVENT)
		return "CHANGED_EVENT";
	if (state == ZOO_CHILD_EVENT)
		return "CHILD_EVENT";
	if (state == ZOO_SESSION_EVENT)
		return "SESSION_EVENT";
	if (state == ZOO_NOTWATCHING_EVENT)
		return "NOTWATCHING_EVENT";

	return "UNKNOWN_EVENT_TYPE";
}

/*void ZooHandler::dumpStat(const struct Stat *stat) {
	char tctimes[40];
	char tmtimes[40];
	time_t tctime;
	time_t tmtime;

	if (!stat) {
	  LOG(ERROR)<<"null";
		return;
	}
	tctime = stat->ctime / 1000;
	tmtime = stat->mtime / 1000;

	ctime_r(&tmtime, tmtimes);
	ctime_r(&tctime, tctimes);

	LOG(ERROR)<<"ctime = "<<tctimes<<"\tczxid="<<(long long) stat->czxid<<"\n"
			"\tmtime="<<tmtimes<<"\tmzxid="<<(long long) stat->mzxid<<"\n"
			"\tversion="<<(unsigned int) stat->version<<"\taversion="<<
			(unsigned int) stat->aversion<<"\n\tephemeralOwner = "<<
			(long long) stat->ephemeralOwner<<endl;
}*/

void ZooHandler::sessionWatcher(zhandle_t *zzh, int type, int state,
    const char *path, void* context) {
	/* Be careful using zh here rather than zzh - as this may be mt code
	 * the client lib may call the watcher before zookeeper_init returns */
	LOG(DEBUG)<<"Watcher "<<zooEventType2String(type)<<" state = "<<sessiontState2String(state);

	if (type == ZOO_SESSION_EVENT) {
		getInstance().sessionState = state;
		if (state == ZOO_CONNECTED_STATE) {
		  getInstance().myid = zoo_client_id(zzh);
			LOG(DEBUG)<<"Connected Successfully. session id: "<<
			    (long long) getInstance().myid->client_id;
		} else if (state == ZOO_AUTH_FAILED_STATE) {
			LOG(ERROR)<<"Authentication failure. Shutting down...";
			zookeeper_close(zzh);
			getInstance().zh = nullptr;
		} else if (state == ZOO_EXPIRED_SESSION_STATE) {
		  LOG(ERROR)<<"Session expired. Shutting down...! Going to redo election!";
			zookeeper_close(zzh);
			getInstance().zh = nullptr;
			getInstance().startElection();
		}
	}
}

bool ZooHandler::connect() {
	string urlZoo = SettingManager::get(CONFIG_KEY_ZOO_SERVER_URL);
	if(urlZoo.length()>0)
		hostPort = urlZoo;
	else
	  LOG(ERROR)<<"No ZOOKEEPER_SERVER specified in the config.";
	zh = zookeeper_init(hostPort.c_str(), sessionWatcher, 30000, 0, 0, 0);
	if (!zh) {
		return false;
	}

	//Now we can start watching other nodes!
	updateGlobalView();

	return true;
}

int ZooHandler::getSessionState() {
	return sessionState;
}

ZooHandler& ZooHandler::getInstance() {
	static ZooHandler instance;
	return instance;
}

bool ZooHandler::blockingConnect() {
	if (!this->connect())
		return false;
	struct timeval start, now;
	gettimeofday(&now, NULL);
	start = now;
	while (sessionState != ZOO_CONNECTED_STATE
	    && (now.tv_usec - start.tv_usec) <= connectionTimeout) {
		gettimeofday(&now, NULL);
		usleep(10);
	}

	if (sessionState == ZOO_CONNECTED_STATE)
		return true;
	else
		return false;
}

ElectionState ZooHandler::getElectionState() {
	return electionState;
}

std::string ZooHandler::getHostName() {
	char buff[100];
	gethostname(buff, sizeof(buff));
	return string(buff);
}

void ZooHandler::startElection() {
	//First Change state to start
	electionState = ElectionState::START;

	LOG(DEBUG)<<"Starting leader election";

	//Connect to the zoo
	if (!blockingConnect()) {
	  LOG(ERROR)<<"Bootstrapping into the zoo failed. SessionState:"<<
		    sessiontState2String(sessionState);
		electionState = ElectionState::FAILED;
		return;
	}

	//Offer leadership
	bool offerResult = makeOffer();
	if (!offerResult) {
		electionState = ElectionState::FAILED;
		return;
	}

	//Check if we're leader or not
	determineElectionStatus();
}

bool ZooHandler::makeOffer() {
	electionState = ElectionState::OFFER;

	//struct ACL _OPEN_ACL_UNSAFE_ACL[] = {{0x1f, {"world", "anyone"}}};
	//struct ACL_vector ZOO_OPEN_ACL_UNSAFE = { 1, _OPEN_ACL_UNSAFE_ACL};
	char newNodePath[1000];
	/* Create Election Base node
	 * zoo_create(zh,(electionZNode).c_str(),"Election Offers",
	 strlen("Election Offers"),&ZOO_OPEN_ACL_UNSAFE ,0,newNodePath,newNodePathLen);*/
	//ZooNode
	//vector<string> listOfFiles = FileSystem::getInstance().listFileSystem(false);
	vector<string> listOfFiles;//Empty list of files
	//Create a ZooNode
#ifdef BFS_ZERO
	ZooNode zooNode(getHostName(),
	    MemoryContorller::getInstance().getAvailableMemory(), listOfFiles,BFSNetwork::getMAC(),BFSTcpServer::getIP(),BFSTcpServer::getPort());
#else
	ZooNode zooNode(getHostName(),
	      MemoryContorller::getInstance().getAvailableMemory(), listOfFiles,BFSNetwork::getMAC(),BFSTcpServer::getIP(),BFSTcpServer::getPort());
#endif
	//Send data
	string str = zooNode.toString();
	int result = zoo_create(zh, (electionZNode + "/" + "n_").c_str(), str.c_str(),
	    str.length(), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL | ZOO_SEQUENCE,
	    newNodePath, sizeof(newNodePath));
	if (result != ZOK) {
	  LOG(ERROR)<<"zoo_create failed:"<<zerror(result);
		return false;
	}

	leaderOffer = LeaderOffer(string(newNodePath), getHostName());

	LOG(DEBUG)<<"Created leader offer: "<<leaderOffer.toString();

	return true;
}

bool ZooHandler::determineElectionStatus() {
	electionState = ElectionState::DETERMINE;

	vector<string> components = split(leaderOffer.getNodePath(), nodeDelimitter);

	//Extract Id from nodepath
	string sequenceNum = components[components.size() - 1];
	int id = std::stoi(sequenceNum.substr(strlen("n_")));
	leaderOffer.setId(id);

	//Get List of children of /BFSElection
	String_vector children;
	int callResult = zoo_get_children(zh, electionZNode.c_str(), 1, &children);
	if (callResult != ZOK) {
	  LOG(ERROR)<<"zoo_get_children failed:"<<zerror(callResult);
		return false;
	}
	vector<string> childrenVector;
	for (int i = 0; i < children.count; i++)
		childrenVector.push_back(string(children.data[i]));

	vector<LeaderOffer> leaderOffers = toLeaderOffers(childrenVector);

	/*
	 * For each leader offer, find out where we fit in. If we're first, we
	 * become the leader. If we're not elected the leader, attempt to stat the
	 * offer just less than us. If they exist, watch for their failure, but if
	 * they don't, become the leader.
	 */

	for (unsigned int i = 0; i < leaderOffers.size(); i++) {
		LeaderOffer leaderOffer = leaderOffers[i];

		if (leaderOffer.getId() == this->leaderOffer.getId()) {
		  LOG(DEBUG)<<"There are "<<leaderOffers.size()<<
		      " leader offers. I am "<<i<<" in line.";

			if (i == 0) {
				becomeLeader();
			} else {
				becomeReady(leaderOffers[i - 1]);
			}

			//Update global View
			updateGlobalView();
			fetchAssignmets();

			// Once we've figured out where we are, we're done.
			break;
		}
	}
	return true;
}

vector<LeaderOffer> ZooHandler::toLeaderOffers(const vector<string> &children) {
	vector<LeaderOffer> leaderOffers;

	/*
	 * Turn each child of rootNodeName into a leader offer. This is a tuple of
	 * the sequence number and the node name.
	 */
	for (string offer : children) {
		char buffer[1000];
		int len = -1;
		int callResult = zoo_get(zh, (electionZNode + "/" + offer).c_str(), 0,
		    buffer, &len, nullptr);
		if (callResult != ZOK) {
			LOG(ERROR)<<"zoo_get:"<<zerror(callResult);
			continue;
		}

		buffer[len - 1] = '\0';
		string hostName = string(buffer);
		int id = std::stoi(offer.substr(strlen("n_")));
		leaderOffers.push_back(
		    LeaderOffer(id, electionZNode + "/" + offer, hostName));
	}

	/*
	 * We sort leader offers by sequence number (which may not be zero-based or
	 * contiguous) and keep their paths handy for setting watches.
	 */
	std::sort(leaderOffers.begin(), leaderOffers.end(), LeaderOffer::Comparator);

	/*
	 * test them
	 for(LeaderOffer offer:leaderOffers)
	 printf("%ld\n",offer.getId());
	 */

	return leaderOffers;
}

void ZooHandler::becomeLeader() {
	electionState = ElectionState::LEADER;
	LOG(DEBUG)<<"Becoming leader with node:"<<leaderOffer.toString();
	MasterHandler::startLeadership();
	//Commit a publish command!(no matter leader or ready!)
	publishListOfFiles();
}

void ZooHandler::becomeReady(LeaderOffer neighborLeaderOffer) {
  LOG(DEBUG)<<leaderOffer.getNodePath()<<
      " not elected leader, Watching node:"<<neighborLeaderOffer.getNodePath();

	/*
	 * Make sure to pass an explicit Watcher because we could be sharing this
	 * zooKeeper instance with someone else.
	 */
	struct Stat stat;
	int callResult = zoo_wexists(zh, neighborLeaderOffer.getNodePath().c_str(),
	    neighbourWatcher, nullptr, &stat);
	if (callResult != ZOK) {
		electionState = ElectionState::FAILED;
		/*
		 * If the stat fails, the node has gone missing between the call to
		 * getChildren() and exists(). We need to try and become the leader.
		 */
		LOG(ERROR)<<"We were behind "<<neighborLeaderOffer.getNodePath()<<
		    " but it looks like they died. Back to determination.";
		determineElectionStatus();
		return;
	}

	//Finally if everything went ok we become ready
	electionState = ElectionState::READY;
	//Commit a publish command!(no matter leader or ready!)
	publishListOfFiles();
}

void ZooHandler::neighbourWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
	if (type == ZOO_DELETED_EVENT) {
		string pathStr(path);
		if (pathStr != getInstance().leaderOffer.getNodePath()
		    && getInstance().electionState != ElectionState::STOP) {
		  LOG(ERROR)<<"Node "<<path<<" deleted. Need to run through the election process.";

			if (!getInstance().determineElectionStatus())
				getInstance().electionState = ElectionState::FAILED;
		}
	}
}

void ZooHandler::publishListOfFiles() {
	if (sessionState != ZOO_CONNECTED_STATE
	    || (electionState != ElectionState::LEADER
	        && electionState != ElectionState::READY)) {
		LOG(ERROR)<<"publishListOfFiles(): invalid sessionstate or electionstate";
		return;
	}

	lock_guard<mutex> lk(lockPublish);
	vector<string> listOfFiles = FileSystem::getInstance().listFileSystem(false);

	bool change = false;
	for(string file:listOfFiles){
	  bool found = false;
	  for(string cacheFileName:cacheFileList)
	    if(cacheFileName == file){
	      found = true;
	      break;
	    }
	  if(!found){
	    change = true;
	    break;
	  }
	}
	if(!change && listOfFiles.size() == cacheFileList.size()){
	  return;
	}

	cacheFileList = listOfFiles;

	//Create a ZooNode
#ifdef BFS_ZERO
	ZooNode zooNode(getHostName(),
	    MemoryContorller::getInstance().getAvailableMemory(), listOfFiles,BFSNetwork::getMAC(),BFSTcpServer::getIP(),BFSTcpServer::getPort());
#else
  ZooNode zooNode(getHostName(),
      MemoryContorller::getInstance().getAvailableMemory(), listOfFiles,BFSNetwork::getMAC(),BFSTcpServer::getIP(),BFSTcpServer::getPort());
#endif

	//Send data
	string str = zooNode.toString();
	int callRes = zoo_set(zh, leaderOffer.getNodePath().c_str(), str.c_str(),
	    str.length(), -1);

	if (callRes != ZOK) {
	  LOG(ERROR)<<"publishListOfFiles(): zoo_set failed:"<< zerror(callRes);
		return;
	}

	LOG(DEBUG)<<"publishListOfFiles successfully:"<<str;
}

std::vector<ZooNode> ZooHandler::getGlobalView() {
  lock_guard<mutex> lk(lockGlobalView);
	return globalView;
}

/**
 * 1)get list of electionznode(/BFSElection) children
 * 	 and set a watch for changes in these folder
 * 2)get content of each node and set watch on them
 * 3)parse nodes content to znode
 * 4)update globalView
 **/
void ZooHandler::updateGlobalView() {
  lock_guard<mutex> lk(lockGlobalView);

	if (sessionState != ZOO_CONNECTED_STATE
	    || (electionState != ElectionState::LEADER
	        && electionState != ElectionState::READY)) {
		LOG(ERROR)<<"updateGlobalView(): invalid sessionstate or electionstate";
		return;
	}

	//Invalidate previous globaView!
	globalView.clear();
	//1)get list of (electionznode)/BFSElection children and set a watch for changes in these folder
	String_vector children;
	int callResult = zoo_wget_children(zh, electionZNode.c_str(), electionFolderWatcher,nullptr, &children);
	if (callResult != ZOK) {
	  LOG(ERROR)<<"updateGlobalView(): zoo_wget_children failed:"<<zerror(callResult);
		return;
	}
	//2)get content of each node and set watch on them
	for (int i = 0; i < children.count; i++) {
		string node(children.data[i]);
		//Allocate 1MB data
		const int length = 1024 * 1024;
		char *buffer = new char[length];
		int len = length;
		int callResult = zoo_wget(zh, (electionZNode + "/" + node).c_str(),
		    nodeWatcher, nullptr, buffer, &len, nullptr);
		if (callResult != ZOK) {
			LOG(ERROR)<<"zoo_wget failed:"<<zerror(callResult);
			delete[] buffer;
			buffer = nullptr;
			continue;
		}
		if(len >= 0 && len <= length-1)
			buffer[len] = '\0';
		Poco::StringTokenizer tokenizer(buffer,"\n",
		    Poco::StringTokenizer::TOK_TRIM |
		    Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		if(tokenizer.count() < 5) {
		  LOG(ERROR)<<"Malformed data at:"<<node<<" Buffer:"<<buffer;
		  continue;
		}

		//3)parse node content to a znode
		vector<string> nodeFiles;

		//3.1 Hostname
		string hostName = tokenizer[0];

		//3.2 MAC String
		unsigned char mac[6];
		sscanf(tokenizer[1].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2],
				&mac[3], &mac[4], &mac[5]);

		//3.3 ip
    string ip(tokenizer[2]);

    //3.4 port
    string port(tokenizer[3]);

    //3.1 freespace
		string freeSize(tokenizer[4]);

		//3.1 files
		for(uint i=5;i<tokenizer.count();i++)
			nodeFiles.push_back(tokenizer[i]);

		//4)update globalView
		ZooNode zoonode(hostName, std::stol(freeSize), nodeFiles,mac,ip,stoul(port));
		globalView.push_back(zoonode);
		delete[] buffer;
		buffer = nullptr;
	}

	/*string glob;
	for(ZooNode node:globalView)
	  glob+= "{"+node.toString()+"}\n";
	LOG(ERROR)<<"GLOBAL VIEW UPDATED:"<<glob<<endl;*/

	//Now we have a fresh globalView! So update list of remote files if our FS!
	updateRemoteFilesInFS();
}

void ZooHandler::updateRemoteFilesInFS() {
	vector<pair<string,ZooNode>> newRemoteFiles;
	vector<string>localFiles = FileSystem::getInstance().listFileSystem(true);
	for(ZooNode node:globalView){
		//We should not include ourself in this
		const unsigned char* myMAC = BFSNetwork::getMAC();
		bool isMe = true;
		for(int i=0;i<6;i++)
			if(node.MAC[i]!=myMAC[i]){
				isMe = false;
				break;
			}
		if(isMe)
			continue;
		for(string remoteFile:node.containedFiles){
			bool exist = false;
			for(string localFile: localFiles)
				if(localFile == remoteFile){
					exist = true;
					break;
				}
			if(!exist)
				newRemoteFiles.push_back(make_pair(remoteFile,node));
		}
	}

	for(pair<string,ZooNode> item: newRemoteFiles){
		FileNode* fileNode = FileSystem::getInstance().findAndOpenNode(item.first);
		//If File exist then we won't create it!
		if(fileNode!=nullptr){
		  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fileNode);
		  fileNode->close(inodeNum);
			continue;
		}
		//Now create a file in FS
		string fileName = FileSystem::getInstance().getFileNameFromPath(item.first);
		FileNode *newFile = FileSystem::getInstance().mkFile(fileName,true,true);
		uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)newFile);
		newFile->setRemoteHostMAC(item.second.MAC);
		newFile->setRemoteIP(item.second.ip);
		newFile->setRemotePort(item.second.port);
		newFile->close(inodeNum);//create and open operation
		char macCharBuff[100];
		sprintf(macCharBuff,"MAC:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		    item.second.MAC[0],item.second.MAC[1],
		    item.second.MAC[2],item.second.MAC[3],
        item.second.MAC[4],item.second.MAC[5]);
		LOG(DEBUG)<<"created:"<<item.first<<" hostName:"<<
		    item.second.hostName<<" "<<string(macCharBuff);
	}

	//Now remove the localRemoteFiles(remote files which have a pointer in our
	// fs locally) which don't exist anymore
	for(string fileName:localFiles) {
	  FileNode* file = FileSystem::getInstance().findAndOpenNode(fileName);
	  if(file == nullptr){
	    LOG(ERROR)<<"ERROR, cannot find corresponding node in filesystem for:"<<fileName;
	    continue;
	  }
	  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)file);

	  if(!file->isRemote()){
	    file->close(inodeNum);
	    continue;
	  }
          
	  file->close(inodeNum);

	  bool exist = false;
	  for(ZooNode node:globalView){
      //We should not include ourself in this
      const unsigned char* myMAC = BFSNetwork::getMAC();
      bool isMe = true;
      for(int i=0;i<6;i++)
        if(node.MAC[i]!=myMAC[i]){
          isMe = false;
          break;
        }
      if(isMe)
        continue;

      for(string remoteFile:node.containedFiles){
        if(remoteFile == fileName){
          exist = true;
          break;
        }
      }
      if(exist)
        break;
	  }
	  if(!exist) {
	    LOG(DEBUG)<<"ZOOOOHANDLER GOING TO REMOVE:"<<file->getFullPath();
	    //fflush(stderr);
	    FileSystem::getInstance().signalDeleteNode(file,false);
	  }
	}

}

void ZooHandler::nodeWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
	if (type == ZOO_CHANGED_EVENT) {
		string pathStr(path);
		LOG(DEBUG)<<"Node "<<path<<" changed! updating globalview...";
		getInstance().updateGlobalView();
	}
}

void ZooHandler::electionFolderWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
	if (type == ZOO_CHILD_EVENT) {
		string pathStr(path);
		//printf("Children of Election folder changed: %s . updating globalview...\n", path);
		getInstance().updateGlobalView();
	}
	getInstance().determineElectionStatus();
}

/**
 * 1)Check election/connection state
 * 2)determine your host-name and check assignmnetZnode/hostname
 * 3)fetch content of assignmnetZnode/hostname & set watch on it
 * 4)Ask DownloadQueue to download them in case we don't have them
 */
void ZooHandler::fetchAssignmets() {
	if (sessionState != ZOO_CONNECTED_STATE
			|| (electionState != ElectionState::LEADER
					&& electionState != ElectionState::READY)) {
		LOG(ERROR)<<"Invalid sessionstate or electionstate";
		return;
	}

	string path = assignmentZNode+"/"+getHostName();
	//Allocate 1MB data
	const int length = 1024 * 1024;
	char *buffer = new char[length];
	int len = length;
	int callResult =
			zoo_wget(zh, path.c_str(), assignmentWatcher, nullptr, buffer, &len, nullptr);
	if (callResult != ZOK) {
		if(callResult == ZNONODE) //No node! needs to set a watch on create
			zoo_wexists(zh,path.c_str(),assignmentWatcher,nullptr,nullptr);
		LOG(DEBUG)<<"zoo_wget failed:"<<zerror(callResult);
		delete[] buffer;
		buffer = nullptr;
		return;
	}

	if(len >= 0 && len <= length-1)
		buffer[len] = '\0';
	vector<string> assignments;
	//Read line by line
	char *tok = strtok(buffer, "\n");
	while (tok != NULL) {
		string file(tok);
		assignments.push_back(file);
		tok = strtok(NULL, "\n");
	}
	//Release Memory
	delete[] buffer;
	buffer = nullptr;

	//Tell DownloadQueue
	DownloadQueue::getInstance().addZooTask(assignments);
}

void ZooHandler::assignmentWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
	if (type == ZOO_CHANGED_EVENT || type == ZOO_CREATED_EVENT) {
		string pathStr(path);
		LOG(DEBUG)<<"Node "<<path<<" changed! updating assignments...";
		getInstance().fetchAssignmets();
	}
}

ZooNode ZooHandler::getMostFreeNode() {
  updateGlobalView();
  vector<ZooNode>globalView = getGlobalView();
  if(globalView.size() == 0) {
    vector<string> emptyVector;
    ZooNode emptyNode(string(""),0,emptyVector,nullptr,"",0);
    return emptyNode;
  }
  //Now sort ourZoo by Free Space descendingly!
  std::sort(globalView.begin(),globalView.end(),ZooNode::CompByFreeSpaceDes);
  return globalView.front();
}

void ZooHandler::requestUpdateGlobalView() {
  updateGlobalView();
}

void ZooHandler::stopZooHandler() {
  sessionState = ZOO_EXPIRED_SESSION_STATE;
  zookeeper_close(zh);
  zh = nullptr;
  LOG(INFO)<<"ZOOHANDLER LOOP DEAD!";
}

}	//Namespace

