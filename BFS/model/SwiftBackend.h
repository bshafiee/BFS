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

namespace FUSESwift {

class SwiftBackend: public Backend {
public:
  SwiftBackend(BackendType _type);
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
