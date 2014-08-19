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
#include "UploadQueue.h"

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
  SwiftResult<vector<Container>*>* res = account->swiftGetContainers(true);
  if(res->getError().code != SwiftError::SWIFT_OK) {
      log_msg("SwiftError: %s\n",res->getError().msg.c_str());
      return false;
  }
  for(auto it = res->getPayload()->begin(); it != res->getPayload()->end();it++)
    if((*it).getName() == DEFAULT_CONTAINER_NAME)
      defaultContainer = &(*it);
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

bool SwiftBackend::put(const SyncEvent* _putEvent) {
  if(_putEvent == nullptr || account == nullptr
      || defaultContainer == nullptr || _putEvent->node == nullptr)
    return false;
  //get lock delete so file won't be deleted
  _putEvent->node->lockDelete();

  SwiftResult<vector<Object>*>* res = defaultContainer->swiftGetObjects();
  Object *obj = nullptr;
  if(res->getPayload() != nullptr)
    for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++)
      if((*it).getName() == convertToSwiftName(_putEvent->node->getFullPath())) {
        obj = &(*it);
      }

  //CheckEvent validity
  if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)) {
    //release file delete lock, so they can delete it
    _putEvent->node->unlockDelete();
    return false;
  }
  //Check if Obj already exist
  if(obj != nullptr) {
    //check MD5
    if(obj->getHash() == _putEvent->node->getMD5()) {//No change
      log_msg("Sync: File:%s did not change with compare to remote version, MD5:%s\n",
          _putEvent->node->getFullPath().c_str(),_putEvent->node->getMD5().c_str());
      //release file delete lock, so they can delete it
      _putEvent->node->unlockDelete();
      return true;
    }
  }
  else
    obj = new Object(defaultContainer,convertToSwiftName(_putEvent->node->getFullPath()));

  //upload chunk by chunk
  //Make a back up of file name in case it gets deleted while uploading
  string nameBackup = _putEvent->node->getName();
  ostream *outStream = nullptr;
  SwiftResult<Poco::Net::HTTPClientSession*> *chunkedResult =
      obj->swiftCreateReplaceObject(outStream);
  if(chunkedResult->getError().code != SwiftError::SWIFT_OK) {
    delete outStream;
    delete obj;
    //release file delete lock, so they can delete it
    _putEvent->node->unlockDelete();
    return false;
  }
  //release file delete lock, so they can delete it
  _putEvent->node->unlockDelete();

  //Ready to write (write each time a blocksize)
  char *buff = new char[FileSystem::blockSize];
  size_t offset = 0;
  long read = 0;
  FileNode *node = _putEvent->node;
  do {
    //node = FileSystem::getInstance().getNode(nameBackup);
    //if(node == nullptr)
      //break;
    //CheckEvent validity
    if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)) return false;
    //get lock delete so file won't be deleted
    _putEvent->node->lockDelete();
    read = node->read(buff,offset,FileSystem::blockSize);
    //get lock delete so file won't be deleted
    _putEvent->node->unlockDelete();
    offset += read;
    outStream->write(buff,read);
  }
  while(read > 0);

  if(node == nullptr) {
    log_msg("Sync: File:%s failed due to interferring delete operation\n",nameBackup.c_str());
    return false;
  }
  else {
    //CheckEvent validity
    if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)) return false;
    //get lock delete so file won't be deleted
    _putEvent->node->lockDelete();
    log_msg("Sync: File:%s sent:%zu bytes, filesize:%zu, MD5:%s ObjName:%s\n",
      _putEvent->node->getFullPath().c_str(),offset,_putEvent->node->getSize(),
      _putEvent->node->getMD5().c_str(),obj->getName().c_str());
    //get lock delete so file won't be deleted
    _putEvent->node->unlockDelete();
  }
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

bool SwiftBackend::put_metadata(const SyncEvent* _putMetaEvent) {
  return false;
}

bool SwiftBackend::move(const SyncEvent* _moveEvent) {
  //we need locking here as well like delete!
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
	SwiftResult<vector<Object>*>* res = defaultContainer->swiftGetObjects();
	if(res->getError().code != SwiftError::SWIFT_OK)
		return nullptr;
	vector<string>* listFiles = new vector<string>();
	for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++) {
	  //Check if this object already is downloaded
	  FileNode* node =
	      FileSystem::getInstance().getNode(convertFromSwiftName((*it).getName()));
	  //TODO check last modified time not MD5
	  //if(node!=nullptr && node->getMD5() == (*it)->getHash())
	  if(node!=nullptr)
	    continue;//existing node
	  else
	    listFiles->push_back(convertFromSwiftName((*it).getName()));
	}
	return listFiles;
}

bool SwiftBackend::remove(const SyncEvent* _removeEvent) {
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

istream* SwiftBackend::get(const SyncEvent* _getEvent) {
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

vector<pair<string,string>>* SwiftBackend::get_metadata(const SyncEvent* _getMetaEvent) {
  if(_getMetaEvent == nullptr || account == nullptr
        || defaultContainer == nullptr)
      return nullptr;
  //Try to download object
  Object obj(defaultContainer,_getMetaEvent->fullPathBuffer);
  return obj.getExistingMetaData();
}

} /* namespace FUSESwift */
