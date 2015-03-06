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

#include "ZooHandler.h"
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <algorithm>    // std::sort
#include "LeaderOffer.h"
#include "ZooNode.h"
#include "MemoryController.h"
#include "DownloadQueue.h"
#include "string.h"
#include "ZeroNetwork.h"
#include "SettingManager.h"
#include "BFSTcpServer.h"
#include "LoggerInclude.h"
#include <Poco/StringTokenizer.h>
#include <algorithm>
#include <string.h>
#include "Filesystem.h"
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
		    ElectionState::FAILED),globalView(1000){
  srand(unsigned(time(NULL)));
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
			//publish free space
      getInstance().createInfoNode();
      getInstance().publishFreeSpace();
      getInstance().updateNodesInfoView();
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

	//Create a ZooNode
	ZooNode zooNode(getHostName(),0,nullptr,BFSNetwork::getMAC(),BFSTcpServer::getIP(),
	    BFSTcpServer::getPort());
	//Send data
	string str = zooNode.toString();
	int result = zoo_create(zh, (electionZNode + "/" +zooNode.ip+"_").c_str(), str.c_str(),
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

	long id = std::stoi(sequenceNum.substr(sequenceNum.find('_')+1));
	leaderOffer.setId(id);

	//Get List of children of /BFSElection
	String_vector children;
	children.data = nullptr;
	int callResult = zoo_get_children(zh, electionZNode.c_str(), 1, &children);
	if (callResult != ZOK) {
	  LOG(ERROR)<<"zoo_get_children failed:"<<zerror(callResult);
		return false;
	}
	vector<string> childrenVector;
	for (int i = 0; i < children.count; i++)
		childrenVector.push_back(string(children.data[i]));
	free(children.data);
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
		char buffer[100000l];
		int len = -1;
		int callResult = zoo_get(zh, (electionZNode + "/" + offer).c_str(), 0,
		    buffer, &len, nullptr);
		if (callResult != ZOK) {
			LOG(ERROR)<<"zoo_get:"<<zerror(callResult);
			continue;
		}

		buffer[len - 1] = '\0';
		string hostName = string(buffer);
		long id = std::stol(offer.substr(offer.find('_')+1));
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
	LOG(DEBUG)<<"Becoming leader, hostname:"<<leaderOffer.toString();
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
  //TIMED_FUNC(objname3);
	if(SettingManager::runtimeMode()!=RUNTIME_MODE::DISTRIBUTED)
		return;
	if (sessionState != ZOO_CONNECTED_STATE
	    || (electionState != ElectionState::LEADER
	        && electionState != ElectionState::READY)) {
		LOG(ERROR)<<"publishListOfFiles(): invalid sessionstate or electionstate";
		return;
	}

	lock_guard<mutex> lk(lockPublish);
	std::unordered_map<std::string,FileEntryNode> listOfFiles;
	FileSystem::getInstance().listFileSystem(listOfFiles,false,true);


	bool change = false;
	if(listOfFiles.size() != cacheFileList.size())
	  change = true;
	else{
    for(std::unordered_map<std::string,FileEntryNode>::iterator
        itListFiles = listOfFiles.begin();
        itListFiles != listOfFiles.end();
        itListFiles++){

      std::unordered_map<std::string,FileEntryNode>::const_iterator got =
      cacheFileList.find(itListFiles->first);

      //check if that name exist
      if(got == cacheFileList.end()){
        change = true;
        break;
      }
      //Check if is directory is also same
      if(got->second.isDirectory != itListFiles->second.isDirectory){
        change = true;
        break;
      }
    }
	}

	if(!change){
	  LOG(DEBUG)<<"No new change, in the list of file. Returning...";
	  return;
	}
	cacheFileList.swap(listOfFiles);
	//DANGER! FROM THIS POINT LIST OF FILE IS INVALID

	//Create a ZooNode
  ZooNode zooNode(getHostName(),0 , &cacheFileList,BFSNetwork::getMAC(),BFSTcpServer::getIP(),BFSTcpServer::getPort());

	//Send data
  {
  //TIMED_SCOPE(timerBlkObj, "SET PUBLISH for:"+to_string(cacheFileList.size())+" took:");
	string str = zooNode.toString();
	int callRes = zoo_set(zh, leaderOffer.getNodePath().c_str(), str.c_str(),
	    str.length(), -1);

	if (callRes != ZOK) {
	  LOG(ERROR)<<"publishListOfFiles(): zoo_set failed:"<< zerror(callRes);
		return;
	}

	LOG(DEBUG)<<"publishListOfFiles successfully:"<<str;
  }
}

void ZooHandler::getGlobalView(std::vector<ZooNode> &_globaView) {
  lock_guard<mutex> lk(lockGlobalView);
  for(auto it=globalView.begin();it!=globalView.end();it++){
    ZooNode node = it->second;
    node.containedFiles = new std::unordered_map<std::string,FileEntryNode>();
    node.containedFiles->insert(it->second.containedFiles->begin(),it->second.containedFiles->end());
    _globaView.emplace_back(node);
  }
}

std::vector<ZooNode> ZooHandler::getGlobalFreeView() {
  lock_guard<mutex> lk(lockGlobalFreeView);
  return globalFreeView;
}

/**
 * 1)get list of electionznode(/BFSElection) children
 * 	 and set a watch for changes in these folder
 * 2)get content of each node and set watch on them
 * 3)parse nodes content to znode
 * 4)update globalView
 **/
void ZooHandler::updateGlobalView() {
  if(SettingManager::runtimeMode()!=RUNTIME_MODE::DISTRIBUTED)
    return;
  lock_guard<mutex> lk(lockGlobalView);

	if (sessionState != ZOO_CONNECTED_STATE
	    || (electionState != ElectionState::LEADER
	        && electionState != ElectionState::READY)) {
		LOG(ERROR)<<"updateGlobalView(): invalid sessionstate or electionstate";
		return;
	}

	//1)get list of (electionznode)/BFSElection children and set a watch for changes in these folder
	String_vector children;
	children.data = nullptr;
	int callResult = zoo_wget_children(zh, electionZNode.c_str(), electionFolderWatcher,nullptr, &children);
	if (callResult != ZOK) {
	  LOG(ERROR)<<"updateGlobalView(): zoo_wget_children failed:"<<zerror(callResult);
	  if(children.data)
	    free(children.data);
		return;
	}
	//Allocate 1MB data
  const int length = 1024 * 1024;
  char *buffer = new char[length];
	//2)get content of each node and set watch on them
	for (int i = 0; i < children.count; i++) {
		int len = length;
		string tmp = electionZNode;
		tmp += "/";
		tmp += string(children.data[i]);
		int callResult = zoo_wget(zh, tmp.c_str(),
		    nodeWatcher, nullptr, buffer, &len, nullptr);
		if (callResult != ZOK) {
			LOG(ERROR)<<"zoo_wget failed:"<<zerror(callResult);
			continue;
		}
		if(len >= 0 && len <= length-1)
			buffer[len] = '\0';

		//Process Node data
		processNodeView(buffer,tmp.c_str());
	}
	delete[] buffer;
  buffer = nullptr;

	if(children.data)
	  free(children.data);
}

bool ZooHandler::isME(const ZooNode &zNode){
  //We should not include ourself in this
  const unsigned char* myMAC = BFSNetwork::getMAC();

  bool sameMAC = true;
  for(uint i=0;i<6;i++)
    if(myMAC[i] != zNode.MAC[i]){
      sameMAC = false;
      break;
    }

  if(sameMAC && SettingManager::getPort()==(int)zNode.port)//isME
    return true;
  return false;
}

bool ZooHandler::createRemoteNodeInLocalFS(const ZooNode &node,std::string& _fullpath,bool _isDir) {
  //Now create a file in FS
  FileSystem::getInstance().createHierarchy(_fullpath,true);
  FileNode *newFile = nullptr;
  if(_isDir){//dir
    newFile = FileSystem::getInstance().mkDirectory(_fullpath,true);
    if(!newFile)
      return false;
    newFile->open();
  }
  else //file
    newFile = FileSystem::getInstance().mkFile(_fullpath,true,true);//open
  bool updated = false;
  if(newFile == nullptr) {
    /**
     * Check Maybe this node existed (a move has happened); so we just need to
     * re assign ip and mac
     */
    newFile = FileSystem::getInstance().findAndOpenNode(_fullpath);
    if(newFile == nullptr)
      return false;
    updated = true;
  }
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)newFile);
  newFile->setRemoteHostMAC(node.MAC);
  newFile->setRemoteIP(node.ip);
  newFile->setRemotePort(node.port);
  newFile->close(inodeNum);//create and open operation, close file
  char macCharBuff[100];
  sprintf(macCharBuff,"MAC:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
      node.MAC[0],node.MAC[1],
      node.MAC[2],node.MAC[3],
      node.MAC[4],node.MAC[5]);
  LOG(DEBUG)<<(updated?"Updated ":"Created ")<<(_isDir?"Directory:":"File:")<<_fullpath<<" hostName:"<<
      node.hostName<<" "<<string(macCharBuff);
  return true;
}

void ZooHandler::nodeWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
	if (type == ZOO_CHANGED_EVENT) {
		LOG(DEBUG)<<"Node "<<path<<" changed! updating globalview...";
		//string ipFromPath(path+(strlen(getInstance().electionZNode.c_str())+1)*sizeof(char),strchr(path,'_'));
		getInstance().updateViewPerNode(path);
	}
}

void ZooHandler::updateViewPerNode(const char* path){
  lock_guard<mutex> lk(lockGlobalView);
  if (sessionState != ZOO_CONNECTED_STATE
      || (electionState != ElectionState::LEADER
          && electionState != ElectionState::READY)) {
    LOG(ERROR)<<"updateGlobalView(): invalid sessionstate or electionstate";
    return;
  }

  //Find node
  //Allocate 1MB data
  const int length = 1024 * 1024;
  char *buffer = new char[length];

  int len = length;
  int callResult = zoo_wget(zh, path,nodeWatcher, nullptr, buffer, &len, nullptr);
  if (callResult != ZOK) {
    LOG(ERROR)<<"zoo_wget failed:"<<zerror(callResult);
    delete[] buffer;
    buffer = nullptr;
    return;
  }
  if(len >= 0 && len <= length-1){
    buffer[len] = '\0';
    processNodeView(buffer,path);
  }
  else
    LOG(ERROR)<<"ERROR in reading data from:"<<path<<" read:"<<len<<" bytes.";

  delete []buffer;
  buffer = nullptr;
}
void ZooHandler::processNodeView(const char* buffer,const char* path){
  Poco::StringTokenizer tokenizer(buffer,"\n",
  Poco::StringTokenizer::TOK_TRIM |
  Poco::StringTokenizer::TOK_IGNORE_EMPTY);

  uint numTokens = tokenizer.count();

  if(numTokens < 5) {
    LOG(ERROR)<<"Malformed data at:"<<path<<" Buffer:"<<buffer;
    delete []buffer;
    return;
  }

  //3)parse node content to a znode
  //3.1 Hostname
  string &hostName = tokenizer[0];

  //3.2 MAC String
  unsigned char mac[6];
  sscanf(tokenizer[1].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2],
      &mac[3], &mac[4], &mac[5]);

  //3.3 ip
  //we already know IP
  string &ip = tokenizer[2];

  //3.4 port
  uint32_t port = stoul(tokenizer[3]);

  //3.1 freespace
  unsigned long freeSpace = 0;
  //we know free space is 0 here tokenizer[4]
  //stoll(string(itrTokens->first,itrTokens->second));

  //Now let's find this node in our globalView if not exist create one!
  std::size_t hash = hash_fn_gv(ip);
  auto got = globalView.find(hash);
  bool newZNode = false;
  if(got==globalView.end()){//Not Found
    std::unordered_map<std::string,FileEntryNode> *nodeFiles =
            new std::unordered_map<std::string,FileEntryNode>();
    //Wont' call destructor of zoonode because otherwise it's nodefiles will be invalid
    auto resInsert =  globalView.emplace(hash,ZooNode(hostName, freeSpace, nodeFiles,mac,ip,port));
    if(!resInsert.second){
      LOG(ERROR)<<"Error in inserting new node to globalView:"<<hostName<<" ip:"<<ip<<" port:"<<port<<" hash:"<<hash;
      delete []buffer;
      return;
    }
    got = resInsert.first;
    newZNode = true;
  }

  //Now 'got' is pointing to the correct zoonode
  //remember for newly inserted nodes we have to insert them all and create them all!
  if(newZNode){
    for(uint i=5;i<numTokens;i++) {
      const char &firstChar = tokenizer[i][0];
      string fileName = tokenizer[i].substr(1);
      if(!got->second.containedFiles->emplace(fileName,FileEntryNode(firstChar=='D',false)).second){
        LOG(ERROR)<<"Failed to insert file:"<<fileName<<" to files of "<<got->second.hostName;
        continue;
      }

      if(!isME(got->second)){//Don't create local files again
        //Create it in the filesystem
        if(!createRemoteNodeInLocalFS(got->second,fileName,firstChar=='D'))
          LOG(ERROR)<<"Error in creating new remotefile in local FS:"<<fileName<<" at:"<<got->second.hostName;
      }
    }
  } else {//Not a new ZooNode
    /**
     * First check for new remote files
     * Also mark files in containedfiles of each
     * ZooNode as seen by update so we won't check
     * them for delete
     */
    for(uint i=5;i<numTokens;i++) {
      const char &firstChar = tokenizer[i][0];
      string fileName = tokenizer[i].substr(1);

      //check if it exist in this node or not!
      auto res = got->second.containedFiles->find(fileName);
      if(res == got->second.containedFiles->end()) {//A newFile
        //Add it to contained files hashmap
        if(!got->second.containedFiles->emplace(fileName,FileEntryNode(firstChar=='D',true)).second){
          LOG(ERROR)<<"Failed to insert file:"<<fileName<<" to files of "<<got->second.hostName;
          continue;
        }

        if(!isME(got->second)){//Don't create local files again
          //Create it in the filesystem
          if(!createRemoteNodeInLocalFS(got->second,fileName,firstChar=='D'))
            LOG(ERROR)<<"Error in creating new remotefile in local FS:"<<fileName<<" at:"<<got->second.hostName;
        }
      } else {//Just mark it as seen by update
        res->second.isVisitedByZooUpdate = true;
      }
    }
    /**
     * Second Check for files in our FS that do not exist remotely anymore
     * and should be deleted.
     * Go through znode Files and look for those which are not visitedByZooUpdate!
     **/
    if(!isME(got->second)){//Not necessarily for myself
      for(auto itRemove=got->second.containedFiles->begin();
          itRemove != got->second.containedFiles->end();){
        if(itRemove->second.isVisitedByZooUpdate){//Has been seen
          itRemove->second.isVisitedByZooUpdate = false;//just set it to false for next time
          itRemove++;
        }
        else {//This file should be removed!
          FileNode* toBeRemovedFile = FileSystem::getInstance().findAndOpenNode(itRemove->first);
          if(toBeRemovedFile == nullptr){
            LOG(DEBUG)<<"ToBeRemoved node is null!:"<<itRemove->first;
            //remove from list of containedFiles
            itRemove = got->second.containedFiles->erase(itRemove);
            continue;
          }
          /*
           * VERY IMPORTANT! we should check if this file is remote!
           * If it's not remote it means that it could have been moved to here!
           * and we are the real owner of it not that mother fucker remote node!
           * Jez! what a creepy bug this was!
           */
          if(toBeRemovedFile->isRemote() && !toBeRemovedFile->shouldNotZooRemove() && toBeRemovedFile->getRemoteHostIP() == ip){
            LOG(DEBUG)<<"SIGNAL DELTE FROM ZOOHANDLER FOR:"<<itRemove->first<<" host:"<<got->second.hostName;
            FileSystem::getInstance().signalDeleteNode(toBeRemovedFile,false);
          }
          /*
           * However, no matter it is remote or local now we will remove it from
           * the list of files for that mother fucker node.
           */
          LOG(INFO)<<"DEBUG:"<<itRemove->first<<" from hashmap host:"<<got->second.hostName;
          itRemove = got->second.containedFiles->erase(itRemove);
        }
      }
    }
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
	const long length = 1024l * 1024l;
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

ZooNode ZooHandler::getFreeNodeFor(uint64_t _reqSize) {
  updateNodesInfoView();
  lock_guard<mutex> lk(lockGlobalFreeView);

  ZooNode emptyNode(string(""),0,nullptr,nullptr,"",0);

  if(globalFreeView.size() == 0) {
    return emptyNode;
  }

  vector<ZooNode> possibleNodes;

  for(ZooNode n:globalFreeView)
    if(n.freeSpace > _reqSize)
      possibleNodes.push_back(n);
  if(possibleNodes.size() == 0)
    return emptyNode;
  int randomIndex = std::rand()%possibleNodes.size();
  return possibleNodes[randomIndex];
  //Now sort ourZoo by Free Space descendingly!
  /*std::sort(globalFreeView.begin(),globalFreeView.end(),ZooNode::CompByFreeSpaceDes);
  string output;
  for(ZooNode z:globalFreeView){
    output += "("+z.hostName+","+std::to_string(z.freeSpace/1024l/1024l)+"MB)";
  }
  LOG(DEBUG)<<output<<"front:("<<globalFreeView.front().hostName<<
      ","<<globalFreeView.front().freeSpace/1024ll/1024ll<<"MB)";
  return globalFreeView.front();*/
}

ZooNode ZooHandler::getMostFreeNode() {
  updateNodesInfoView();
  lock_guard<mutex> lk(lockGlobalFreeView);

  ZooNode emptyNode(string(""),0,nullptr,nullptr,"",0);

  if(globalFreeView.size() == 0) {
    return emptyNode;
  }

  //Now sort ourZoo by Free Space descendingly!
  std::sort(globalFreeView.begin(),globalFreeView.end(),ZooNode::CompByFreeSpaceDes);

  //Shuffle equal nods
  int startSame = 0;
  int endSame = 0;
  for(unsigned int i=0;i<globalFreeView.size();i++) {
    uint64_t diffFreeView = globalFreeView[0].freeSpace - globalFreeView[i].freeSpace;
    /**
     * if Freespace diff is less than or equal to 1% of most free
     * node storage we consider that node also as most free node as well.
     */
    if(diffFreeView <= globalFreeView[0].freeSpace/100)
      endSame = i;
  }

  int randomIndex = std::rand()%(endSame-startSame+1);
  return globalFreeView[randomIndex];
}

void ZooHandler::requestUpdateGlobalView() {
  updateGlobalView();
}

void ZooHandler::createInfoNode() {
  if(SettingManager::runtimeMode()!=RUNTIME_MODE::DISTRIBUTED)
    return;
  if (sessionState != ZOO_CONNECTED_STATE) {
    LOG(ERROR)<<"createInfoNode(): invalid sessionstate";
    return;
  }

  char newNodePath[1000];
  //ZooNode
  //Create a ZooNode
  ZooNode zooNode(getHostName(),
      MemoryContorller::getInstance().getAvailableMemory()-
      MemoryContorller::getInstance().getClaimedMemory(),
      nullptr,BFSNetwork::getMAC(),BFSTcpServer::getIP(),
      BFSTcpServer::getPort());
  //Send data
  string str = zooNode.toString();
  int result = zoo_create(zh, (infoZNode+ "/" + "n_").c_str(), str.c_str(),
      str.length(), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL | ZOO_SEQUENCE,
      newNodePath, sizeof(newNodePath));
  if (result != ZOK) {
    LOG(ERROR)<<"zoo_create failed:"<<zerror(result);
    return;
  }

  this->infoNodePath = string(newNodePath);
  LOG(DEBUG)<<"Created InfoNode: "<<str;
}

void ZooHandler::infoNodeWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
  LOG(DEBUG)<<"INFO FOLDER WATCHER EVENT";
  getInstance().updateNodesInfoView();
}

void ZooHandler::publishFreeSpace() {
  if (sessionState != ZOO_CONNECTED_STATE) {
    LOG(ERROR)<<"publishFreeSpace(): invalid sessionstate or electionstate";
    return;
  }

  //Create a ZooNode
  int64_t freeSpace = MemoryContorller::getInstance().getAvailableMemory() -
      MemoryContorller::getInstance().getClaimedMemory();
  if(freeSpace < 0){
    LOG(ERROR)<<"\nJEEZZZINVALID FREE SPACE! avail:"<<
        MemoryContorller::getInstance().getAvailableMemory()
        <<" claimed:"<<MemoryContorller::getInstance().getClaimedMemory()
        <<endl;
    freeSpace = 0;
  }

  ZooNode zooNode(getHostName(),freeSpace ,nullptr,BFSNetwork::getMAC(),BFSTcpServer::getIP(),BFSTcpServer::getPort());

  //Send data
  string str = zooNode.toString();
  int callRes = zoo_set(zh, infoNodePath.c_str(), str.c_str(),str.length(), -1);

  if (callRes != ZOK) {
    LOG(ERROR)<<"publishFreeSpace(): zoo_set failed:"<< zerror(callRes);
    return;
  }

  LOG(INFO)<<"publish:"<< freeSpace/1024ll/1024ll<<"MB freespace. Avail:"<<
      MemoryContorller::getInstance().getAvailableMemory()/1024ll/1024ll<<
      "MB claimed:"<<MemoryContorller::getInstance().getClaimedMemory()/1024ll/1024ll<<"MB";
}

void ZooHandler::stopZooHandler() {
  sessionState = ZOO_EXPIRED_SESSION_STATE;
  zookeeper_close(zh);
  zh = nullptr;
  LOG(INFO)<<"ZOOHANDLER LOOP DEAD!";
}

void ZooHandler::infoFolderWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
  LOG(DEBUG)<<"INFO FOLDER WATCHER EVENT";
  getInstance().updateNodesInfoView();
}

void ZooHandler::updateNodesInfoView() {
  if(SettingManager::runtimeMode()!=RUNTIME_MODE::DISTRIBUTED)
      return;
  lock_guard<mutex> lk(lockGlobalFreeView);

  if (sessionState != ZOO_CONNECTED_STATE) {
    LOG(ERROR)<<"updateNodesInfoView(): invalid sessionstate or electionstate";
    return;
  }

  globalFreeView.clear();
  //1)get list of (infoZNode)/BFSElection children and set a watch for changes in these folder
  String_vector children;
  children.data = nullptr;
  int callResult = zoo_wget_children(zh, infoZNode.c_str(), infoFolderWatcher,nullptr, &children);
  if (callResult != ZOK) {
    LOG(ERROR)<<"infoFolderWatcher(): zoo_wget_children failed:"<<zerror(callResult);
    return;
  }
  //2)get content of each node and set watch on them
  for (int i = 0; i < children.count; i++) {
    string node(children.data[i]);
    //Allocate 1MB data
    const int length = 1024 * 1024;
    char *buffer = new char[length];
    int len = length;
    int callResult = zoo_wget(zh, (infoZNode+ "/" + node).c_str(),
        infoNodeWatcher, nullptr, buffer, &len, nullptr);
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

    //4)update globalFreeView
    globalFreeView.emplace_back(ZooNode(hostName,stoll(freeSize),nullptr,mac,ip,stoul(port)));
    delete[] buffer;
    buffer = nullptr;
  }
  if(children.data)
    free(children.data);
  //Debug
  /*string output;
  std::sort(globalFreeView.begin(),globalFreeView.end(),ZooNode::CompByFreeSpaceDes);
  for(ZooNode z:globalFreeView){
    output += "("+z.hostName+","+std::to_string(z.freeSpace/1024l/1024l)+"MB)";
  }
  LOG(DEBUG)<<"UpdatedFreeSpaceView:"<<output;*/
}

void ZooHandler::printGlobalView() {
  /*string output;
  for(ZooNode node:globalView){
    output += node.hostName +"(";
    if(node.containedFiles){
      for(std::unordered_map<std::string,FileEntryNode>::iterator
          it = node.containedFiles->begin();
          it != node.containedFiles->end();
          it++){
        output += it->first+",";
      }
    }
    output += "),";
  }
  LOG(INFO)<<"\nGlobalView:"<<output<<"\n";*/
}

}	//Namespace


