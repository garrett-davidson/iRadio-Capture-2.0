/*
    assniffer - platform.h (some win32/linux compatibility defines/includes)
    Copyright (C) 2004 Cockos Incorporated

    assniffer is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    assniffer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with assniffer; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _ASSNIFFER_PLATFORM_H_
#define _ASSNIFFER_PLATFORM_H_

#ifndef _WIN32

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

static char *lstrcpyn(char *out, const char *in, int l) // this is like strcpy(), except it null terminates (within the alloted space), yay
{
  char *op=out;
  while (*in && l-- > 1) *op++=*in++;
  *op=0;
	return out;
}

#define strnicmp strncasecmp
#define stricmp strcasecmp

#ifndef min
#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))
#endif

#define IS_DIR_CHAR(x) ((x)=='/')

#ifndef CharNext
#define CharNext(x) ((char *)(x)+1)
#endif

#else // _WIN32

#define IS_DIR_CHAR(x) ((x)=='\\'||(x)=='/')

#endif

#endif//_ASSNIFFER_PLATFORM_H_
