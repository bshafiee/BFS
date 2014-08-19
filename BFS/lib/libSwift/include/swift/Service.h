/*
 * Service
 *
 *  Created on: 2014-05-29
 *      Author: Behrooz Shafiee Sarjaz
 */

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
