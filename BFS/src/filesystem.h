/*
 * filesystem.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_
#include "Global.h"
#include "tree.h"
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <list>


namespace FUSESwift {


class FileNode;

class FileSystem: public Tree {
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

  FileNode* mkFile(FileNode* _parent, const std::string &_name,bool _isRemote,bool _open);
  FileNode* mkDirectory(FileNode* _parent, const std::string &_name,bool _isRemote);
public:
  //Constants
  static const size_t blockSize = 1024*512;
  static const std::string delimiter;
  //Functions
  void initialize(FileNode* _root);
  static FileSystem& getInstance();
  virtual ~FileSystem();
  FileNode* mkFile(const std::string &_path,bool _isRemote,bool _open);
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
  std::string printFileSystem();
  /**
   * list of pairs <filenam,isDir?>
   */
  std::vector<std::pair<std::string,bool> > listFileSystem(bool _includeRemotes,bool _includeFolders);
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

} /* namespace FUSESwift */
#endif /* FILESYSTEM_H_ */
