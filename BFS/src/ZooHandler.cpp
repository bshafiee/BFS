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
		fprintf(stderr,"No ZOO_ELECTION_ZNODE specified in the config.\n");
	str = SettingManager::get(CONFIG_KEY_ZOO_ASSIGNMENT_ZNODE);
	if(str.length()>0)
		assignmentZNode = str;
	else
		fprintf(stderr,"No ZOO_ASSIGNMENT_ZNODE specified in the config.\n");
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

void ZooHandler::dumpStat(const struct Stat *stat) {
	char tctimes[40];
	char tmtimes[40];
	time_t tctime;
	time_t tmtime;

	if (!stat) {
		fprintf(stderr, "null\n");
		return;
	}
	tctime = stat->ctime / 1000;
	tmtime = stat->mtime / 1000;

	ctime_r(&tmtime, tmtimes);
	ctime_r(&tctime, tctimes);

	fprintf(stderr, "\tctime = %s\tczxid=%llx\n"
			"\tmtime=%s\tmzxid=%llx\n"
			"\tversion=%x\taversion=%x\n"
			"\tephemeralOwner = %llx\n", tctimes, (long long) stat->czxid, tmtimes,
	    (long long) stat->mzxid, (unsigned int) stat->version,
	    (unsigned int) stat->aversion, (long long) stat->ephemeralOwner);
}

void ZooHandler::sessionWatcher(zhandle_t *zzh, int type, int state,
    const char *path, void* context) {
	/* Be careful using zh here rather than zzh - as this may be mt code
	 * the client lib may call the watcher before zookeeper_init returns */
	fprintf(stderr, "Watcher %s state = %s", zooEventType2String(type).c_str(),
	    sessiontState2String(state).c_str());
	if (path && strlen(path) > 0) {
		fprintf(stderr, " for path %s", path);
	}
	fprintf(stderr, "\n");

	if (type == ZOO_SESSION_EVENT) {
		getInstance().sessionState = state;
		if (state == ZOO_CONNECTED_STATE) {
			const clientid_t *id = zoo_client_id(zzh);
			getInstance().myid = *id;
			fprintf(stderr, "Connected Successfully. session id: 0x%llx\n",
			    (long long) getInstance().myid.client_id);
		} else if (state == ZOO_AUTH_FAILED_STATE) {
			fprintf(stderr, "Authentication failure. Shutting down...\n");
			zookeeper_close(zzh);
			getInstance().zh = nullptr;
		} else if (state == ZOO_EXPIRED_SESSION_STATE) {
			fprintf(stderr,
			    "Session expired. Shutting down...! Going to redo election!\n");
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
		fprintf(stderr,"No ZOOKEEPER_SERVER specified in the config.\n");
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

	printf("Starting leader election\n");

	//Connect to the zoo
	if (!blockingConnect()) {
		printf("Bootstrapping into the zoo failed. SessionState:%s\n",
		    sessiontState2String(sessionState).c_str());
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
	vector<string> listOfFiles = FileSystem::getInstance().listFileSystem(false);
	//Create a ZooNode
	ZooNode zooNode(getHostName(),
	    MemoryContorller::getInstance().getAvailableMemory(), listOfFiles,BFSNetwork::getMAC());
	//Send data
	string str = zooNode.toString();
	int result = zoo_create(zh, (electionZNode + "/" + "n_").c_str(), str.c_str(),
	    str.length(), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL | ZOO_SEQUENCE,
	    newNodePath, sizeof(newNodePath));
	if (result != ZOK) {
		printf("makeOffer(): zoo_create failed:%s\n", zerror(result));
		return false;
	}

	leaderOffer = LeaderOffer(string(newNodePath), getHostName());

	printf("Created leader offer %s\n", leaderOffer.toString().c_str());

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
		printf("determineElectionStatus(): zoo_get_children failed:%s\n",
		    zerror(callResult));
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
			printf("There are %lu leader offers. I am %u in line.",
			    leaderOffers.size(), i);

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
			printf("toLeaderOffers(): zoo_get:%s\n", zerror(callResult));
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
	printf("Becoming leader with node:%s\n", leaderOffer.toString().c_str());
	MasterHandler::startLeadership();
	//Commit a publish command!(no matter leader or ready!)
	publishListOfFiles();
}

void ZooHandler::becomeReady(LeaderOffer neighborLeaderOffer) {
	printf("%s not elected leader. Watching node:%s\n",
	    leaderOffer.getNodePath().c_str(),
	    neighborLeaderOffer.getNodePath().c_str());

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
		printf(
		    "We were behind %s but it looks like they died. Back to determination.\n",
		    neighborLeaderOffer.getNodePath().c_str());
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
			printf("Node %s deleted. Need to run through the election process.\n",
			    path);

			if (!getInstance().determineElectionStatus())
				getInstance().electionState = ElectionState::FAILED;
		}
	}
}

void ZooHandler::publishListOfFiles() {
	if (sessionState != ZOO_CONNECTED_STATE
	    || (electionState != ElectionState::LEADER
	        && electionState != ElectionState::READY)) {
		//printf("publishListOfFiles(): invalid sessionstate or electionstate\n");
		return;
	}

	vector<string> listOfFiles = FileSystem::getInstance().listFileSystem(false);
	//Traverse FileSystem Hierarchies

	//Create a ZooNode
	ZooNode zooNode(getHostName(),
	    MemoryContorller::getInstance().getAvailableMemory(), listOfFiles,BFSNetwork::getMAC());
	//Send data
	string str = zooNode.toString();
	int callRes = zoo_set(zh, leaderOffer.getNodePath().c_str(), str.c_str(),
	    str.length(), -1);
	if (callRes != ZOK) {
		//printf("publishListOfFiles(): zoo_set failed:%s\n", zerror(callRes));
		return;
	}

	//LOG(ERROR)<<"publishListOfFiles successfully:"<<str<<endl;
}

std::vector<ZooNode> ZooHandler::getGlobalView() {
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
	if (sessionState != ZOO_CONNECTED_STATE
	    || (electionState != ElectionState::LEADER
	        && electionState != ElectionState::READY)) {
		printf("updateGlobalView(): invalid sessionstate or electionstate\n");
		return;
	}

	//Invalidate previous globaView!
	globalView.clear();
	//1)get list of (electionznode)/BFSElection children and set a watch for changes in these folder
	String_vector children;
	int callResult = zoo_wget_children(zh, electionZNode.c_str(), electionFolderWatcher,nullptr, &children);
	if (callResult != ZOK) {
		printf("updateGlobalView(): zoo_wget_children failed:%s\n",
		    zerror(callResult));
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
			printf("updateGlobalView(): zoo_wget:%s\n", zerror(callResult));
			delete[] buffer;
			buffer = nullptr;
			continue;
		}
		if(len >= 0 && len <= length-1)
			buffer[len] = '\0';

		//3)parse node content to a znode
		vector<string> nodeFiles;
		char *tok = strtok(buffer, "\n");
		if (tok == NULL) {
			printf("updateGlobalView(): strtok failed. Malform data at:%s\n",
			    node.c_str());
			delete[] buffer;
			buffer = nullptr;
			continue;
		}
		string hostName(tok);

		tok = strtok(NULL, "\n");
		if (tok == NULL) {
			printf("updateGlobalView(): strtok failed. Malform data at:%s\n",
					node.c_str());
			delete[] buffer;
			buffer = nullptr;
			continue;
		}
		unsigned char mac[6];
		//Parse MAC String
		sscanf(tok, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2],
				&mac[3], &mac[4], &mac[5]);

		tok = strtok(NULL, "\n");
		if (tok == NULL) {
			printf("updateGlobalView(): strtok failed. Malform data at:%s\n",
			    node.c_str());
			delete[] buffer;
			buffer = nullptr;
			continue;
		}
		string freeSize(tok);
		tok = strtok(NULL, "\n");
		while (tok != NULL) {
			string file(tok);
			nodeFiles.push_back(file);
			tok = strtok(NULL, "\n");
		}
		//4)update globalView
		ZooNode zoonode(hostName, std::stol(freeSize), nodeFiles,mac);
		globalView.push_back(zoonode);
		delete[] buffer;
		buffer = nullptr;
	}

	string glob;
	for(ZooNode node:globalView)
	  glob+= "{"+node.toString()+"}\n";
	//LOG(ERROR)<<"GLOBAL VIEW UPDATED:"<<glob<<endl;

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
		FileNode* fileNode = FileSystem::getInstance().getNode(item.first);
		//If File exist then we won't create it!
		if(fileNode!=nullptr)
			continue;
		//Now create a file in FS
		FileNode* parent = FileSystem::getInstance().createHierarchy(item.first);
		string fileName = FileSystem::getInstance().getFileNameFromPath(item.first);
		FileNode *newFile = FileSystem::getInstance().mkFile(parent, fileName,true);
		newFile->setRemoteHostMAC(item.second.MAC);
		printf("created:%s hostName:%s MAC:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
				item.first.c_str(),item.second.hostName.c_str(),item.second.MAC[0],
				item.second.MAC[1],item.second.MAC[2],item.second.MAC[3],
				item.second.MAC[4],item.second.MAC[5]);
	}

	//Now remove the localRemoteFiles(remote files which have a pointer in our
	// fs locally) which don't exist anymore
	for(string fileName:localFiles) {
	  FileNode* file = FileSystem::getInstance().getNode(fileName);
	  if(file == nullptr){
	    fprintf(stderr,"updateRemoteFilesInFS(): ERROR, cannot find "
	        "corresponding node in filesystem for:%s\n",fileName.c_str());
	    continue;
	  }

	  if(!file->isRemote())
	    continue;

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
	    LOG(ERROR)<<"ZOOOOHANDLER GOING TO REMOVE:"<<file->getFullPath();
	    //fflush(stderr);
	    file->signalDelete(false);
	  }
	}

}

void ZooHandler::nodeWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
	if (type == ZOO_CHANGED_EVENT) {
		string pathStr(path);
		//printf("Node %s changed! updating globalview...\n", path);
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
		printf("fetchAssignmets(): invalid sessionstate or electionstate\n");
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
		printf("fetchAssignmets(): zoo_wget:%s\n", zerror(callResult));
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
		printf("assignmentWatcher(): Node %s changed! updating assignments...\n", path);
		getInstance().fetchAssignmets();
	}
}

ZooNode ZooHandler::getMostFreeNode() {
  updateGlobalView();
  vector<ZooNode>globalView = getGlobalView();
  if(globalView.size() == 0) {
    vector<string> emptyVector;
    ZooNode emptyNode(string(""),0,emptyVector,nullptr);
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
}

}	//Namespace

