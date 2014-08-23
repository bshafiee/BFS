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

#ifndef ENDPOINT_H_
#define ENDPOINT_H_

#include <iostream>
#include <json/json.h>

namespace Swift {

class Endpoint {
  std::string adminURL;
  std::string id;
  std::string internalURL;
  std::string publicURL;
  std::string region;

public:
  Endpoint();
  virtual ~Endpoint();
  static Endpoint* fromJSON(const Json::Value &val);
  static Json::Value* toJSON(const Endpoint& instance);
  //Getter Setters
  const std::string& getAdminUrl() const;
  void setAdminUrl(const std::string& adminUrl);
  const std::string& getId() const;
  void setId(const std::string& id);
  const std::string& getInternalUrl() const;
  void setInternalUrl(const std::string& internalUrl);
  const std::string& getPublicUrl() const;
  void setPublicUrl(const std::string& publicUrl);
  const std::string& getRegion() const;
  void setRegion(const std::string& region);
};

} /* namespace Swift */
#endif /* ENDPOINT_H_ */
