/*
 * ZooNode.h
 *
 *  Created on: Sep 23, 2014
 *      Author: behrooz
 */

#ifndef ZOONODE_H_
#define ZOONODE_H_
#include <string>
#include <vector>
#include <sstream>
#include "../Backend.h"

namespace FUSESwift {

struct ZooNode {
  std::string hostName;
  unsigned long freeSpace;
  std::vector<std::string> containedFiles;
  unsigned char MAC[6];

  ZooNode(std::string _hostName,unsigned long _freeSpace,
  				std::vector<std::string> _containedFiles,const unsigned char *_mac):hostName(_hostName),
					freeSpace(_freeSpace),containedFiles(_containedFiles) {
  	if(_mac != nullptr)
  		memcpy(MAC,_mac,sizeof(char)*6);
  }

  std::string toString() {
  	std::stringstream output;
  	output << hostName<< "\n";
  	char macBuff[100];
  	sprintf(macBuff,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5]);
  	output << macBuff<< "\n";
  	output << freeSpace;
  	if(containedFiles.size() > 0)
  		output << "\n";

		for(unsigned int i =0;i<containedFiles.size();i++) {
			if(i==containedFiles.size()-1)
				output << containedFiles[i];
			else
				output << containedFiles[i] << "\n";
		}
		//std::string s = output.str();
		//std::cout<<s<<std::endl;
  	return output.str();
  }

  static bool CompByFreeSpaceAsc (const ZooNode& lhs, const ZooNode& rhs) {
  	return lhs.freeSpace < rhs.freeSpace;
  }

  static bool CompByFreeSpaceDes (const ZooNode& lhs, const ZooNode& rhs) {
		return lhs.freeSpace > rhs.freeSpace;
	}
};
}//namespace
#endif /* ZOONODE_H_ */
