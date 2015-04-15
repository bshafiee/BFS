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

#ifndef BACKENDMANAGER_H_
#define BACKENDMANAGER_H_
#include "Global.h"
#include <vector>
#include "Backend.h"

namespace BFS {

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

} /* namespace BFS */
#endif /* BACKENDMANAGER_H_ */
