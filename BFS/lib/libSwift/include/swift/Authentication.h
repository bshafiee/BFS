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
