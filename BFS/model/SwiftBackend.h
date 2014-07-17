/*
 * SwiftBackend.h
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef SWIFTBACKEND_H_
#define SWIFTBACKEND_H_

#include "Backend.h"
#include <Authentication.h>
#include <Account.h>

namespace FUSESwift {

class SwiftBackend: public Backend {
  const std::string DEFAULT_CONTAINER_NAME = "KOS";
  Swift::Account* account;
  Swift::Container* defaultContainer;
  bool initDefaultContainer();
  std::string convertToSwiftName(const std::string &fullPath);
public:
  SwiftBackend();
  virtual ~SwiftBackend();

  //Implement backend interface
  bool initialize(Swift::AuthenticationInfo* _authInfo);
  bool put(SyncEvent *_putEvent);
  bool put_metadata(SyncEvent *_removeEvent);
  bool move(SyncEvent *_moveEvent);
  bool remove(SyncEvent *_moveEvent);

};

} /* namespace FUSESwift */
#endif /* SWIFTBACKEND_H_ */
