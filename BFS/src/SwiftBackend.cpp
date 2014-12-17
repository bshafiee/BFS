/*
 * SwiftBackend.cpp
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "SwiftBackend.h"
#include "LoggerInclude.h"
#include <Swift/SwiftResult.h>
#include <Swift/Object.h>
#include <Swift/Container.h>
#include <Swift/Logger.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include "filesystem.h"
#include "UploadQueue.h"
#include "SettingManager.h"

using namespace std;
using namespace Swift;

namespace FUSESwift {

SwiftBackend::SwiftBackend():Backend(BackendType::SWIFT),account(nullptr),defaultContainer(nullptr) {}

SwiftBackend::~SwiftBackend() {
  if(account)
    delete this->account;
  if(defaultContainer)
    delete this->defaultContainer;
}

bool SwiftBackend::initialize(Swift::AuthenticationInfo* _authInfo) {
  if(_authInfo == nullptr)
    return false;
  //Init logger
  if(!SettingManager::getBool(CONFIG_KEY_DEBUG_SWIFT_CPP_SDK))
    Logger::setLogStreams(Logger::null_stream,Logger::null_stream,
      Logger::null_stream,Logger::null_stream);
  else
    Logger::setLogStreams(std::cerr,std::cerr,std::cerr,std::cerr);
  SwiftResult<Account*>* res = Account::authenticate(*_authInfo,true);
  if(res->getError().code != SwiftError::SWIFT_OK) {
    LOG(ERROR)<<"SwiftError: "<<res->getError().toString();
    return false;
  }

  account = res->getPayload();
  return initDefaultContainer();
}

bool SwiftBackend::initDefaultContainer() {
  SwiftResult<vector<Container>*>* res = account->swiftGetContainers(true);
  if(res->getError().code != SwiftError::SWIFT_OK) {
      LOG(ERROR)<<"SwiftError: "<<res->getError().toString();
      return false;
  }
  for(auto it = res->getPayload()->begin(); it != res->getPayload()->end();it++)
    if((*it).getName() == DEFAULT_CONTAINER_NAME)
      defaultContainer = &(*it);
  if(defaultContainer!=nullptr){
    LOG(INFO)<<DEFAULT_CONTAINER_NAME<<" exist with:"<<defaultContainer->getTotalObjects();
    return true;
  }
  //create default container
  defaultContainer = new Container(account,DEFAULT_CONTAINER_NAME);
  SwiftResult<int*>* resCreateContainer = defaultContainer->swiftCreateContainer();
  bool result = resCreateContainer->getError().code == SwiftError::SWIFT_OK;
  if(!result)
    LOG(INFO)<<"Failed to initiate default container: "<<resCreateContainer->getError().toString();

  delete resCreateContainer;
  resCreateContainer = nullptr;

  return result;
}

bool SwiftBackend::put(const SyncEvent* _putEvent) {
  if(_putEvent == nullptr || account == nullptr
      || defaultContainer == nullptr || _putEvent->fullPathBuffer == "")
    return false;
  FileNode* node = FileSystem::getInstance().findAndOpenNode(_putEvent->fullPathBuffer);
  if(node == nullptr)
    return false;
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  LOG(DEBUG)<<"GOT: "<<node<<" to upload!How many Open?"<<node->concurrentOpen()<<" name:"<<_putEvent->fullPathBuffer;

  SwiftResult<vector<Object>*>* res = defaultContainer->swiftGetObjects();
  Object *obj = nullptr;
  if(res->getPayload() != nullptr)
    for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++)
      if((*it).getName() == convertToSwiftName(node->getFullPath())) {
        obj = &(*it);
      }

  //CheckEvent validity
  if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)) {
    delete res;
    //release file delete lock, so they can delete it
    node->close(inodeNum);
    return false;
  }
  //If file is open just return! because if it's a write this is a waste if
  //it's a read this will slow it down!
  if(node->concurrentOpen() > 1){
    //release file delete lock, so they can delete it
    node->setNeedSync(true);//Reschedule again
    delete res;
    node->close(inodeNum);
    return false;
  }


  bool shouldDeleteOBJ = false;
  //Check if Obj already exist
  if(obj != nullptr) {
    //check MD5
    /**
     * when we are here we defenitely need update so it's vain to calculate MD5
     * which is both time consuming and need write lock and kills
     * write performance;threfore I just got rid of it.
     */
    /*if(obj->getHash() == node->getMD5()) {//No change
      LOG(ERROR)<<"Sync: File:"<<node->getFullPath()<<
          " did not change with compare to remote version."<<obj->getHash();

      delete res;
      node->isUPLOADING  = false;
      node->close(inodeNum);
      return true;
    }*/
  }
  else {
    shouldDeleteOBJ = true;
    obj = new Object(defaultContainer,convertToSwiftName(node->getFullPath()));
  }

  //upload chunk by chunk
  //Make a back up of file name in case it gets deleted while uploading
  string nameBackup = node->getName();
  ostream *outStream = nullptr;

  if(node->mustBeDeleted()){
    delete res;
    if(shouldDeleteOBJ)delete obj;
    node->close(inodeNum);
    return false;
  }


  SwiftResult<Poco::Net::HTTPClientSession*> *chunkedResult =
      obj->swiftCreateReplaceObject(outStream);
  if(chunkedResult->getError().code != SwiftError::SWIFT_OK) {
    delete chunkedResult;
    if(shouldDeleteOBJ) delete obj;
    delete res;
    //release file delete lock, so they can delete it
    node->close(inodeNum);
    return false;
  }

  //Ready to write (write each time a blocksize)
  uint64_t buffSize = 1024l*1024l*10l;
  char *buff = new char[buffSize];//10MB buffer
  size_t offset = 0;
  long read = 0;
  //FileNode *node = _putEvent->node;
  do {
    //CheckEvent validity
    if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)){
      delete chunkedResult;
      if(shouldDeleteOBJ) delete obj;
      delete res;
      delete []buff;
      buff = nullptr;
      node->close(inodeNum);
      return false;
    }

    if(node->mustBeDeleted()){//Check Delete
      delete chunkedResult;
      delete res;
      if(shouldDeleteOBJ) delete obj;
      delete []buff;
      buff = nullptr;
      node->close(inodeNum);
      return false;
    }

    //get lock delete so file won't be deleted
    read = node->read(buff,offset,buffSize);


    offset += read;
    outStream->write(buff,read);
  }
  while(read > 0);

  delete []buff;
  buff = nullptr;

  if(node->mustBeDeleted()){//Check Delete
    delete chunkedResult;
    delete res;
    if(shouldDeleteOBJ) delete obj;
    node->close(inodeNum);
    return false;
  }

  //Now send object
  Poco::Net::HTTPResponse response;
  chunkedResult->getPayload()->receiveResponse(response);
  if(response.getStatus() == response.HTTP_CREATED) {
    delete chunkedResult;
    delete res;
    if(shouldDeleteOBJ) delete obj;
    node->close(inodeNum);
    return true;
  }
  else {
    LOG(ERROR)<<"Error in swift: "<<response.getReason();
    delete chunkedResult;
    delete res;
    if(shouldDeleteOBJ) delete obj;
    node->close(inodeNum);
    return false;
  }
  LOG(ERROR)<<"\n\n\nGOSHHHHHHHH UNREACHABLEEEEEEEEEEEEEEEEE\n\n\n\n555555555555555\nZZZ\n";
  return false;
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

std::vector<BackendItem>* FUSESwift::SwiftBackend::list() {
	if(account == nullptr || defaultContainer == nullptr)
		return nullptr;
	SwiftResult<vector<Object>*>* res = defaultContainer->swiftGetObjects();
	if(res->getError().code != SwiftError::SWIFT_OK) {
	  LOG(ERROR)<<"Error in getting list of files in Swiftbackend:"<<res->getError().toString();
		return nullptr;
	}
	vector<BackendItem>* listFiles = new vector<BackendItem>();
	for(auto it = res->getPayload()->begin();it != res->getPayload()->end();it++) {
	  //Check if this object already is downloaded
	  FileNode* node =
	      FileSystem::getInstance().findAndOpenNode(convertFromSwiftName((*it).getName()));
	  //TODO check last modified time not MD5
	  //if(node!=nullptr && node->getMD5() == (*it)->getHash())
	  if(node!=nullptr){
	    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
	    node->close(inodeNum);
	    continue;//existing node
	  }
	  else {
	    BackendItem item(convertFromSwiftName(it->getName()),it->getLength(),it->getHash(),it->getLastModified());
	    listFiles->push_back(item);
	  }
	}

	delete res;
	res = nullptr;

	return listFiles;
}

bool SwiftBackend::remove(const SyncEvent* _removeEvent) {
  if(_removeEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
      return false;
  Object obj(defaultContainer,convertToSwiftName(_removeEvent->fullPathBuffer));
  Swift::SwiftResult<std::istream*>* delResult = obj.swiftDeleteObject();
  /*LOG(ERROR)<<"Sync: remove fullpathBuffer:"<< _removeEvent->fullPathBuffer<<
      " SwiftName:"<< obj.getName()<<" httpresponseMsg:"<<
      delResult->getResponse()->getReason();*/
  bool result = delResult->getError().code == SwiftError::SWIFT_OK;

  if(!result)
    LOG(ERROR)<<"Deleting:"<< _removeEvent->fullPathBuffer<<
          " SwiftName:"<< obj.getName()<<" httpresponseMsg:"<<
          "failed:Responese:"<< delResult->getResponse()->getReason()<<
          " Error:"<<delResult->getError().toString();
  else
    LOG(INFO)<<"SUCCESSFUL DELETE:"<<_removeEvent->fullPathBuffer;

  delete delResult;
  delResult = nullptr;

  return result;
}

std::pair<std::istream*,intptr_t> SwiftBackend::get(const SyncEvent* _getEvent) {
  if(_getEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
    return make_pair(nullptr,0);
  //Try to download object
  Object obj(defaultContainer,convertToSwiftName(_getEvent->fullPathBuffer));
  SwiftResult<std::istream*>* res = obj.swiftGetObjectContent();
  if(res->getError().code != SwiftError::SWIFT_OK) {
    LOG(ERROR)<<"Swift Error: Downloading obj:"<<res->getError().toString();
    return make_pair(nullptr,0);
  }
  else
    return make_pair(res->getPayload(),(intptr_t)res);
}

vector<pair<string,string>>* SwiftBackend::get_metadata(const SyncEvent* _getMetaEvent) {
  if(_getMetaEvent == nullptr || account == nullptr
        || defaultContainer == nullptr)
      return nullptr;
  //Try to download object
  Object obj(defaultContainer,_getMetaEvent->fullPathBuffer);
  return obj.getExistingMetaData();
}

void SwiftBackend::releaseGetData(intptr_t &_ptr) {
  if(_ptr)
    delete (SwiftResult<std::istream*>*)_ptr;
  _ptr = 0;
}

} /* namespace FUSESwift */
