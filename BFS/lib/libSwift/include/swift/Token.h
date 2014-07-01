/*
 * Token.h
 *
 *  Created on: 2014-05-28
 *      Author: Behrooz Shafiee Sarjaz
 */

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
};

} /* namespace Swift */
#endif /* TOKEN_H_ */
