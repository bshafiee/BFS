/*
 * ZooNode.h
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#ifndef ZOONODE_H_
#define ZOONODE_H_

#include "Global.h"
#include <string>
#include <vector>
#include <sstream>
#include "Backend.h"
//#include "LoggerInclude.h"

namespace FUSESwift {

struct ZooNode {
  std::string hostName;
  uint64_t freeSpace;
  std::vector<std::string> containedFiles;
  unsigned char MAC[6];
  std::string ip;
  uint32_t port;

  ZooNode(std::string _hostName,unsigned long _freeSpace,
  				std::vector<std::string> _containedFiles,const unsigned char *_mac,std::string _ip,uint32_t _port):hostName(_hostName),
					freeSpace(_freeSpace),containedFiles(_containedFiles),ip(_ip),port(_port) {
  	if(_mac != nullptr)
  		memcpy(MAC,_mac,sizeof(char)*6);
  }

  std::string toString() {
  	std::stringstream output;
  	output << hostName<< "\n";
  	char macBuff[100];
  	sprintf(macBuff,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5]);
  	output << macBuff<< "\n";
  	output << ip << "\n";
  	output << port << "\n";
  	output << freeSpace;
  	if(containedFiles.size() > 0)
  		output << "\n";

  	uint counter = 0;
		for(auto it =containedFiles.begin();it!=containedFiles.end();it++) {
			if(counter == containedFiles.size()-1)
				output << *it;
			else
				output << *it << "\n";
			counter++;
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
    if(this->hostName != obj.hostName)
      return false;
    for(int i=0;i<6;i++)
      if(this->MAC[i] != obj.MAC[i])
        return false;

    return true;
  }
};
}//namespace
#endif /* ZOONODE_H_ */
