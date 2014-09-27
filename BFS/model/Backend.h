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

struct BackendItem {
  BackendItem (std::string _name, long _length,
               std::string _hash, std::string _last_modified) {
    name = _name;
    length = _length;
    hash = _hash;
    last_modified = _last_modified;
  }
  std::string name;
  long length;
  std::string hash;
  std::string last_modified;

  static bool CompBySizeAsc (const BackendItem& lhs, const BackendItem& rhs) {
		return lhs.length < rhs.length;
	}

	static bool CompBySizeDes (const BackendItem& lhs, const BackendItem& rhs) {
		return lhs.length > rhs.length;
	}

	static bool CompByNameAsc (const BackendItem& lhs, const BackendItem& rhs) {
		return lhs.name < rhs.name;
	}

	bool operator == (const BackendItem& a) const {
		return (a.name == this->name && a.length == this->length &&
						a.hash == this->hash && a.last_modified == this->last_modified);
	}
};

class Backend {
  BackendType type;
public:
  Backend(BackendType _type);
  virtual ~Backend();
  /**
   * Virtual list of methods that each
   * Backend implementation should provide
   * **/
  virtual std::vector<BackendItem>* list() = 0;
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
