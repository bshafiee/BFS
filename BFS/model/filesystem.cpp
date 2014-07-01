/*
 * filesystem.cpp
 *
 *  Created on: 2014-06-30
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "filesystem.h"

using namespace FUSESwift;

FileSystem::FileSystem(FileNode* _root):Tree(_root),root(_root) {

}

FileSystem::~FileSystem() {
  //recursively delete
}

void FileSystem::destroy() {
  if(root == nullptr)
    return;

}
