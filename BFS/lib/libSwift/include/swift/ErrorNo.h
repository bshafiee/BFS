/**************************************************************************
    This is a general SDK for OpenStack Swift API written in C++
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
**************************************************************************/

#ifndef ERRORNO_H_
#define ERRORNO_H_

#include <iostream>
#include <cstring>

namespace Swift {

struct SwiftError {
  int code;
  std::string msg;
  SwiftError(int _code, std::string _msg);
  const std::string toString();
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
