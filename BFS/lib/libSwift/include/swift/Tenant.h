/*
 * Tenant.h
 *
 *  Created on: 2014-05-28
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef TENANT_H_
#define TENANT_H_

#include <iostream>
#include <json/json.h>

namespace Swift {

class Tenant {
private:
  std::string id;
  std::string name;
  std::string description;
  bool enabled;

public:
  Tenant();
  Tenant(const std::string &_id, const std::string &_name, const std::string &_description, bool _enabled);
  virtual ~Tenant();
  static Tenant* fromJSON(const Json::Value &val);
  static Json::Value* toJSON(const Tenant &instance);
  //Getters and Setters
  const std::string& getDescription() const;
  void setDescription(const std::string& description);
  bool isEnabled() const;
  void setEnabled(bool enabled);
  const std::string& getId() const;
  void setId(const std::string& id);
  const std::string& getName() const;
  void setName(const std::string& name);
};

} /* namespace Swift */
#endif /* TENANT_H_ */
