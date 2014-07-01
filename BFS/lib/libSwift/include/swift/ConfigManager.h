/*
 * ConfigManager.h
 *
 *  Created on: 2014-05-26
 *      Author: Behrooz Shafiee Sarjaz
 */

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

