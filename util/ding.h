/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef util_ding
#define util_ding

#include <string.h>
#include "types.h"


char* mcxStrDup
(  const char* str
)  ;


/*
 * if len < 0, strlen(p) is used.
*/

int mcxStrCountChar
(  const char*    p
,  char           c
,  int            len
)  ;

char* mcxStrChrIs
(  const char*    src
,  int (*fbool)(int c)
,  int      len
)  ;

char* mcxStrChrAint
(  const char*    src
,  int (*fbool)(int c)
,  int  len
)  ;

char* mcxStrRChrIs
(  const char*    src
,  int (*fbool)(int c)
,  int      offset
)  ;

char* mcxStrRChrAint
(  const char*    src
,  int (*fbool)(int c)
,  int      offset
)  ;

#define TR_SQUASH 1
#define TR_DELETE 2
#define TR_COMPLEMENT 4

mcxbool trLoadTable
(  const char*    src
,  const char*    dst
,  int*     tbl         /* should have size 256 */
,  int      flags
)  ;

int mcxStrTranslate
(  char*    src
,  int*     tbl         /* should have size 256 */
,  int      flags
)  ;

#endif

