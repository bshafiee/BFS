/*
 * SwiftResult.h
 *
 *  Created on: 2014-05-28
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef SWIFTRESULT_H_
#define SWIFTRESULT_H_

//#include <Poco/Net/HTTPClientSession.h>
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

    //std::cout <<"DESTRUCTOR SWIFTRESULT"<<std::endl;
  }

  const SwiftError& getError() const {
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
