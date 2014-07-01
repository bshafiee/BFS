/*
 * ErrorNo.h
 *
 *  Created on: 2014-05-26
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef ERRORNO_H_
#define ERRORNO_H_

#include <iostream>
#include <cstring>

namespace Swift {

struct SwiftError {
  int code;
  std::string msg;
  SwiftError(int _code, std::string _msg);
  std::string toString();
  /** List of Errors **/
  static const int SWIFT_OK = 0; //Successful
  static const int SWIFT_FAIL = -1; //Unsuccessful
  static const int SWIFT_EXCEPTION = -2; //Exception happened
  static const int SWIFT_HTTP_ERROR = -3; //HTTP erro happened
  static const int SWIFT_JSON_PARSE_ERROR = -3; //JSON Parsing Error happened
};

//Always the same message
extern SwiftError SWIFT_OK;

}
#endif /* ERRORNO_H_ */
