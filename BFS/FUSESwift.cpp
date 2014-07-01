/*
 * FUSESwift.cpp
 *
 *  Created on: 2014-06-25
 *      Author: Behrooz Shafiee Sarjaz
 */

#include "FUSESwift.h"
#include "log.h"

namespace FUSESwift {

int swift_getattr(const char* path, struct stat* stbuff) {
  //stbuff->
  log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuff);
  log_stat (stbuff);
  return 0;
}

void* swift_init(struct fuse_conn_info* conn) {
  log_msg("\nbb_init()\n");

  log_conn(conn);
  log_fuse_context(fuse_get_context());

  return BB_DATA;
}

} /* namespace Swift */
