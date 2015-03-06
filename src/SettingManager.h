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

#ifndef SETTINGMANAGER_H_
#define SETTINGMANAGER_H_
#include "Global.h"
#include <unordered_map>
#include <string>

namespace FUSESwift {

enum class RUNTIME_MODE {DISTRIBUTED,STANDALONE_SWIFT,STANDALONE};

static const std::string CONFIG_KEY_MODE = "mode";
static const std::string CONFIG_KEY_SWIFT_USERNAME = "username";
static const std::string CONFIG_KEY_SWIFT_PASSWORD = "password";
static const std::string CONFIG_KEY_SWIFT_URL = "auth_url";
static const std::string CONFIG_KEY_SWIFT_TENANT = "tenant_name";
static const std::string CONFIG_KEY_MAX_MEM_COEF = "max_mem_coef";
static const std::string CONFIG_KEY_ZERO_NETWORK_DEV = "network_dev";
static const std::string CONFIG_KEY_TCP_PORT = "tcp_port";
static const std::string CONFIG_KEY_ZOO_SERVER_URL = "zoo_server_url";
static const std::string CONFIG_KEY_ZOO_ELECTION_ZNODE = "zoo_election_znode";
static const std::string CONFIG_KEY_ZOO_ASSIGNMENT_ZNODE = "zoo_assignment_znode";
static const std::string CONFIG_KEY_DEBUG_SWIFT_CPP_SDK = "debug_swift_cpp_sdk";

class SettingManager {
typedef std::unordered_map<std::string,std::string> Dictionary;
	static Dictionary config;
	static RUNTIME_MODE runtimeMod;
	static int port;
	SettingManager();
public:
	virtual ~SettingManager();
	static std::string get(std::string key);
	static int getInt(std::string key);
	static bool getBool(std::string key);
	static long getLong(std::string key);
	static uint64_t getUINT64(std::string key);
	static double getDouble(std::string key);
	static void set(std::string key,std::string value);
	static void load(std::string path);
	static RUNTIME_MODE runtimeMode();
	static int getPort();
};

} /* namespace FUSESwift */

#endif /* SETTINGMANAGER_H_ */
