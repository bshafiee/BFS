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

#include "GlusterBackend.h"
#include "LoggerInclude.h"
#include "Filesystem.h"
#include "UploadQueue.h"
#include "SettingManager.h"
#include <glusterfs/api/glfs.h>
#include <Poco/StringTokenizer.h>

using namespace std;

namespace FUSESwift {


/*static inline bool fileExists (const std::string& name) {
  struct stat st;
  if(stat(name.c_str(),&st) == 0)
    if(st.st_mode & S_IFREG != 0)
      return true;
  return false;
}

static inline bool dirExists (const std::string& name) {
  struct stat st;
  if(stat(name.c_str(),&st) == 0)
    if(st.st_mode & S_IFDIR != 0)
      return true;
  return false;
}*/

GlusterBackend::GlusterBackend():Backend(BackendType::GLUSTER) {
}

GlusterBackend::~GlusterBackend() {
  glfs_fini((glfs_t*)fs);
}

bool GlusterBackend::initialize(std::string _volume,std::string _volumeServer) {
  GlusterFSConnection conn;
  conn.volumeName = _volume;
  if(_volumeServer.empty())
    return false;
  Poco::StringTokenizer tk(_volumeServer,",",Poco::StringTokenizer::TOK_TRIM |
      Poco::StringTokenizer::TOK_IGNORE_EMPTY);
  for(auto it = tk.begin();it!=tk.end();it++){
    Poco::StringTokenizer sTk(*it,":",Poco::StringTokenizer::TOK_TRIM |
          Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    if(sTk.count()!=3){
      LOG(ERROR)<<"Error while parsing Gluster Volume server:"<<*it;
      continue;
    }
    conn.volumeServers.emplace_back(VolumeServer(sTk[0],stoi(sTk[1]),sTk[2]));
  }

  return initialize(conn);
}

bool GlusterBackend::initialize(GlusterFSConnection _connectionInfo) {
  this->connectionInfo= _connectionInfo;

  //Create
  fs = glfs_new(connectionInfo.volumeName.c_str());
  if(fs==nullptr){
    LOG(ERROR)<<"Error in creating GlusterFS instance.";
    return false;
  }
  //Set volume server
  /**
   *
   * NOTE: This API is special, multiple calls to this function with different
   *  volfile servers, port or transport-type would create a list of volfile
   *  servers which would be polled during `volfile_fetch_attempts()
   *
   * @transport: String specifying the transport used to connect to the
   *  management daemon. Specifying NULL will result in the usage
   *  of the default (tcp) transport type. Permitted values
   *  are those what you specify as transport-type in a volume
   *  specification file (e.g "tcp", "rdma" etc.)
   *
   *  0 : Success.
   * -1 : Failure. @errno will be set with the type of failure.
   **/
  bool allFailed = true;
  for(VolumeServer &volServer:_connectionInfo.volumeServers){
    int ret = glfs_set_volfile_server ((glfs_t*)fs, volServer.transport.c_str(), volServer.serverIP.c_str(), volServer.port);
    //int ret = glfs_set_volfile_server ((glfs_t*)fs, volServer.transport.c_str(), volServer.serverIP.c_str(), 23432);
    //int ret = glfs_set_volfile_server ((glfs_t*)fs, volServer.transport.c_str(), volServer.serverIP.c_str(), );
    if(ret !=0){
      glfs_unset_volfile_server((glfs_t*)fs, volServer.transport.c_str(), volServer.serverIP.c_str(), volServer.port);
      LOG(ERROR)<<"Error in connecting to VolumeServer:"<<volServer.serverIP
          <<":"<<volServer.port<<"("<<volServer.transport<<")"<<" ErrorCode:"<<ret;
    } else
      allFailed = false;
  }

  if(allFailed){
    LOG(ERROR)<<"Couldn't connect to any of Volume Servers. Gluster Initialization failed.";
    glfs_fini((glfs_t*)fs);
    return false;
  }

  //Set Logging info
  if(glfs_set_logging ((glfs_t*)fs, "/dev/stderr", 5)!=0)
    LOG(ERROR)<<"Failed to set logging for GlusterFS.";

  int ret = glfs_init((glfs_t*)fs);
  if(ret!=0){
    LOG(ERROR)<<"Failed to initialize GlusterFS. ErrorCode:"<<ret;
    glfs_fini((glfs_t*)fs);
    return false;
  }

  LOG(INFO)<<"GlusterFS backend initialized for Volume:"<<this->connectionInfo.volumeName;
  /*vector<BackendItem> tmp;
  list(tmp);*/

  return true;
}

bool GlusterBackend::recursiveListDir(const char* path,vector<BackendItem>& _list) {
  glfs_fd_t *dp;
  struct dirent *dirp;
  if((dp  = glfs_opendir((glfs_t*)fs,path)) == NULL) {
      LOG(ERROR)<<"Error in opening"<<path<<" in GlusterFS.ErrorNo:"<<errno;
      return false;
  }

  bool res = true;
  while ((dirp = glfs_readdir(dp)) != NULL) {
    if(dirp->d_type == DT_REG){//File
      struct stat st;
      char newPath[2048];
      int len = 0;
      if(strcmp(path,"/")==0)//root
        len = snprintf(newPath, sizeof(newPath)-1, "%s%s", path, dirp->d_name);
      else
        len = snprintf(newPath, sizeof(newPath)-1, "%s/%s", path, dirp->d_name);
      newPath[len]= 0;
      glfs_stat((glfs_t*)fs,newPath,&st);

      char mtime[1024];
      len = snprintf(mtime,sizeof(mtime),"%lld.%.9ld", (long long)st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
      mtime[len] = 0;
      _list.emplace_back(BackendItem(string(newPath),st.st_size,"",string(mtime)));
    }else if(dirp->d_type == DT_DIR){//Directory
      if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
          continue;
      char newPath[2048];
      int len = 0;
      if(strcmp(path,"/")==0)//root
        len = snprintf(newPath, sizeof(newPath)-1, "%s%s", path, dirp->d_name);
      else
        len = snprintf(newPath, sizeof(newPath)-1, "%s/%s", path, dirp->d_name);
      newPath[len] = 0;
      res = recursiveListDir(newPath, _list);
    }
  }
  glfs_closedir(dp);
  return res;
}


bool GlusterBackend::list(std::vector<BackendItem>& list) {
  bool res = recursiveListDir("/",list);
  /*LOG(INFO)<<"List is:";
  for(BackendItem item:list)
    LOG(INFO)<<item.name;*/
  return res;
}

bool GlusterBackend::get(const SyncEvent* _getEvent) {
  if(_getEvent == nullptr || _getEvent->fullPathBuffer.empty())
    return false;

  //Try to find file
  /**
   * RETURN VALUES
   * NULL   : Failure. @errno will be set with the type of failure.
   * Others : Pointer to the opened glfs_fd_t.
   */
  glfs_fd_t *fd = glfs_open((glfs_t*)fs, _getEvent->fullPathBuffer.c_str(), O_RDONLY);
  if(!fd){
    LOG(ERROR)<<"Error while openeing a handle to:"<<_getEvent->fullPathBuffer;
    return false;
  }

  FileNode* fileNode = FileSystem::getInstance().findAndOpenNode(_getEvent->fullPathBuffer);
  //If File exist then we won't download it!
  if(fileNode!=nullptr){
    LOG(DEBUG)<<"File "<<fileNode->getFullPath()<<" already exist! no need to download.";
    //Close it! so it can be removed if needed
    uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)fileNode);
    fileNode->close(inodeNum);
    glfs_close(fd);
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
    glfs_close(fd);
    return false;
  }

  //and write the content
  uint64_t bufSize = 64ll*1024ll*1024ll;
  char *buff = new char[bufSize];//64MB buffer
  size_t offset = 0;
  int64_t read = 0;
  do {

    //No need to download it anymore.
    if(newFile->mustBeDeleted()){
      delete newFile;
      glfs_close(fd);
      return true;
    }

    read = glfs_pread(fd,buff,bufSize,offset,0);

    if(read < 0){
      LOG(ERROR)<<"Error while reading: "<<_getEvent->fullPathBuffer;
      glfs_close(fd);
      return false;
    }

    if(read>0){
      FileNode* afterMove = nullptr;
      long retCode = newFile->writeHandler(buff,offset,read,afterMove,true);

      while(retCode == -1)//-1 means moving
        retCode = newFile->writeHandler(buff,offset,read,afterMove,true);

      if(afterMove){
        newFile = afterMove;
        FileSystem::getInstance().replaceAllInodesByNewNode((intptr_t)newFile,(intptr_t)afterMove);
      }
      //Check space availability
      if(retCode < 0) {
        LOG(ERROR)<<"Error in writing file:"<<newFile->getFullPath()<<", probably no diskspace, Code:"<<retCode;
        delete newFile;
        glfs_close(fd);
        delete []buff;
        return false;
      }
    }

    offset += read;
  }while(read);

  newFile->setNeedSync(false);//We have just created this file so it's upload flag false
  glfs_close(fd);
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

std::vector<std::pair<std::string, std::string> >* GlusterBackend::get_metadata(
    const SyncEvent* _getMetaEvent) {
  return nullptr;
}

bool GlusterBackend::createDirectory(const char* path){
  if(glfs_mkdir((glfs_t*)fs, path,0775)!=0){
    if(errno == EEXIST)//already exist
      return true;
    LOG(ERROR)<<"Error in creating directory:"<<path<<" erroNo:"<<errno;
    return false;
  }
  return true;
}
bool GlusterBackend::put(const SyncEvent* _putEvent) {
  if(_putEvent == nullptr || _putEvent->fullPathBuffer.empty())
      return false;

  FileNode* node = FileSystem::getInstance().findAndOpenNode(_putEvent->fullPathBuffer);
  if(node == nullptr){
    LOG(ERROR)<<_putEvent->fullPathBuffer<<" Does not exist! So can't upload it.";
    return false;
  }
  uint64_t inodeNum = FileSystem::getInstance().assignINodeNum((intptr_t)node);

  //Double check if not already uploaded
  if(node->flushed()){
    node->close(inodeNum);
    return true;
  }

  //CheckEvent validity
  if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)) {
    //release file delete lock, so they can delete it
    node->close(inodeNum);
    return true;//going to be deletes so anyway say it's synced!
  }

  if(node->mustBeDeleted()){
    node->close(inodeNum);
    return true;
  }

  //Create necessary directories
  //Traverse FileSystem Hierarchies
  Poco::StringTokenizer tokenizer(_putEvent->fullPathBuffer,
      FileSystem::delimiter,Poco::StringTokenizer::TOK_TRIM |
      Poco::StringTokenizer::TOK_IGNORE_EMPTY);
  //Nested directories
  if(tokenizer.count()>=2){
    for(unsigned int i=0;i<tokenizer.count()-1;i++){
      string path = FileSystem::delimiter;
      for(unsigned int j=0;j<=i;j++)
        path += tokenizer[j];
      if(!createDirectory(path.c_str())){
        LOG(ERROR)<<"Failed to create parent directory("<<tokenizer[i]<<") for:"<<_putEvent->fullPathBuffer;
        node->close(inodeNum);
        return false;
      }
    }
  }
  //Create the file! (overwrite or create)
  glfs_fd_t *fd = glfs_creat((glfs_t*)fs, _putEvent->fullPathBuffer.c_str(), O_RDWR, 0644);
  //glfs_fd_t *fd = glfs_open(,O_RDWR| O_CREAT | O_TRUNC);
  if(!fd){
    LOG(ERROR)<<"Error in creating file:"<<_putEvent->fullPathBuffer<<" in GlusterFS. errno:"<<errno;
    return false;
  }

  //Ready to write (write each time a blocksize)
  uint64_t buffSize = 1024ll*1024ll*10ll;
  char *buff = new char[buffSize];//10MB buffer
  size_t offset = 0;
  long read = 0;
  //FileNode *node = _putEvent->node;
  do {
    //CheckEvent validity
    if(!UploadQueue::getInstance().checkEventValidity(*_putEvent)){
      glfs_close(fd);
      delete []buff;
      buff = nullptr;
      node->close(inodeNum);
      return true;
    }

    if(node->mustBeDeleted()){//Check Delete
      glfs_close(fd);
      delete []buff;
      buff = nullptr;
      node->close(inodeNum);
      return true;
    }

    //get lock delete so file won't be deleted
    read = node->read(buff,offset,buffSize);


    offset += read;
    int64_t written = glfs_write(fd,buff,read,0);
    if(written!=read){
      LOG(ERROR)<<"Error while writing to:"<<_putEvent->fullPathBuffer<<
          ". Asked to write:"<<read<<" but wrote:"<<written;
      glfs_close(fd);
      return false;
    }
  }
  while(read > 0);

  delete []buff;
  buff = nullptr;

  if(node->mustBeDeleted()){//Check Delete
    glfs_close(fd);
    node->close(inodeNum);
    return true;
  }

  //Now sync
  if(glfs_close(fd)!=0){
    LOG(ERROR)<<"Error in closing:"<<_putEvent->fullPathBuffer<<" errorNo:"<<errno;
    node->close(inodeNum);
    return false;
  }

  node->close(inodeNum);
  return true;
}

bool GlusterBackend::put_metadata(const SyncEvent* _putMetaEvent) {
  return false;
}

bool GlusterBackend::move(const SyncEvent* _moveEvent) {
  return false;
}

bool GlusterBackend::remove(const SyncEvent* _removeEvent) {
  //First assume it's a file
  if(glfs_unlink((glfs_t*)fs,_removeEvent->fullPathBuffer.c_str())!=0){
    if(errno == EISDIR){//Was a directory
      if(glfs_rmdir((glfs_t*)fs,_removeEvent->fullPathBuffer.c_str())!=0){
        LOG(ERROR)<<"Error in deleting directory:"<<_removeEvent->fullPathBuffer<<". errorNo:"<<errno;
        return false;
      }
    }else {
      LOG(ERROR)<<"Error in deleting file:"<<_removeEvent->fullPathBuffer<<". errorNo:"<<errno;
      return false;
    }
  }
  //LOG(INFO)<<"Deleted "<<_removeEvent->fullPathBuffer<<" from GLUSTERFS SUCCESS";
  return true;
}

} /* namespace FUSESwift */
