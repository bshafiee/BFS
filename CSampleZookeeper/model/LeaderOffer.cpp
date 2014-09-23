/*
 * LeaderOffer.cpp
 *
 *  Created on: Sep 22, 2014
 *      Author: behrooz
 */

#include "LeaderOffer.h"
#include <sstream>

using namespace std;

LeaderOffer::LeaderOffer (long _id, std::string _nodePath,
		std::string _hostName):id(_id),nodePath(_nodePath),hostName(_hostName) {
}

LeaderOffer::LeaderOffer(std::string _nodePath, std::string _hostName):
		id(-1), nodePath(_nodePath),hostName(_hostName) {
}

LeaderOffer::LeaderOffer():id(-1), nodePath(""),hostName("") {
}

LeaderOffer::~LeaderOffer() {
	// TODO Auto-generated destructor stub
}

std::string LeaderOffer::getHostName() const {
	return hostName;
}

long LeaderOffer::getId() const {
	return id;
}

std::string LeaderOffer::getNodePath() const {
	return nodePath;
}

string LeaderOffer::toString() {
	stringstream stream;
	stream <<"{ id:" << id << " nodePath:" << nodePath << " hostName:" << hostName
					<< " }";
	return stream.str();
}
