/*
 * BackendManager.h
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef BACKENDMANAGER_H_
#define BACKENDMANAGER_H_
#include "Global.h"
#include <vector>
#include "Backend.h"

namespace FUSESwift {

class BackendManager {
  static std::vector<Backend*> list;
  static Backend* currentBackend;
  BackendManager();
public:
  virtual ~BackendManager();
  static void registerBackend(Backend* _backend);
  static bool selectBackend(Backend* _backend);
  static bool selectBackend(BackendType _type);
  static Backend* getActiveBackend();
};

} /* namespace FUSESwift */
#endif /* BACKENDMANAGER_H_ */
