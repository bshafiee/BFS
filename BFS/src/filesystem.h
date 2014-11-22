/*
 * filesystem.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "tree.h"
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>


namespace FUSESwift {


class FileNode;

class FileSystem: public Tree {
  FileNode *root;
  //Singleton instance
  static FileSystem *mInstance;
  FileSystem(FileNode* _root);
  FileNode* searchNode(FileNode* _parent, std::string _name, bool _isDir);
  FileNode* traversePathToParent(const std::string &_path);
  //inode handling
  std::atomic<uint64_t> inodeCounter;
  std::unordered_map<uint64_t,intptr_t> inodeMap;
  std::mutex inodeMapMutex;
public:
  //Constants
  static const size_t blockSize = 1024*512;
  static std::string delimiter;
  //Functions
  void initialize(FileNode* _root);
  static FileSystem& getInstance();
  virtual ~FileSystem();
  FileNode* mkFile(FileNode* _parent, const std::string &_name,bool _isRemote);
  FileNode* mkDirectory(FileNode* _parent, const std::string &_name,bool _isRemote);
  FileNode* mkFile(const std::string &_path,bool _isRemote);
  FileNode* mkDirectory(const std::string &_path,bool _isRemote);
  FileNode* searchFile(FileNode* _parent, const std::string &_name);
  FileNode* searchDir(FileNode* _parent, const std::string &_name);
  FileNode* getNode(const std::string &_path);
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
  FileNode* createHierarchy(const std::string &_path);
  std::string printFileSystem();
  /**
   * Caller's responisbility to call delete on returned list
   */
  std::vector<std::string> listFileSystem(bool _includeRemotes);
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
  uint64_t assignINodeNum(intptr_t _nodePtr);
  void replaceNodeByINodeNum(uint64_t _inodeNum, intptr_t _nodePtr);
  void removeINodeEntry(uint64_t _inodeNum);
};

} /* namespace FUSESwift */
#endif /* FILESYSTEM_H_ */
