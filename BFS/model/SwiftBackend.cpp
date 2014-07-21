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
    if((*it)->getName() == convertToSwiftName(_putEvent->node->getFullPath())) {
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
    obj = new Object(defaultContainer,convertToSwiftName(_putEvent->node->getFullPath()));

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
  log_msg("Sync: File:%s sent:%zu bytes, filesize:%zu, MD5:%s ObjName:%s\n",
      _putEvent->node->getFullPath().c_str(),offset,_putEvent->node->getSize(),
      _putEvent->node->getMD5().c_str(),obj->getName().c_str());
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

bool SwiftBackend::put_metadata(SyncEvent* _putMetaEvent) {
  return false;
}

bool SwiftBackend::move(SyncEvent* _moveEvent) {
  /*
  if(_moveEvent == nullptr || account == nullptr
        || defaultContainer == nullptr)
    return false;
  SwiftResult<vector<Object*>*>* res = defaultContainer->swiftGetObjects();
  Object *obj = nullptr;
  for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++)
    if((*it)->getName() == convertToSwiftName(_moveEvent->fullPathBuffer)) {
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
    obj = new Object(defaultContainer,convertToSwiftName(_putEvent->node->getFullPath()));*/
  return false;
}

std::string FUSESwift::SwiftBackend::convertToSwiftName(
    const std::string& fullPath) {
  if(fullPath.length() == 0)
    return "";
  else//remove leading '/'
    return fullPath.substr(1,fullPath.length()-1);
}

std::string FUSESwift::SwiftBackend::convertFromSwiftName(
		const std::string& swiftPath) {
	if(swiftPath.length() == 0)
		return "";
	else
		return FileSystem::delimiter+swiftPath;
}

vector<string>* FUSESwift::SwiftBackend::list() {
	if(account == nullptr || defaultContainer == nullptr)
		return nullptr;
	SwiftResult<vector<Object*>*>* res = defaultContainer->swiftGetObjects();
	if(res->getError().code != SwiftError::SWIFT_OK)
		return nullptr;
	vector<string>* listFiles = new vector<string>();
	for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++) {
	  //Check if this object already is downloaded
	  FileNode* node =
	      FileSystem::getInstance()->getNode(convertFromSwiftName((*it)->getName()));
	  if(node!=nullptr && node->getMD5() == (*it)->getHash())
	    continue;//existing node
	  else
	    listFiles->push_back(convertFromSwiftName((*it)->getName()));
	}
	return listFiles;
}

bool SwiftBackend::remove(SyncEvent* _removeEvent) {
  if(_removeEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
      return false;
  Object obj(defaultContainer,convertToSwiftName(_removeEvent->fullPathBuffer));
  SwiftResult<std::istream*>* delResult = obj.swiftDeleteObject();
  log_msg("Sync: remove fullpathBuffer:%s SwiftName:%s httpresponseMsg:%s\n",
      _removeEvent->fullPathBuffer.c_str(),obj.getName().c_str(),
      delResult->getResponse()->getReason().c_str());
  if(delResult->getError().code != SwiftError::SWIFT_OK) {
    log_msg("Error in swift delete: %s\n",delResult->getError().msg.c_str());
    return false;
  }
  else
    return true;
}

istream* SwiftBackend::get(SyncEvent* _getEvent) {
  if(_getEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
    return nullptr;
  //Try to download object
  Object obj(defaultContainer,convertToSwiftName(_getEvent->fullPathBuffer));
  SwiftResult<std::istream*>* res = obj.swiftGetObjectContent();
  if(res->getError().code != SwiftError::SWIFT_OK) {
    log_msg("Swift Error: Downloading obj:%s\n",res->getError().msg.c_str());
    return nullptr;
  }
  else
    return res->getPayload();
}

vector<pair<string,string>>* SwiftBackend::get_metadata(SyncEvent* _getMetaEvent) {
  if(_getMetaEvent == nullptr || account == nullptr
        || defaultContainer == nullptr)
      return nullptr;
  //Try to download object
  Object obj(defaultContainer,_getMetaEvent->fullPathBuffer);
  return obj.getExistingMetaData();
}

} /* namespace FUSESwift */
