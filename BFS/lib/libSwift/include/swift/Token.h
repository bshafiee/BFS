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
#ifndef TOKEN_H_
#define TOKEN_H_

#include <iostream>
#include <Tenant.h>
#include <json/json.h>

namespace Swift {

class Token {
private:
  std::string expires;
  std::string id;
  std::string issued_at;
  Tenant *tenant;

public:
  Token();
  Token(const std::string &_expires,const std::string &id,const std::string &issued_at,Tenant *tenant);
  virtual ~Token();
  static Token* fromJSON(const Json::Value &val);
  static Json::Value* toJSON(const Token& instance);
  //Getter Setters
  const std::string& getExpires() const;
  void setExpires(const std::string& expires);
  const std::string& getId() const;
  void setId(const std::string& id);
  const std::string& getIssuedAt() const;
  void setIssuedAt(const std::string& issuedAt);
  Tenant* getTenant() const;
  void setTenant(Tenant* tenant);
  Token & operator=(const Token &other);
};

} /* namespace Swift */
#endif /* TOKEN_H_ */
