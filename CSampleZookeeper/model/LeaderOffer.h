/*
 * LeaderOffer.h
 *
 *  Created on: Sep 22, 2014
 *      Author: behrooz
 */

#ifndef LEADEROFFER_H_
#define LEADEROFFER_H_
#include <string>

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
};

#endif /* LEADEROFFER_H_ */
