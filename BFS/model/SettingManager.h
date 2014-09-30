/*
 * SettingManager.h
 *
 *  Created on: Sep 29, 2014
 *      Author: catrina
 */

#ifndef SETTINGMANAGER_H_
#define SETTINGMANAGER_H_
#include <unordered_map>
#include <string>

namespace FUSESwift {

typedef std::unordered_map<std::string,std::string> Dictionary;

class SettingManager {
	static Dictionary config;
	SettingManager();
public:
	virtual ~SettingManager();
	static const std::string KEY_MODE;
	static std::string get(std::string key);
	static void set(std::string key,std::string value);
};

} /* namespace FUSESwift */

#endif /* SETTINGMANAGER_H_ */
