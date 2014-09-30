/*
 * SettingManager.cpp
 *
 *  Created on: Sep 29, 2014
 *      Author: catrina
 */

#include "SettingManager.h"

namespace FUSESwift {

const std::string SettingManager::KEY_MODE = "MODE";
Dictionary SettingManager::config;

SettingManager::SettingManager() {
	// TODO Auto-generated constructor stub

}

SettingManager::~SettingManager() {
	// TODO Auto-generated destructor stub
}

std::string SettingManager::get(std::string key) {
	auto itr = config.find(key);
	if(itr == config.end())
		return "";
	else
		return itr->second;
}

void SettingManager::set(std::string key, std::string value) {
	config.insert(Dictionary::value_type(key,value));
}

} /* namespace FUSESwift */
