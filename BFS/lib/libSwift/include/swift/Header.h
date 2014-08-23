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
