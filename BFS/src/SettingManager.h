/*
 * SettingManager.h
 *
 *  Created on: Sep 29, 2014
 *      Author: catrina
 */

#ifndef SETTINGMANAGER_H_
#define SETTINGMANAGER_H_
#include "Global.h"
#include <unordered_map>
#include <string>

namespace FUSESwift {

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

class SettingManager {
typedef std::unordered_map<std::string,std::string> Dictionary;
	static Dictionary config;
	SettingManager();
public:
	virtual ~SettingManager();
	static std::string get(std::string key);
	static int getInt(std::string key);
	static long getLong(std::string key);
	static uint64_t getUINT64(std::string key);
	static double getDouble(std::string key);
	static void set(std::string key,std::string value);
	static void load(std::string path);
};

} /* namespace FUSESwift */

#endif /* SETTINGMANAGER_H_ */
