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

#ifndef ZOOHANDLER_H_
#define ZOOHANDLER_H_

#include "Global.h"
#include <string>
#include <errno.h>
#include <vector>
#include <unistd.h>
#include "LeaderOffer.h"
#include "MasterHandler.h"
#include "ZooNode.h"
#include "BFSNetwork.h"
#include <algorithm>
#include <random>

extern "C" {
  #include <zookeeper/zookeeper.h>
  #include <zookeeper/proto.h>
}


namespace FUSESwift {
/**
 * The internal state of the election support service.
 */
enum class ElectionState {
	START, OFFER, DETERMINE, LEADER, READY, FAILED, STOP
};

class ZooHandler {
  friend MasterHandler;
private:
	zhandle_t *zh;
	const clientid_t *myid;
	int sessionState;
	std::string hostPort = "10.42.0.97:2181,10.42.0.62:2182,129.97.170.232:2181";
	const long long connectionTimeout = 5000*1000;//wait up to 5 seconds for connection
	std::string electionZNode = "/BFSElection";
	std::string infoZNode = "/BFSInfo";
	std::string assignmentZNode = "/BFSTasks";
	const char nodeDelimitter = '/';
	ElectionState electionState;
	LeaderOffer leaderOffer;
	//A hashmap too keep track of which file is at which node!<nodeaddress,list of files>
	std::mutex lockGlobalView;
	std::vector<ZooNode> globalView;
	std::mutex lockGlobalFreeView;
  std::vector<ZooNode> globalFreeView;
  std::string infoNodePath;
	//Publish list of files
	std::mutex lockPublish;
  std::vector<std::pair<std::string,bool>> cacheFileList;
	//Private Constructor
	ZooHandler();
	/** Election private helpers **/
	static void sessionWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
	bool connect();
	bool blockingConnect();
	bool makeOffer();
	bool determineElectionStatus();
	std::vector<LeaderOffer> toLeaderOffers(const std::vector<std::string> &children);
	void becomeLeader();
  void becomeReady(LeaderOffer neighborLeaderOffer);
  static void neighbourWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
  /** Updates Global View of files at each node **/
  void updateGlobalView();
  /** Keeps an eye on the nodes, to see if there is a change in their file list **/
  static void nodeWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
  /** Keeps an eye on the BFSElection znode to see if a new client joins. **/
  static void electionFolderWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
  /** Fetch assignments **/
	void fetchAssignmets();
	/** Keeps an eye on the assignment node **/
	static void assignmentWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
	/** This will be called to update list of remote files in our file system **/
	void updateRemoteFilesInFS();
	/** update nodes free space **/
	static void infoNodeWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
	/** keep an eye on infoFolder as well **/
	static void infoFolderWatcher(zhandle_t* zzh, int type, int state, const char* path, void* context);
	/** create infoNode **/
	void createInfoNode();
public:
	static ZooHandler& getInstance();
	virtual ~ZooHandler();
	/** Helper methods **/
	static std::string sessiontState2String(int state);
	static std::string zooEventType2String(int state);
	static std::string getHostName();
	/** Zoo Operations **/
	int getSessionState();
	ElectionState getElectionState();
	void publishListOfFiles();
	std::vector<ZooNode> getGlobalView();
	std::vector<ZooNode> getGlobalFreeView();
	void startElection();
	ZooNode getFreeNodeFor(uint64_t _reqSize);
	ZooNode getMostFreeNode();
	void requestUpdateGlobalView();
	void stopZooHandler();
	void publishFreeSpace();
	void updateNodesInfoView();
	void printGlobalView();
};

}//Namespace
#endif /* ZOOHANDLER_H_ */
