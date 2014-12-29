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
