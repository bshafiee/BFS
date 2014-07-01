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
#include <iostream>
#include <ErrorNo.h>

namespace Swift {

template <class T>
class SwiftResult {
  Poco::Net::HTTPResponse *response;
  SwiftError error;
  /** Real Data **/
  T payload;

public:
  SwiftResult():response(nullptr), error(SwiftError::SWIFT_OK,"SWIFT_OK")  {
  }

  virtual ~SwiftResult()  {
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
};

} /* namespace Swift */

#endif /* SWIFTRESULT_H_ */
