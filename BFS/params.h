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
    char *rootdir;
};
#define BB_DATA ((struct bb_state *) fuse_get_context()->private_data)

#endif
