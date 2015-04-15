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

#ifndef LEADEROFFER_H_
#define LEADEROFFER_H_
#include "Global.h"
#include <string>

namespace BFS {

class LeaderOffer {
private:
  long id;
  std::string nodePath;
  std::string hostName;
public:
  LeaderOffer (long _id, std::string _nodePath, std::string _hostName);
  LeaderOffer (std::string _nodePath, std::string _hostName);
  LeaderOffer ();
	virtual ~LeaderOffer();
	std::string toString();
	std::string getHostName() const;
	long getId() const;
	std::string getNodePath() const;
  void setId(long id);
  /** Comparator for sorting **/
  static bool Comparator (const LeaderOffer& lhs, const LeaderOffer& rhs) {
    return lhs.id < rhs.id;
  }

};
}//Namespace
#endif /* LEADEROFFER_H_ */
