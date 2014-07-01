/*
 * Authentication.h
 *
 *  Created on: 2014-05-28
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef AUTHENTICATION_H_
#define AUTHENTICATION_H_

#include <cstdio>
#include <iostream>

namespace Swift {

/**
 * The method of authentication. Various options:
 * <ul>
 *     <li>
 *         <b>BASIC</b>; authenticate against Swift itself. Authentication URL, username and password
 *         must be passed.
 *     </li>
 *     <li>
 *         <b>TEMPAUTH</b>; authenticate against Swift itself. Authentication URL, username and password
 *         must be passed.
 *     </li>
 *     <li>
 *         <b>KEYSTONE</b> (default); makes use of OpenStack Compute. Authentication URL, username and
 *         password must be passed. Ideally, tenant ID and/or name are passed as well. API can auto-
 *         discover the tenant if none is passed and if it can be resolved (one tenant for user).
 *     </li>
 * </ul>
 */
enum class AuthenticationMethod {
  BASIC, TEMPAUTH, KEYSTONE
};

struct AuthenticationInfo {

  /**
   * The ObjectStore username
   */
  std::string username = "";
  /**
   * The ObjectStore password
   */
  std::string password = "";
  /**
   * The ObjectStore authentication URL (Keystone)
   */
  std::string authUrl = "";
  std::string tenantName = "";
  AuthenticationMethod method = AuthenticationMethod::KEYSTONE;
};

inline std::string authenticationMethodToString(AuthenticationMethod method) {
  switch (method) {
  case AuthenticationMethod::BASIC:
    return "BASIC";
  case AuthenticationMethod::TEMPAUTH:
    return "TEMPAUTH";
  case AuthenticationMethod::KEYSTONE:
    return "KEYSTONE";
  default:
    return "UNKNOWN";
  }
}

}

#endif /* AUTHENTICATION_H_ */
