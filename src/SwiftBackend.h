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

#ifndef SWIFTBACKEND_H_
#define SWIFTBACKEND_H_
#include "Global.h"
#include "Backend.h"
#include <Swift/Authentication.h>
#include <Swift/Account.h>
#include <tuple>

namespace BFS {

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
  bool list(std::vector<BackendItem>& _list);
  bool get(const SyncEvent *_getEvent);
  std::vector<std::pair<std::string,std::string> >* get_metadata(const SyncEvent *_getMetaEvent);
  bool put(const SyncEvent *_putEvent);
  bool put_metadata(const SyncEvent *_putMetaEvent);
  bool move(const SyncEvent *_moveEvent);
  bool remove(const SyncEvent *_removeEvent);
};

} /* namespace BFS */
#endif /* SWIFTBACKEND_H_ */
