/*
 * HTTPIO.h
 *
 *  Created on: 2014-05-27
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef HTTPIO_H_
#define HTTPIO_H_

#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/SocketStream.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/HashMap.h>
#include <Poco/URI.h>
#include <iostream>
#include <vector>
#include <Header.h>
#include <SwiftResult.h>
#include <Account.h>

namespace Swift {

Poco::Net::HTTPClientSession* doHTTPIO(const Poco::URI &uri,
    const std::string &type, std::vector<HTTPHeader> *params);
Poco::Net::HTTPClientSession* doHTTPIO(const Poco::URI &uri,
    const std::string &type, std::vector<HTTPHeader> *params,
    const std::string &reqBody, const std::string &contentType);
Poco::Net::HTTPClientSession* doHTTPIO(const Poco::URI &uri,
    const std::string &type, std::vector<HTTPHeader> *params,
    const char* reqBody, ulong size, const std::string& contentType);
Poco::Net::HTTPClientSession* doHTTPIO(const Poco::URI &uri,
    const std::string &type, std::vector<HTTPHeader> *params,
    std::ostream* &outputStream);

template<class T>
SwiftResult<T>* doSwiftTransaction(Account *_account, std::string &_uriPath,
    const std::string &_method, std::vector<HTTPHeader>* _uriParams,
    std::vector<HTTPHeader>* _reqMap, std::vector<int> *_httpValidCodes,
    const char *bodyReqBuffer = nullptr, ulong size = 0,
    std::string *contentType = nullptr);

template<class T>
inline SwiftResult<T>* returnNullError(const std::string &whatsNull);

} /* namespace Swift */

#endif /* HTTPIO_H_ */
