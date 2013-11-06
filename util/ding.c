/* (c) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ding.h"
#include "types.h"
#include "alloc.h"


const char* trGetToken
(  const char *p
,  const char *z
,  int  *tokval
)  ;

char* mcxStrDup
(  const char* str
)
   {  int len = strlen(str)
   ;  char* rts = mcxAlloc(len+1, RETURN_ON_FAIL)
   ;  if (rts)
      memcpy(rts, str, len+1)
   ;  return rts
;  }


int mcxStrCountChar
(  const char*    p
,  char           c
,  int            len
)
   {  const char* z = p
   ;  int ct =  0

   ;  z += (len >= 0) ? len : strlen(p)

   ;  while (p<z)
      if (*p++ == c)
      ct++
   ;  return ct
;  }


char* mcxStrChrIs
(  const char*    p
,  int    (*fbool)(int c)
,  int      len
)
   {  if (len)
      while (*p && !fbool((int) *p) && --len && ++p)
      ;
      return (char*) ((len && *p) ?  p : NULL)
;  }


char* mcxStrChrAint
(  const char*    p
,  int    (*fbool)(int c)
,  int      len
)
   {  if (len)
      while (*p && fbool((int) *p) && --len && ++p)
      ;
      return (char *) ((len && *p) ?  p : NULL)
;  }


char* mcxStrRChrIs
(  const char*    p
,  int    (*fbool)(int c)
,  int      offset
)
   {  const char* z =  offset >= 0 ? p+offset : p + strlen(p)
   ;  while (--z >= p && !fbool((unsigned char) *z))
      ;
      return (char*) (z >= p ? z : NULL)
;  }


char* mcxStrRChrAint
(  const char*    p
,  int    (*fbool)(int c)
,  int      offset
)
   {  const char* z  =  offset >= 0 ? p+offset : p + strlen(p)
   ;  while (--z >= p && fbool((unsigned char) *z))
      ;
      return (char*) (z >= p ? z : NULL)
;  }


