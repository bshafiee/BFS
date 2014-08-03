/*
 * Backend.h
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef BACKEND_H_
#define BACKEND_H_

#include <cstring>
#include "syncEvent.h"
#include <vector>

namespace FUSESwift {

enum class BackendType {SWIFT, AMAZON_S3, HARDDISK, NULLDISK, SSH};

class Backend {
  BackendType type;
public:
  Backend(BackendType _type);
  virtual ~Backend();
  /**
   * Virtual list of methods that each
   * Backend implementation should provide
   * **/
  virtual std::vector<std::string>* list() = 0;
  virtual std::istream* get(const SyncEvent *_getEvent) = 0;
  virtual std::vector<std::pair<std::string,std::string> >* get_metadata(const SyncEvent *_getMetaEvent) = 0;
  virtual bool put(const SyncEvent *_putEvent) = 0;
  virtual bool put_metadata(const SyncEvent *_putMetaEvent) = 0;
  virtual bool move(const SyncEvent *_moveEvent) = 0;
  virtual bool remove(const SyncEvent *_removeEvent) = 0;
  BackendType getType();
  static std::string backendTypeToStr(BackendType _type);
};

} /* namespace FUSESwift */
#endif /* BACKEND_H_ */
