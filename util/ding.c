/*    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ding.h"
#include "types.h"
#include "alloc.h"


const char* trGetRange
(  const char *p
,  const char *z
,  int  *offset
,  int  *len
)  ;

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


int mcxStrTranslate
(  char*    src
,  int*     tbl
,  int      flags
)
   {  int i,j
   ;  int prev =  -1          /* only relevant for squash case */
   ;  int len = strlen(src)
   ;  mcxbool squash = flags & TR_SQUASH
   ;

      for (i=0,j=0;i<len;i++)
      {  unsigned char idx = *(src+i)

         /* tbl[idx] < 0 is deletion, implies *not* squash */

      ;  if (tbl[idx] >= 0)
         {  mcxbool translate = (tbl[idx] > 0)
         ;  int val =  translate ? tbl[idx] : idx

         ;  /* val != prev: no squash */
            /* !squash: no squash */
            /* !tbl[idx]: character is not mapped, always print */

            if (val != prev || !squash || !translate)
            {  *(src+j) =  (char) val
            ;  j++
         ;  }
            if (squash)
            prev = translate ? val : -1
      ;  }
      }
      *(src+j) = '\0'
   ;  return j
;  }


mcxbool trLoadTable
(  const char* src
,  const char* dst
,  int* tbl
,  int flags
)
   {  const char* p = src
   ;  const char* z = p + strlen(p)

   ;  const char* P = dst
   ;  const char* Z = P + strlen(P)

   ;  mcxbool empty        =  FALSE
   ;  mcxbool complement   =  flags & TR_COMPLEMENT
   ;  mcxbool delete       =  flags & TR_DELETE
   ;  int i

   ;  for (i=0;i<256;i++)
      tbl[i] = 0

   ;  if (complement)
      {
         if (!delete && !*P)
         return FALSE

      ;  while (p<z)
         {
            int p_offset, p_len
         ;  p = trGetRange(p, z, &p_offset, &p_len)

         ;  if (!p)
            return FALSE

         ;  while (--p_len >= 0)
            tbl[p_offset+p_len] = 1
      ;  }
         for (i=0;i<256;i++)
         tbl[i] = tbl[i] ? 0 : delete ? -1 : *P
      ;  return TRUE
   ;  }

      while (p<z)
      {
         int p_offset, p_len, P_offset, P_len
      ;  p = trGetRange(p, z, &p_offset, &p_len)
; /* fprintf(stderr, "___ char(%d), len %d\n", p_offset, p_len) */
      ;  if (P >= Z)
         empty = TRUE
      ;  else
         {  P = trGetRange(P, Z, &P_offset, &P_len)
; /*fprintf(stderr, "___ char(%d), len %d\n", P_offset, P_len) */
      ;  }

      ;  if (!P)
         return FALSE

      ;  if (!p || (!empty && (p_len != P_len)))
         return FALSE

      ;  while (--p_len >= 0)
         tbl[p_offset+p_len] = empty ? -1 : P_offset + p_len
   ;  }

      return TRUE
;  }


const char* trGetRange
(  const char *p
,  const char *z
,  int  *offset
,  int  *len
)
   {  int bound

   ;  p = trGetToken(p, z, offset)

   ;  if (!p)
      return NULL
      
   ;  if (*p == '-')
      {  p = trGetToken(p+1, z, &bound)
      ;  if (!p)
         return NULL

      ;  if (bound <= *offset)
         return NULL

      ;  *len = bound - *offset + 1
   ;  }
      else
      {  *len = 1
   ;  }

   ;  return p
;  }


const char* trGetToken
(  const char *p
,  const char *z
,  int  *tokval
)
#define MCX_HEX(c)   (  (c>='0' && c<='9') ? c-'0'\
                     :  (c>='a' && c<='f') ? c-'a'+10\
                     :  (c>='A' && c<='F') ? c-'A'+10\
                     :  0\
                     )
   {  if (*p == '\\')
      {  if (p+3 >= z)
         return NULL

      ;  if
         (  isdigit((unsigned char) *(p+1))
         && isdigit((unsigned char) *(p+2))
         && isdigit((unsigned char) *(p+3))
         )
         *tokval = 64 * (*(p+1)-'0') + 8  * (*(p+2)-'0') + (*(p+3)-'0')
      ;  else if
         (  *(p+1) == 'x'
         && isxdigit((unsigned char) *(p+2))
         && isxdigit((unsigned char) *(p+3))
         )
         *tokval = 16 * MCX_HEX(*(p+2)) +  MCX_HEX(*(p+3))
      ;  else
         return NULL

      ;  return p+4
   ;  }
      else
      {  *tokval = *p
      ;  return p+1
   ;  }
      return NULL
;  }

