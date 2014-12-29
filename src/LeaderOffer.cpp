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

#include "LeaderOffer.h"
#include <sstream>

using namespace std;

namespace FUSESwift {

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

void LeaderOffer::setId(long id) {
  this->id = id;
}

}//Namespace
