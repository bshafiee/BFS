/**************************************************************************
    This is a general SDK for OpenStack Swift API written in C++
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
**************************************************************************/

#ifndef CONFIGMANAGER_H_
#define CONFIGMANAGER_H_

#include <Poco/HashMap.h>
#include <cstring>
#include <ErrorNo.h>
#include <ConfigKey.h>

namespace Swift {

using namespace std;
using namespace Poco;

class ConfigManager {
private:

public:
  //Hashmap
  typedef HashMap<ConfigKey, string> ConfigMap;
  //Methods
  static const string* getProperty(ConfigKey key);
  static int putProperty(ConfigKey key,const string &value);
  static int removeProperty(ConfigKey key);
  static ConfigMap::Iterator beginIterator();
};

} /* namespace Swift */
#endif /* CONFIGMANAGER_H_ */

