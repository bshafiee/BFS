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
#include <unordered_set>

extern "C" {
  #include <zookeeper/zookeeper.h>
  #include <zookeeper/proto.h>
}


namespace BFS {
/**
 * The internal state of the election support service.
 */
enum class ElectionState {
	START, OFFER, DETERMINE, LEADER, READY, FAILED, STOP
};

class ZooHandler {
  friend MasterHandler;
private:
  std::atomic<bool> isRunning;
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
	/**
	 * A hashmap of hash(hostname)->zoonode
	 * for every node in the cluster
	 */
	std::unordered_map<std::size_t,ZooNode> globalView;
	//Hash function used to hash nodes hostname
	std::hash<std::string> hash_fn_gv;

	std::mutex lockGlobalFreeView;
  std::vector<ZooNode> globalFreeView;
  std::string infoNodePath;
	//Publish list of files
	std::mutex lockPublish;
	std::unordered_map<std::string,FileEntryNode> cacheFileList;
	//A list holding the file I have! so I won't need to call listFiles from Filesystem
	std::atomic<bool> myFilesChanged;
	std::unordered_map<std::string,bool> myFiles;//<name,isDir>
	std::mutex lockMyFiles;
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
  /** unlike updateGlobaView, this function only updates view for a specif node **/
  void onNodeChange(const char* path);
  void processNodeChange(const char* buffer,int len);
  void processNodeDelete(const char* path);
  /** Callback for node async get **/
  static void nodeAsyncGet(int rc, const char *value, int value_len,
      const struct Stat *stat, const void *data);
  /** Keeps an eye on the nodes, to see if there is a change in their file list **/
  static void nodeWatcher(zhandle_t *zzh, int type, int state, const char *path,void* context);
  /** Keeps an eye on the BFSElection znode to see if a new client joins. **/
  static void electionFolderWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
  /** Fetch assignments **/
	void fetchAssignmets();
	/** Keeps an eye on the assignment node **/
	static void assignmentWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
	/** This will be called to update list of remote files in our file system *
	void updateRemoteFilesInFS();*/
	/** update nodes free space **/
	static void infoNodeWatcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
	/** keep an eye on infoFolder as well **/
	static void infoFolderWatcher(zhandle_t* zzh, int type, int state, const char* path, void* context);
	/** create infoNode **/
	void createInfoNode();
	/**
	 * used in update GlobalView to create files which exist at other node
	 * but not in our FS.
	 */
	bool createRemoteNodeInLocalFS(const ZooNode &zNode,std::string& _fullpath,bool _isDir);
	/*
	 * Returns true if this zNode is This host
	 */
	bool isME(const ZooNode &zNode);
	/**
	 * Zoo Publisher function
	 */
	static void publishLoop();
	void internalPublishListOfFiles();
	void initializePublishBuffer();
	std::mutex mutexPublishLoop;
  std::condition_variable condVarPublishLoop;
  std::thread *publisherThread;
  char *publishBuffer;
  int publishBufferCommonLen;
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
	void publishListOfFilesSYNC();
	void publishListOfFilesASYN();
	void getGlobalView(std::vector<ZooNode>&);
	std::vector<ZooNode> getGlobalFreeView();
	void startElection();
	ZooNode getFreeNodeFor(uint64_t _reqSize);
	ZooNode getMostFreeNode();
	void requestUpdateGlobalView();
	void stopZooHandler();
	void publishFreeSpace();
	void updateNodesInfoView();
	void printGlobalView();
	//To inform new file or deleting file
	void informNewFile(const std::string& _fullPath,bool _isDir);
	void informDelFile(const std::string& _fullPath);
};

}//Namespace
#endif /* ZOOHANDLER_H_ */
