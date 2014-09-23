/*
 * ZooHandler.h
 *
 *  Created on: Sep 22, 2014
 *      Author: behrooz
 */

#ifndef ZOOHANDLER_H_
#define ZOOHANDLER_H_
#include <stdio.h>
#include <stdlib.h>
#include <zookeeper.h>
#include <proto.h>
#include <ctime>
#include <string>
#include <errno.h>
#include <vector>
#include <unistd.h>
#include "LeaderOffer.h"

/**
 * The internal state of the election support service.
 */
enum class ElectionState {
	START, OFFER, DETERMINE, LEADER, READY, FAILED, STOP
};

class ZooHandler {
private:
	zhandle_t *zh;
	clientid_t myid;
	int sessionState;
	//const std::string hostPort = "10.42.0.62:2181,10.42.0.62:2182,10.42.0.62:2183";
	const std::string hostPort = "127.0.0.1:2181";
	const int connectionTimeout = 5000*1000;//wait up to 5 seconds for connection
	const std::string electionZNode = "/BFSElection";
	const char nodeDelimitter = '/';
	ElectionState electionState;
	LeaderOffer leaderOffer;
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
public:
	static ZooHandler& getInstance();
	virtual ~ZooHandler();
	/** Helper methods **/
	static std::string sessiontState2String(int state);
	static std::string zooEventType2String(int state);
	static void dumpStat(const struct Stat *stat);
	static std::string getHostName();
	/** Zoo Operations **/
	int getSessionState();
	ElectionState getElectionState();
	void startElection();
};

#endif /* ZOOHANDLER_H_ */
