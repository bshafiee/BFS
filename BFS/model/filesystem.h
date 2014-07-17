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
  FileNode* traversePathToParent(std::string _path);
  pthread_mutex_t mutex; /* read and write operations */
public:
  //Constants
  static const size_t blockSize = 4096;
  const std::string delimiter = "/";
  //Functions
  void initialize(FileNode* _root);
  static FileSystem* getInstance();
  virtual ~FileSystem();
  FileNode* mkFile(FileNode* _parent, std::string _name);
  FileNode* mkDirectory(FileNode* _parent, std::string _name);
  FileNode* mkFile(std::string _path);
  FileNode* mkDirectory(std::string _path);
  size_t rmNode(FileNode* &_parent,FileNode* &_node);
  FileNode* searchFile(FileNode* _parent, std::string _name);
  FileNode* searchDir(FileNode* _parent, std::string _name);
  std::string getDelimiter();
  FileNode* getNode(std::string _path);
  FileNode* findParent(const std::string &_path);
  void destroy();
  void lock();
  void unlock();

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
