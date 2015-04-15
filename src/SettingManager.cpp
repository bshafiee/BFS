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

#include "SettingManager.h"
#include <fstream>
#include <algorithm> // remove and remove_if
#include <ctype.h>
#include "LoggerInclude.h"

using namespace std;

namespace BFS {

//const std::string SettingManager::KEY_MODE = "MODE";
SettingManager::Dictionary SettingManager::config;
RUNTIME_MODE SettingManager::runtimeMod = RUNTIME_MODE::DISTRIBUTED;
BackendType SettingManager::backendType = BackendType::NONE;
int SettingManager::port = -1;

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
	string runtimeModeStr = get(CONFIG_KEY_MODE);
	if(runtimeModeStr == "DISTRIBUTED")
		runtimeMod = RUNTIME_MODE::DISTRIBUTED;
	else if(runtimeModeStr == "STANDALONE")
		runtimeMod = RUNTIME_MODE::STANDALONE;
	else{
		LOG(FATAL)<<"Can't determine runtime mode, using default mode: Distributed";
	}
	LOG(INFO)<<"Runtime Mode:"<<runtimeModeStr;


	//Backend
	string backendStr = get(CONFIG_KEY_BACKEND);
  if(backendStr == "GLUSTER")
    backendType = BackendType::GLUSTER;
  else if(backendStr == "SWIFT")
    backendType = BackendType::SWIFT;
  else if(backendStr == "NONE")//No backend
    backendType = BackendType::NONE;
  else{
    LOG(ERROR)<<"Can't determine backend type, using default mode: NONE";
    backendType = BackendType::NONE;
  }
  LOG(INFO)<<"Backend:"<<backendStr;

	port = SettingManager::getInt(CONFIG_KEY_TCP_PORT);
}

RUNTIME_MODE SettingManager::runtimeMode() {
  return runtimeMod;
}

BackendType SettingManager::getBackendType() {
  return backendType;
}

int SettingManager::getPort() {
  return port;
}

} /* namespace BFS */


