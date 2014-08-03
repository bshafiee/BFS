/*
 * filesystem.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "tree.h"
//#include "filenode.h"

namespace FUSESwift {


class FileNode;

class FileSystem: public Tree {
  FileNode *root;
  //Singleton instance
  static FileSystem *mInstance;
  FileSystem(FileNode* _root);
  FileNode* searchNode(FileNode* _parent, std::string _name, bool _isDir);
  FileNode* traversePathToParent(const std::string &_path);
public:
  //Constants
  static const size_t blockSize = 4096;
  static std::string delimiter;
  //Functions
  void initialize(FileNode* _root);
  static FileSystem& getInstance();
  virtual ~FileSystem();
  FileNode* mkFile(FileNode* _parent, const std::string &_name);
  FileNode* mkDirectory(FileNode* _parent, const std::string &_name);
  FileNode* mkFile(const std::string &_path);
  FileNode* mkDirectory(const std::string &_path);
  size_t rmNode(FileNode* &_parent,FileNode* &_node);
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
};

} /* namespace FUSESwift */
#endif /* FILESYSTEM_H_ */
