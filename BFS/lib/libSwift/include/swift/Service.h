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

#ifndef SERVICE_H_
#define SERVICE_H_

#include <iostream>
#include <json/json.h>
#include <Endpoint.h>

namespace Swift {
/**
 *  {
        "endpoints" : [
           {
              "adminURL" : "http://192.168.249.109:35357/v2.0",
              "id" : "173b6f35020040e7b4db0119d3449be0",
              "internalURL" : "http://192.168.249.109:5000/v2.0",
              "publicURL" : "http://192.168.249.109:5000/v2.0",
              "region" : "RegionOne"
           }
        ],
        "endpoints_links" : [],
        "name" : "keystone",
        "type" : "identity"
     }
 */

class Service {
  std::string name;
  std::string type;
  std::vector<Endpoint*> endpoints;

public:
  Service();
  virtual ~Service();
  static Service* fromJSON(const Json::Value &val);
  static Json::Value* toJSON(const Service& instance);
  //Getter Setters
  const std::string& getName() const;
  void setName(const std::string& name);
  const std::string& getType() const;
  void setType(const std::string& type);
  const std::vector<Endpoint*>& getEndpoints() const;
  void setEndpoints(const std::vector<Endpoint*>& endpoints);
  Endpoint* getFirstEndpoint();
};

} /* namespace Swift */
#endif /* SERVICE_H_ */
