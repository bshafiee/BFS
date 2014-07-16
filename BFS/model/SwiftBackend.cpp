/*
 * SwiftBackend.cpp
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "SwiftBackend.h"
#include "../log.h"
#include <Object.h>
#include <Container.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include "filesystem.h"

using namespace std;
using namespace Swift;

namespace FUSESwift {

SwiftBackend::SwiftBackend():Backend(BackendType::SWIFT),account(nullptr),defaultContainer(nullptr) {}

SwiftBackend::~SwiftBackend() {
  // TODO Auto-generated destructor stub
}

bool SwiftBackend::initialize(Swift::AuthenticationInfo* _authInfo) {
  if(_authInfo == nullptr)
    return false;
  SwiftResult<Account*>* res = Account::authenticate(*_authInfo,true);
  if(res->getError().code != SwiftError::SWIFT_OK) {
    log_msg("SwiftError: %s\n",res->getError().msg.c_str());
    return false;
  }

  account = res->getPayload();
  return initDefaultContainer();
}

bool SwiftBackend::initDefaultContainer() {
  SwiftResult<vector<Container*>*>* res = account->swiftGetContainers(true);
  if(res->getError().code != SwiftError::SWIFT_OK) {
      log_msg("SwiftError: %s\n",res->getError().msg.c_str());
      return false;
  }
  for(auto it = res->getPayload()->begin(); it != res->getPayload()->end();it++)
    if((*it)->getName() == DEFAULT_CONTAINER_NAME)
      defaultContainer = *it;
  if(defaultContainer!=nullptr)
    return true;
  //create default container
  defaultContainer = new Container(account,DEFAULT_CONTAINER_NAME);
  SwiftResult<void*>* resCreateContainer = defaultContainer->swiftCreateContainer();
  if(resCreateContainer->getError().code == SwiftError::SWIFT_OK)
    return true;
  else
    return false;
}

bool SwiftBackend::put(SyncEvent* _putEvent) {
  if(_putEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
    return false;
  SwiftResult<vector<Object*>*>* res = defaultContainer->swiftGetObjects();
  Object *obj = nullptr;
  for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++)
    if((*it)->getName() == _putEvent->node->getFullPath()) {
      obj = *it;
    }
  //Check if Obj already exist
  if(obj != nullptr) {
    //check MD5
    if(obj->getHash() == _putEvent->node->getMD5()) {//No change
      log_msg("Sync: File:%s did not change with compare to remote version, MD5:%s\n",
          _putEvent->node->getFullPath().c_str(),_putEvent->node->getMD5().c_str());
      return true;
    }
  }
  else
    obj = new Object(defaultContainer,_putEvent->node->getFullPath());

  //upload chunk by chunk
  ostream *outStream = nullptr;
  SwiftResult<Poco::Net::HTTPClientSession*> *chunkedResult =
      obj->swiftCreateReplaceObject(outStream);
  if(chunkedResult->getError().code != SwiftError::SWIFT_OK) {
    delete outStream;
    delete obj;
    return false;
  }
  //Ready to write (write each time a blocksize)
  char *buff = new char[FileSystem::blockSize];
  size_t offset = 0;
  long read = 0;
  while((read = _putEvent->node->read(buff,offset,FileSystem::blockSize)) > 0) {
    offset += read;
    outStream->write(buff,read);
  }
  log_msg("Sync: File:%s sent:%zu bytes, filesize:%zu\n",
      _putEvent->node->getFullPath().c_str(),offset,_putEvent->node->getSize());
  //Now send object
  Poco::Net::HTTPResponse response;
  chunkedResult->getPayload()->receiveResponse(response);
  if(response.getStatus() == response.HTTP_CREATED) {
    delete []buff;
    return true;
  }
  else {
    log_msg("Error in swift: %s\n",response.getReason().c_str());
    return false;
  }
}

bool SwiftBackend::put_metadata(SyncEvent* _removeEvent) {
}

bool SwiftBackend::move(SyncEvent* _moveEvent) {
}

bool SwiftBackend::remove(SyncEvent* _moveEvent) {
  if(_moveEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
      return false;
  Object obj(defaultContainer,_moveEvent->node->getFullPath());
  SwiftResult<std::istream*>* delResult = obj.swiftDeleteObject();
  if(delResult->getError().code != SwiftError::SWIFT_OK)
    return false;
  else
    return true;
}

} /* namespace FUSESwift */
