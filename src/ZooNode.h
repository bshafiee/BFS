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

#ifndef ZOONODE_H_
#define ZOONODE_H_

#include "Global.h"
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include "Backend.h"
//#include "LoggerInclude.h"

namespace BFS {

struct FileEntryNode{
  FileEntryNode(){}
  FileEntryNode(bool _isDir,bool _isVisited):
    isDirectory(_isDir),isVisitedByZooUpdate(_isVisited){}
  bool isDirectory = false;
  bool isVisitedByZooUpdate = false;
};

struct ZooNode {
  std::string hostName;
  uint64_t freeSpace;
  //a pair of file name and a bool indicating if is directory or file
  //std::vector<std::pair<std::string,bool>> *containedFiles;
  std::unordered_map<std::string,FileEntryNode> *containedFiles;
  unsigned char MAC[6];
  std::string ip;
  uint32_t port;

  ZooNode(std::string _hostName,unsigned long _freeSpace,
      std::unordered_map<std::string,FileEntryNode> *_containedFiles,
      const unsigned char *_mac,std::string _ip,
      uint32_t _port):hostName(_hostName),freeSpace(_freeSpace),
      containedFiles(_containedFiles),ip(_ip),port(_port) {
  	if(_mac != nullptr)
  		memcpy(MAC,_mac,sizeof(char)*6);
  }

  ~ZooNode() {}

  std::string toString() {
  	std::stringstream output;
  	output << hostName<< "\n";
  	char macBuff[100];
  	sprintf(macBuff,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5]);
  	output << macBuff<< "\n";
  	output << ip << "\n";
  	output << port << "\n";
  	output << freeSpace;

  	if(containedFiles){
      uint len = containedFiles->size();
      if(len > 0)
        output << "\n";

      uint counter = 0;
      for(std::unordered_map<std::string,FileEntryNode>::iterator it =containedFiles->begin();it!=containedFiles->end();it++) {
        if(counter == len-1)
          output << (it->second.isDirectory?"D":"F")<< it->first;
        else
          output << (it->second.isDirectory?"D":"F")<< it->first<< "\n";
        counter++;
      }
  	}
		//std::string s = output.str();
		//std::cout<<s<<std::endl;
  	return output.str();
  }

  std::ostream& operator<<(std::ostream& os) {
    os<<toString();
    return os;
  }

  static bool CompByFreeSpaceAsc (const ZooNode& lhs, const ZooNode& rhs) {
  	return lhs.freeSpace < rhs.freeSpace;
  }

  static bool CompByFreeSpaceDes (const ZooNode& lhs, const ZooNode& rhs) {
		return lhs.freeSpace > rhs.freeSpace;
	}

  inline bool operator == (const ZooNode& obj) {
    if(this->freeSpace != obj.freeSpace)
      return false;
    //if(this->hostName != obj.hostName)
    if(this->hostName.compare(obj.hostName)!=0)
      return false;
    if(this->port != obj.port)
      return false;
    for(int i=0;i<6;i++)
      if(this->MAC[i] != obj.MAC[i])
        return false;

    return true;
  }
};
}//namespace
#endif /* ZOONODE_H_ */
