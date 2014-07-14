/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  There are a couple of symbols that need to be #defined before
  #including all the headers.
*/

#ifndef _PARAMS_H_
#define _PARAMS_H_

// maintain bbfs state in here
#include <limits.h>
#include <stdio.h>
#include <fuse.h>

struct bb_state {
    FILE *logfile;
};

#define BB_DATA ((struct bb_state *) fuse_get_context()->private_data)

//DEBUG LEVELS
#define DEBUG 0
#define DEBUG_INIT       0
#define DEBUG_DESTROY    0
#define DEBUG_GET_ATTRIB 0
#define DEBUG_MKNOD      0
#define DEBUG_MKDIR      0
#define DEBUG_RMDIR      0
#define DEBUG_RENAME     0
#define DEBUG_OPEN       1
#define DEBUG_READ       0
#define DEBUG_WRITE      0
#define DEBUG_FLUSH      0
#define DEBUG_RELEASE    1
#define DEBUG_OPENDIR    0
#define DEBUG_READDIR    0
#define DEBUG_RELEASEDIR 0
#define DEBUG_ACCESS     0


#endif// _PARAMS_H_
