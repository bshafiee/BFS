/*
 * SettingManager.cpp
 *
 *  Created on: Sep 29, 2014
 *      Author: catrina
 */

#include "SettingManager.h"
#include <fstream>
#include <algorithm> // remove and remove_if
#include <ctype.h>
#include "LoggerInclude.h"

using namespace std;

namespace FUSESwift {

//const std::string SettingManager::KEY_MODE = "MODE";
SettingManager::Dictionary SettingManager::config;

SettingManager::SettingManager() {}

SettingManager::~SettingManager() {}

std::string SettingManager::get(std::string key) {
	auto itr = config.find(key);
	if(itr == config.end())
		return "";
	else
		return itr->second;
}

void SettingManager::set(std::string key, std::string value) {
	config.insert(Dictionary::value_type(key,value));
	//printf("%s=%s\n",key.c_str(),value.c_str());
}

int SettingManager::getInt(std::string key) {
	string str = get(key);
	if(!str.length())
		return 0;
	return std::stoi(str);
}

bool SettingManager::getBool(std::string key) {
  string str = get(key);
  if(!str.length())
    return false;
  return str == "true";
}

long SettingManager::getLong(std::string key) {
	string str = get(key);
	if(!str.length())
		return 0;
	return std::stol(str);
}

uint64_t SettingManager::getUINT64(std::string key) {
	string str = get(key);
	if(!str.length())
		return 0;
	return std::stoull(str);
}

double SettingManager::getDouble(std::string key) {
	string str = get(key);
	if(!str.length())
		return 0;
	return std::stod(str);
}

void SettingManager::load(std::string path) {
	ifstream infile(path);
	string line;
	while (getline(infile, line)) {
		line.erase( std::remove_if( line.begin(), line.end(),  ::isspace), line.end() );
		if(line.c_str()[0] == '#')//comment
			continue;
		size_t indexOfEqual = -1;
		indexOfEqual = line.find('=');
		if(indexOfEqual == std::string::npos) {
			LOG(ERROR)<<"Invalid config input:"<<line;
			continue;
		}

		string key = line.substr(0,indexOfEqual);
		string value = line.substr(indexOfEqual+1);

		set(key,value);
	}
}

} /* namespace FUSESwift */
