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

#ifndef _PARAMS_H_
#define _PARAMS_H_
#include "Global.h"
#include <limits.h>
#include <stdio.h>
#include <fuse.h>
#include <mutex>

//DEBUG LEVELS
#define DDEBUG 0
#define DEBUG_INIT       0
#define DEBUG_DESTROY    0
#define DEBUG_GET_ATTRIB 0
#define DEBUG_READLINK   0
#define DEBUG_MKNOD      0
#define DEBUG_MKDIR      0
#define DEBUG_RMDIR      0
#define DEBUG_RENAME     1
#define DEBUG_OPEN       0
#define DEBUG_TRUNCATE   0
#define DEBUG_READ       0
#define DEBUG_WRITE      0
#define DEBUG_FLUSH      0
#define DEBUG_RELEASE    0
#define DEBUG_OPENDIR    0
#define DEBUG_READDIR    0
#define DEBUG_RELEASEDIR 0
#define DEBUG_ACCESS     0
#define DEBUG_UNLINK     0


#endif// _PARAMS_H_
