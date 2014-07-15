/*
 * SwiftBackend.cpp
 *
 *  Created on: 2014-07-15
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "SwiftBackend.h"

using namespace std;
using namespace Swift;

namespace FUSESwift {

SwiftBackend::SwiftBackend(BackendType _type):Backend(_type) {
  // TODO Auto-generated constructor stub

}

SwiftBackend::~SwiftBackend() {
  // TODO Auto-generated destructor stub
}

bool SwiftBackend::initialize(Swift::AuthenticationInfo* _authInfo) {
}

bool SwiftBackend::put(SyncEvent* _putEvent) {
}

bool SwiftBackend::put_metadata(SyncEvent* _removeEvent) {
}

bool SwiftBackend::move(SyncEvent* _moveEvent) {
}



bool SwiftBackend::remove(SyncEvent* _moveEvent) {
}

} /* namespace FUSESwift */
