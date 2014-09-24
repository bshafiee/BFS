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
		zh(nullptr), sessionState(ZOO_EXPIRED_SESSION_STATE),
		electionState(ElectionState::FAILED){
	//Set Debug info
	zoo_set_log_stream(stdout);
	zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
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
			(unsigned int) stat->aversion,
			(long long) stat->ephemeralOwner);
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
			fprintf(stderr, "Session expired. Shutting down...\n");
			zookeeper_close(zzh);
			getInstance().zh = nullptr;
		}
	}
}

bool ZooHandler::connect() {
	zh = zookeeper_init(hostPort.c_str(), sessionWatcher, 30000, 0, 0, 0);
	if (!zh) {
		return false;
	}

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
	if(!this->connect())
		return false;
  struct timeval start, now;
  gettimeofday(&now, NULL);
  start = now;
  while(sessionState != ZOO_CONNECTED_STATE &&
  		(now.tv_usec-start.tv_usec) <= connectionTimeout ) {
  	gettimeofday(&now, NULL);
  	usleep(10);
  }

  if(sessionState == ZOO_CONNECTED_STATE)
  	return true;
  else
  	return false;
}

ElectionState ZooHandler::getElectionState() {
	return electionState;
}

std::string ZooHandler::getHostName() {
	char buff[100];
	gethostname(buff,sizeof(buff));
	return string(buff);
}

void ZooHandler::startElection() {
	//First Change state to start
	electionState = ElectionState::START;

	printf("Starting leader election\n");

	//Connect to the zoo
	if(!blockingConnect()) {
		printf("Bootstrapping into the zoo failed. SessionState:%s\n",
				sessiontState2String(sessionState).c_str());
		electionState = ElectionState::FAILED;
		return;
	}

	//Offer leadership
	bool offerResult = makeOffer();
	if(!offerResult) {
		electionState = ElectionState::FAILED;
		return;
	}

	//Check if we're leader or not
	determineElectionStatus();
}

bool ZooHandler::makeOffer() {
	electionState = ElectionState::OFFER;

	//Create an ephemeral sequential node
	string hostName = getHostName();
	//struct ACL _OPEN_ACL_UNSAFE_ACL[] = {{0x1f, {"world", "anyone"}}};
	//struct ACL_vector ZOO_OPEN_ACL_UNSAFE = { 1, _OPEN_ACL_UNSAFE_ACL};
	char newNodePath[1000];
	/* Create Election Base node
	 * zoo_create(zh,(electionZNode).c_str(),"Election Offers",
				strlen("Election Offers"),&ZOO_OPEN_ACL_UNSAFE ,0,newNodePath,newNodePathLen);*/

	int result = zoo_create(zh,(electionZNode + "/" + "n_").c_str(),hostName.c_str(),
			hostName.length(),&ZOO_OPEN_ACL_UNSAFE ,ZOO_EPHEMERAL|ZOO_SEQUENCE,newNodePath,sizeof(newNodePath));
	if(result != ZOK) {
		printf("makeOffer(): zoo_create failed:%s\n",zerror(result));
		return false;
	}

	leaderOffer = LeaderOffer(string(newNodePath),hostName);

	printf("Created leader offer %s\n", leaderOffer.toString().c_str());

	return true;
}

bool ZooHandler::determineElectionStatus() {
  electionState = ElectionState::DETERMINE;

  vector<string> components = split(leaderOffer.getNodePath(),nodeDelimitter);

  //Extract Id from nodepath
  string sequenceNum = components[components.size() - 1];
  int id = std::stoi(sequenceNum.substr(strlen("n_")));
  leaderOffer.setId(id);


  //Get List of children of /BFSElection
  String_vector children;
  int callResult = zoo_get_children(zh,electionZNode.c_str(),1,&children);
  if(callResult != ZOK) {
    printf("determineElectionStatus(): zoo_get_children:%s\n",zerror(callResult));
    return false;
  }
  vector<string>childrenVector;
  for(int i=0;i<children.count;i++)
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
    char buffer [1000];
    int len = -1;
    int callResult = zoo_get(zh,(electionZNode+"/"+offer).c_str(),0,
                            buffer,&len,nullptr);
    if(callResult != ZOK) {
      printf("toLeaderOffers(): zoo_get:%s\n",zerror(callResult));
      continue;
    }

    buffer[len-1] = '\0';
    string hostName = string(buffer);
    int id = std::stoi(offer.substr(strlen("n_")));
    leaderOffers.push_back(LeaderOffer(id, electionZNode + "/" + offer, hostName));
  }

  /*
   * We sort leader offers by sequence number (which may not be zero-based or
   * contiguous) and keep their paths handy for setting watches.
   */
  std::sort(leaderOffers.begin(),leaderOffers.end(),LeaderOffer::Comparator);

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
  int callResult = zoo_wexists(zh,neighborLeaderOffer.getNodePath().c_str(),
                                neighbourWatcher,nullptr,&stat);
  if(callResult != ZOK) {
    electionState = ElectionState::FAILED;
    /*
     * If the stat fails, the node has gone missing between the call to
     * getChildren() and exists(). We need to try and become the leader.
     */
    printf("We were behind %s but it looks like they died. Back to determination.\n",
            neighborLeaderOffer.getNodePath().c_str());
    determineElectionStatus();
    return;
  }

  //Finally if everything went ok we become ready
  electionState = ElectionState::READY;
}

void ZooHandler::neighbourWatcher(zhandle_t* zzh, int type, int state,
    const char* path, void* context) {
  if (type == ZOO_DELETED_EVENT) {
    string pathStr(path);
    if (pathStr != getInstance().leaderOffer.getNodePath()
        && getInstance().electionState != ElectionState::STOP) {
      printf("Node %s deleted. Need to run through the election process.\n",path);

      if(!getInstance().determineElectionStatus())
        getInstance().electionState = ElectionState::FAILED;
    }
  }
}

}//Namespace
