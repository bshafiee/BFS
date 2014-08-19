/*
 * Header.h
 *
 *  Created on: 2014-06-04
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef HEADER_H_
#define HEADER_H_

#include <iostream>

namespace Swift {

struct HTTPHeader {
private:
  std::pair<std::string,std::string> pair;
public:
  HTTPHeader(std::string key,std::string value);
  virtual ~HTTPHeader();
  /** Methods **/
  std::string getKey();
  std::string getValue();
  std::string getQueryValue();
};


/** Common HTTP Headers **/
extern HTTPHeader HEADER_FORMAT_APPLICATION_JSON;
extern HTTPHeader HEADER_FORMAT_APPLICATION_XML;
extern HTTPHeader HEADER_FORMAT_TEXT_XML;

} /* namespace Swift */
#endif /* HEADER_H_ */
