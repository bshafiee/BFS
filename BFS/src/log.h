/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _LOG_H_
#define _LOG_H_
#include "Global.h"
#include <stdio.h>
#include "params.h"


//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(void);
void  log_close(void);
void log_conn (struct fuse_conn_info *conn);
void log_fi (struct fuse_file_info *fi);
void log_stat(struct stat *si);
void log_statvfs(struct statvfs *sv);
void log_utime(struct utimbuf *buf);
void log_fuse_context(struct fuse_context *context);

void log_msg(const char *format, ...);
#endif
