/*
 * Endpoint.h
 *
 *  Created on: 2014-06-02
 *      Author: Behrooz Shafiee Sarjaz
 */

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
