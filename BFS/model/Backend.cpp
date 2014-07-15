/*
 * Backend.cpp
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "Backend.h"

namespace FUSESwift {

Backend::Backend(BackendType _type):type(_type) {}

Backend::~Backend() {
  // TODO Auto-generated destructor stub
}

BackendType Backend::getType() {
  return type;
}

std::string Backend::backendTypeToStr(BackendType _type) {
  switch(_type) {
    case BackendType::AMAZON_S3:
      return "AMAZON_S3";
    case BackendType::HARDDISK:
      return "HARDDISK";
    case BackendType::NULLDISK:
      return "NULLDISK";
    case BackendType::SSH:
      return "SSH";
    case BackendType::SWIFT:
      return "SWIFT";
    default:
      return "";
  }
}

} /* namespace FUSESwift */
