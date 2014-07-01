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
  FileNode *root;
public:
  FileSystem(FileNode* _root);
  virtual ~FileSystem();
  void destroy();
};

} /* namespace FUSESwift */
#endif /* FILESYSTEM_H_ */
