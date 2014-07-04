/*
 * filesystem.h
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "tree.h"
#include "filenode.h"

namespace FUSESwift {

class FileSystem: public Tree {
  const size_t blockSize = 1;
  const std::string delimiter = "/";
  FileNode *root;
  //Singleton instance
  static FileSystem *mInstance;
  FileSystem(FileNode* _root);
  FileNode* searchNode(FileNode* _parent, std::string _name, bool _isDir);
public:
  void initialize(FileNode* _root);
  static FileSystem* getInstance();
  virtual ~FileSystem();
  FileNode* mkFile(FileNode* _parent, std::string _name);
  FileNode* mkDirectory(FileNode* _parent, std::string _name);
  FileNode* mkFile(std::string _path);
  FileNode* mkDirectory(std::string _path);
  size_t rmNode(FileNode* &_node);
  FileNode* searchFile(FileNode* _parent, std::string _name);
  FileNode* searchDir(FileNode* _parent, std::string _name);
  size_t getBlockSize();
  std::string getDelimiter();
  FileNode* getNode(std::string _path);
  FileNode* findParent(const std::string &_path);
  void destroy();
  /**
   * tries to rename it it is permitted.
   * @return
   *  true if successful
   *  false if fails
   */
  bool tryRename(const std::string &_from,const std::string &_to);
  std::string printFileSystem();
};

} /* namespace FUSESwift */
#endif /* FILESYSTEM_H_ */
