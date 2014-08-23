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
SwiftResult<T>* returnNullError(const std::string &whatsNull);

} /* namespace Swift */

#endif /* HTTPIO_H_ */
