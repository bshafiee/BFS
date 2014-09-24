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

  ZooNode(std::string _hostName,unsigned long _freeSpace,
  				std::vector<std::string> _containedFiles):hostName(_hostName),
					freeSpace(_freeSpace),containedFiles(_containedFiles) { }

  std::string toString() {
  	std::stringstream output;
  	output << hostName<< "\n";
  	output << freeSpace;
  	if(containedFiles.size() > 0)
  		output << "\n";

		for(unsigned int i =0;i<containedFiles.size();i++) {
			if(i==containedFiles.size()-1)
				output << containedFiles[i];
			else
				output << containedFiles[i] << "\n";
		}
  	return output.str();
  }
};
}//namespace
#endif /* ZOONODE_H_ */
