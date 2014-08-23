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
