/**********************************************************************
Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
This program comes with ABSOLUTELY NO WARRANTY;

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#ifndef BACKEND_H_
#define BACKEND_H_

#include "Global.h"
#include <cstring>
#include <vector>
#include "SyncEvent.h"

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
  unsigned long length;
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

	static bool equality (const BackendItem& lhs,const BackendItem& rhs){
		return (lhs.name == rhs.name && lhs.length == rhs.length &&
						lhs.hash == rhs.hash && lhs.last_modified == rhs.last_modified);
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
  /**
   * @return
   * 1) A pointer to an stream which you can read data in chunk from
   * 2) A pointer to a container which holds the input stream connection
   *    and should be freed once the read is done, through releaseGetData()
   */
  virtual std::pair<std::istream*,intptr_t> get(const SyncEvent *_getEvent) = 0;
  virtual std::vector<std::pair<std::string,std::string> >* get_metadata(const SyncEvent *_getMetaEvent) = 0;
  virtual bool put(const SyncEvent *_putEvent) = 0;
  virtual bool put_metadata(const SyncEvent *_putMetaEvent) = 0;
  virtual bool move(const SyncEvent *_moveEvent) = 0;
  virtual bool remove(const SyncEvent *_removeEvent) = 0;
  virtual void releaseGetData(intptr_t &_ptr) = 0;
  BackendType getType();
  static std::string backendTypeToStr(BackendType _type);
};

} /* namespace FUSESwift */
#endif /* BACKEND_H_ */
