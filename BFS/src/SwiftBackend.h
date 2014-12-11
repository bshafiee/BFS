/*
 * SwiftBackend.h
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef SWIFTBACKEND_H_
#define SWIFTBACKEND_H_
#include "Global.h"
#include "Backend.h"
#include <Swift/Authentication.h>
#include <Swift/Account.h>
#include <tuple>

namespace FUSESwift {

class SwiftBackend: public Backend {
  const std::string DEFAULT_CONTAINER_NAME = "KOS";
  Swift::Account* account;
  Swift::Container* defaultContainer;
  bool initDefaultContainer();
  std::string convertToSwiftName(const std::string &fullPath);
  std::string convertFromSwiftName(const std::string &swiftPath);
public:
  SwiftBackend();
  virtual ~SwiftBackend();

  bool initialize(Swift::AuthenticationInfo* _authInfo);
  //Implement backend interface
  std::vector<BackendItem>* list();
  std::istream* get(const SyncEvent *_getEvent);
  std::vector<std::pair<std::string,std::string> >* get_metadata(const SyncEvent *_getMetaEvent);
  bool put(const SyncEvent *_putEvent);
  bool put_metadata(const SyncEvent *_putMetaEvent);
  bool move(const SyncEvent *_moveEvent);
  bool remove(const SyncEvent *_removeEvent);

};

} /* namespace FUSESwift */
#endif /* SWIFTBACKEND_H_ */
