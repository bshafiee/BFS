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

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_
#include "Global.h"
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <list>


namespace BFS {


class FileNode;
struct FileEntryNode;

class FileSystem {
  FileNode *root;
  //Singleton instance
  static FileSystem *mInstance;
  FileSystem(FileNode* _root);
  FileNode* traversePathToParent(const std::string &_path);
  //inode handling
  std::atomic<uint64_t> inodeCounter;
  std::unordered_map<uint64_t,std::pair<intptr_t,bool>> inodeMap;//inode to file map
  std::unordered_map<intptr_t,std::list<uint64_t> > nodeInodeMap;//node to inodes map
  std::recursive_mutex inodeMapMutex;
  //Delete queue
  std::recursive_mutex deleteQueueMutex;
  std::list<FileNode*> deleteQueue;
  /*
   * To keep track of remote files, this list is used in ZooHandler
   * updateRemoteFilesInFS for checking what remote files in our fs should
   * be removed because they don't exist anymore on their servers
   */
  //std::vector<FileNode*> remoteFiles;

  FileNode* mkFile(FileNode* _parent, const std::string &_name,bool _isRemote,bool _open);
  FileNode* mkDirectory(FileNode* _parent, const std::string &_name,bool _isRemote);
public:
  //Constants
  static const int64_t blockSize = 1024ll;
  static const std::string delimiter;
  //Functions
  void initialize(FileNode* _root);
  static FileSystem& getInstance();
  virtual ~FileSystem();
  FileNode* mkFile(const std::string &_path,bool _isRemote,bool _open);
  /**
   * Adds an FileNode to fileSystem! Note that it's caller responsibility to
   * set all info for this file(fullpath,name,remote...)
   */
  bool addFile(FileNode* _fNode);

  FileNode* mkDirectory(const std::string &_path,bool _isRemote);
  FileNode* findAndOpenNode(const std::string &_path);
  FileNode* findParent(const std::string &_path);
  std::string getFileNameFromPath(const std::string &_path);
  void destroy();
  /**
   * tries to rename it it is permitted.
   * @return
   *  true if successful
   *  false if fails
   */
  bool tryRename(const std::string &_from,const std::string &_to);
  /**
   * tries to create the specified path if not exist
   * @return:
   * the last directory node in the hierarchy on success
   * nullptr on failure
   */
  FileNode* createHierarchy(const std::string &_path,bool _isRemote);
  //std::string printFileSystem();
  /**
   * list of pairs <filenam,isDir?>
   */
  //void listFileSystem(std::unordered_map<std::string,FileEntryNode> &output,bool _includeRemotes,bool _includeFolders);
  /**
   * checks if the input name is valid
   */
  bool nameValidator(const std::string &_name);

  /** Remote Operations **/
  bool createRemoteFile(const std::string &_name);
  /**
   * moves _localFile to a remote file with tons of free space
   */
  bool moveToRemoteNode(FileNode* _localFile);

  intptr_t getNodeByINodeNum(uint64_t _inodeNum);
  uint64_t assignINodeNum(const intptr_t _nodePtr);
  void replaceAllInodesByNewNode(intptr_t _oldPtr,intptr_t _newPtr);
  void removeINodeEntry(uint64_t _inodeNum);

  //Signal Delete Node
  bool signalDeleteNode(FileNode* _node,bool _informRemoteOwner);
};

} /* namespace BFS */
#endif /* FILESYSTEM_H_ */
