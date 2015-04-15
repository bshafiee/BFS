/**********************************************************************
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
**********************************************************************/

#include "SwiftBackend.h"
#include "LoggerInclude.h"
#include <Swift/SwiftResult.h>
#include <Swift/Object.h>
#include <Swift/Container.h>
#include <Swift/Logger.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>

#include "Filesystem.h"
#include "UploadQueue.h"
#include "SettingManager.h"

using namespace std;
using namespace Swift;

namespace BFS {

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
  if(node == nullptr){
    LOG(ERROR)<<_putEvent->fullPathBuffer<<" Does not exist! So can't upload it.";
    return false;
  }
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);
  //LOG(DEBUG)<<"GOT: "<<node<<" to upload!How many Open?"<<node->concurrentOpen()<<" name:"<<_putEvent->fullPathBuffer;
  //Double check if not already uploaded
  if(node->flushed()){
    node->close(inodeNum);
    return true;
  }

  //CheckEvent validity
  if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)) {
    //delete res;
    //release file delete lock, so they can delete it
    node->close(inodeNum);
    return true;//going to be deletes so anyway say it's synced!
  }

  Object* obj = new Object(defaultContainer,convertToSwiftName(node->getFullPath()));

  //upload chunk by chunk
  //Make a back up of file name in case it gets deleted while uploading
  string nameBackup = node->getName();
  ostream *outStream = nullptr;

  if(node->mustBeDeleted()){
    delete obj;
    node->close(inodeNum);
    return true;
  }


  SwiftResult<Poco::Net::HTTPClientSession*> *chunkedResult =
      obj->swiftCreateReplaceObject(outStream);
  if(chunkedResult->getError().code != SwiftError::SWIFT_OK) {
    LOG(ERROR)<<"Error in creating/replacing object:"<<chunkedResult->getError().toString();
    delete chunkedResult;
    delete obj;
    //release file delete lock, so they can delete it
    node->close(inodeNum);
    return false;
  }

  chunkedResult->getPayload()->setTimeout(Poco::Timespan(5000,0));
  chunkedResult->getPayload()->setKeepAlive(true);


  //Ready to write (write each time a blocksize)
  uint64_t buffSize = 1024ll*1024ll*10ll;
  char *buff = new char[buffSize];//10MB buffer
  size_t offset = 0;
  long read = 0;
  //FileNode *node = _putEvent->node;
  do {
    //CheckEvent validity
    if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)){
      delete chunkedResult;
      delete obj;
      delete []buff;
      buff = nullptr;
      node->close(inodeNum);
      return true;
    }

    if(node->mustBeDeleted()){//Check Delete
      delete chunkedResult;
      delete obj;
      delete []buff;
      buff = nullptr;
      node->close(inodeNum);
      return true;
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
    delete obj;
    node->close(inodeNum);
    return true;
  }

  //Now send object
  Poco::Net::HTTPResponse response;
  response.setKeepAlive(true);

  try {
    chunkedResult->getPayload()->receiveResponse(response);
  }catch(Poco::TimeoutException &e){
    LOG(ERROR)<<"Poco connection timeout:"<<e.message()<<" timeout(getSeesion):"<<
        chunkedResult->getSession()->getTimeout().totalSeconds()<<
        " payload timeout"<<chunkedResult->getPayload()->getTimeout().totalSeconds();
    delete chunkedResult;
    delete obj;
    node->close(inodeNum);
    return false;
  }

  if(response.getStatus() == response.HTTP_CREATED) {
    delete chunkedResult;
    delete obj;
    node->close(inodeNum);
    return true;
  }
  else {
    LOG(ERROR)<<"Error in swift: "<<response.getReason()<<" Status:"<<response.getStatus();
    response.write(cerr);
    delete chunkedResult;
    delete obj;
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

std::string BFS::SwiftBackend::convertToSwiftName(
    const std::string& fullPath) {
  if(fullPath.length() == 0)
    return "";
  else//remove leading '/'
    return fullPath.substr(1,fullPath.length()-1);
}

std::string BFS::SwiftBackend::convertFromSwiftName(
		const std::string& swiftPath) {
	if(swiftPath.length() == 0)
		return "";
	else
		return FileSystem::delimiter+swiftPath;
}

bool BFS::SwiftBackend::list(std::vector<BackendItem>& list) {
	if(account == nullptr || defaultContainer == nullptr)
		return false;
	SwiftResult<vector<Object>*>* res = defaultContainer->swiftGetObjects();
	if(res->getError().code != SwiftError::SWIFT_OK) {
	  LOG(ERROR)<<"Error in getting list of files in Swiftbackend:"<<res->getError().toString();
		return nullptr;
	}
	//vector<BackendItem>* listFiles = new vector<BackendItem>();
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
	    list.emplace_back(BackendItem(convertFromSwiftName(it->getName()),it->getLength(),it->getHash(),it->getLastModified()));
	  }
	}

	delete res;
	res = nullptr;

	return true;
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
//Deleting:/blue15 SwiftName:blue15 httpresponseMsg:failed:Responese:Not Found Error:Error -3: Code:404 Reason:Not Found
  if(!result){
    if(delResult->getResponse()->getReason() == Poco::Net::HTTPResponse::HTTP_REASON_NOT_FOUND)//Not found
      LOG(DEBUG)<<"Deleting:"<< _removeEvent->fullPathBuffer<<
          " SwiftName:"<< obj.getName()<<" httpresponseMsg:"<<
          "failed:Responese:"<< delResult->getResponse()->getReason()<<
          " Error:"<<delResult->getError().toString();
    else
      LOG(ERROR)<<"Deleting:"<< _removeEvent->fullPathBuffer<<
          " SwiftName:"<< obj.getName()<<" httpresponseMsg:"<<
          "failed:Responese:"<< delResult->getResponse()->getReason()<<
          " Error:"<<delResult->getError().toString();
  }
  else
    LOG(DEBUG)<<"SUCCESSFUL DELETE:"<<_removeEvent->fullPathBuffer;

  delete delResult;
  delResult = nullptr;

  return result;
}

bool SwiftBackend::get(const SyncEvent* _getEvent) {
  if(_getEvent == nullptr || account == nullptr
      || defaultContainer == nullptr)
    return false;
  if(_getEvent->fullPathBuffer.length()==0)
    return false;

  //Try to download object
  Object obj(defaultContainer,convertToSwiftName(_getEvent->fullPathBuffer));
  SwiftResult<std::istream*>* res = obj.swiftGetObjectContent();
  if(res->getError().code != SwiftError::SWIFT_OK) {
    LOG(ERROR)<<"Swift Error in Downloading obj:"<<res->getError().toString();
    if(res)
      delete res;
    return false;
  }
  if(res==nullptr || res->getPayload() == nullptr){
    LOG(ERROR)<<"Error in downloading object conentent:"<<obj.getName();
    if(res)
      delete res;
  }

  FileNode* fileNode = FileSystem::getInstance().findAndOpenNode(_getEvent->fullPathBuffer);
  //If File exist then we won't download it!
  if(fileNode!=nullptr){
    LOG(DEBUG)<<"File "<<fileNode->getFullPath()<<" already exist! no need to download.";
    //Close it! so it can be removed if needed
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fileNode);
    fileNode->close(inodeNum);
    delete res;
    return false;
  }

  //Now create a file in FS
  //handle directories
  //FileSystem::getInstance().createHierarchy(_getEvent->fullPathBuffer,false);
  //FileNode *newFile = FileSystem::getInstance().mkFile(_getEvent->fullPathBuffer,false,true);//open
  string name = FileSystem::getInstance().getFileNameFromPath(_getEvent->fullPathBuffer);
  FileNode* newFile = new FileNode(name,_getEvent->fullPathBuffer, false,false);
  if(newFile == nullptr){
    LOG(ERROR)<<"Failed to create a newNode:"<<_getEvent->fullPathBuffer;
    delete res;
    return false;
  }
  //uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)newFile);
  //LOG(DEBUG)<<"DOWNLOADING: ptr:"<<newFile<<" fpath:"<<newFile->getFullPath();

  istream *getStream = res->getPayload();
  //and write the content
  uint64_t bufSize = 64ll*1024ll*1024ll;
  char *buff = new char[bufSize];//64MB buffer
  size_t offset = 0;
  while(!getStream->eof()) {
    getStream->read(buff,bufSize);

    //No need to download it anymore.
    if(newFile->mustBeDeleted()){
      delete newFile;
      delete res;
      return true;
    }

    FileNode* afterMove = nullptr;
    long retCode = newFile->writeHandler(buff,offset,getStream->gcount(),afterMove,true);

    while(retCode == -1)//-1 means moving
      retCode = newFile->writeHandler(buff,offset,getStream->gcount(),afterMove,true);

    if(afterMove){
      newFile = afterMove;
      FileSystem::getInstance().replaceAllInodesByNewNode((intptr_t)newFile,(intptr_t)afterMove);
    }

    //Check space availability
    if(retCode < 0) {
      LOG(ERROR)<<"Error in writing file:"<<newFile->getFullPath()<<", probably no diskspace, Code:"<<retCode;
      delete newFile;
      delete res;
      delete []buff;
      return false;
    }

    offset += getStream->gcount();
  }

  newFile->setNeedSync(false);//We have just created this file so it's upload flag false
  delete res;
  delete []buff;
  //Add it to File system
  if(FileSystem::getInstance().createHierarchy(_getEvent->fullPathBuffer,false)==nullptr){
    LOG(ERROR)<<"Error in creating hierarchy for newly downloaded file:"<<newFile->getFullPath();
    delete newFile;
    return false;
  }
  if(!FileSystem::getInstance().addFile(newFile)){
    LOG(ERROR)<<"Error in adding newly downloaded file:"<<newFile->getFullPath();
    delete newFile;
    return false;
  }
  //Gone well
  return true;
}

vector<pair<string,string>>* SwiftBackend::get_metadata(const SyncEvent* _getMetaEvent) {
  if(_getMetaEvent == nullptr || account == nullptr
        || defaultContainer == nullptr)
      return nullptr;
  //Try to download object
  Object obj(defaultContainer,_getMetaEvent->fullPathBuffer);
  return obj.getExistingMetaData();
}

} /* namespace BFS */
