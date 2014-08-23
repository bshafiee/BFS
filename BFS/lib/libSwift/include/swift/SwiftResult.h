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

#ifndef SWIFTRESULT_H_
#define SWIFTRESULT_H_

#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <iostream>
#include <ErrorNo.h>
#include <typeinfo>
#include <type_traits>

namespace Swift {

template <class T>
class SwiftResult {
  Poco::Net::HTTPResponse *response;
  Poco::Net::HTTPClientSession *session;
  SwiftError error;
  /** Real Data **/
  T payload;

public:
  SwiftResult():response(nullptr), session(nullptr), error(SwiftError::SWIFT_OK,"SWIFT_OK")  {
  }

  virtual ~SwiftResult() {
    if(response!=nullptr) {
      delete response;
      response = nullptr;
    }
    if(session!=nullptr) {
      delete session;
      session = nullptr;
    }

    if(payload!=nullptr) {
      //Istream is part of session which is being deleted in the next statement
      if (!std::is_same<T, std::istream*>::value &&
          !std::is_same<T, Poco::Net::HTTPClientSession*>::value)//session as paylod
          delete static_cast<T>(payload);
      payload = nullptr;
    }
  }

  SwiftError getError() const {
    return error;
  }

  void setError(const SwiftError& error) {
    this->error = error;
  }

  T getPayload() const {
    return payload;
  }

  void setPayload(T payload) {
    this->payload = payload;
  }

  Poco::Net::HTTPResponse* getResponse() const {
    return response;
  }

  void setResponse(Poco::Net::HTTPResponse* response) {
    this->response = response;
  }

  Poco::Net::HTTPClientSession* getSession() const {
    return session;
  }

  void setSession(Poco::Net::HTTPClientSession* _session) {
    this->session = _session;
  }
};

} /* namespace Swift */

#endif /* SWIFTRESULT_H_ */
