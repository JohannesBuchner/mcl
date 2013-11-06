/*   Copyright (C) 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdio.h>
#include <string.h>

#include "tr.h"
#include "ting.h"
#include "ding.h"


   /* returns next (unparsed) character.
    * NULL on error.

    * accepts concatenations of these types of strings:
    *    abcde
    *    a-d   e-e  a-z  A-Z
    *    \001-\016
    *    \000-\255
    *    \x20 \x8a
   */
const char* tr_get_range
(  const char *p
,  const char *z
,  int  *offset
,  int  *len
)  ;


const char* tr_get_token
(  const char *p
,  const char *z
,  int  *tokval
)  ;


int mcxTRTranslate
(  char*    src
,  mcxTR*   tr
)
   {  int i,j
   ;  int prev =  -1          /* only relevant for squash case */
   ;  int len = strlen(src)
   ;  mcxbits flags = tr->modes
   ;  mcxbool squash = flags & MCX_TR_SQUASH
   ;  int* tbl = tr->tbl
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


mcxbool mcxTRLoadTable
(  const char* src
,  const char* dst
,  mcxTR*      tr
,  mcxbits     flags
)
   {  const char* p = src
   ;  const char* z = p + strlen(p)

   ;  const char* P = dst
   ;  const char* Z = P + strlen(P)

   ;  mcxbool empty        =  P >= Z ? TRUE : FALSE
   ;  mcxbool complement   =  flags & MCX_TR_COMPLEMENT
   ;  mcxbool delete       =  flags & MCX_TR_DELETE
   ;  int* tbl = tr->tbl
   ;  int i

   ;  tr->modes = flags

   ;  for (i=0;i<256;i++)
      tbl[i] = 0

   ;  if (complement)
      {
         if (!delete && !*P)
         return FALSE

      ;  while (p<z)
         {
            int p_offset, p_len
         ;  p = tr_get_range(p, z, &p_offset, &p_len)

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
      ;  p = tr_get_range(p, z, &p_offset, &p_len)
; /* fprintf(stderr, "___ char(%d), len %d\n", p_offset, p_len) */

      ;  if (P >= Z)    /* dst is exhausted, rest is mapped to nil */
         empty = TRUE
      ;  else if (!(P = tr_get_range(P, Z, &P_offset, &P_len)))
         return FALSE

      ;  if (!p || (!empty && (p_len != P_len)))
         return FALSE

      ;  while (--p_len >= 0)
         tbl[p_offset+p_len] = empty ? -1 : P_offset + p_len
   ;  }

      return TRUE
;  }


const char* tr_get_range
(  const char *p
,  const char *z
,  int  *lower
,  int  *len
)
   {  int upper = 0
   ;  const char* q = tr_get_token(p, z, lower)

   ;  if (!q)
      return NULL

   ;  if (q<z && *q == '-')
      {  q = tr_get_token(q+1, z, &upper)

      ;  if (!q)
         return NULL

      ;  if (upper < *lower)
         return NULL

      ;  *len = upper - *lower + 1
   ;  }
      else
      *len = 1

   ;  return q
;  }


/* fixme: should pbb cast everything p to unsigned char */

const char* tr_get_token
(  const char *p
,  const char *z
,  int  *tokval
)
#define MCX_HEX(c)   (  (c>='0' && c<='9') ? c-'0'\
                     :  (c>='a' && c<='f') ? c-'a'+10\
                     :  (c>='A' && c<='F') ? c-'A'+10\
                     :  0\
                     )
   {  int c = 0
   ;  if (p >= z)
      return NULL

   ;  if (*p == '\\')
      {  if (p+1 == z)
         {  *tokval = '\\'
         ;  return p + 2
      ;  }
      
         if (p+3 >= z)
         return NULL

      ;  if
         (  isdigit((unsigned char) *(p+1))
         && isdigit((unsigned char) *(p+2))
         && isdigit((unsigned char) *(p+3))
         )
         c = 64 * (*(p+1)-'0') + 8  * (*(p+2)-'0') + (*(p+3)-'0')
      ;  else if
         (  *(p+1) == 'x'
         && isxdigit((unsigned char) *(p+2))
         && isxdigit((unsigned char) *(p+3))
         )
         c = 16 * MCX_HEX(*(p+2)) +  MCX_HEX(*(p+3))
      ;  else
         return NULL

      ;  if (c < 0 || c > 255)
         return NULL

      ;  *tokval = c
      ;  return p+4
   ;  }
      else
      {  c = (unsigned char) *p
      ;  if (c < 0 || c > 255)
         return NULL
      ;  *tokval = c
      ;  return p+1
   ;  }
      return NULL
;  }


mcxTing* mcxTRExpandSpec
(  const char* spec
,  mcxbits  bits
)
   {  mcxTR tr
   ;  int i = 0, n_set = 0
   ;  mcxTing* set = mcxTingEmpty(NULL, 256)
   ;  mcxbits complement = bits & MCX_TR_COMPLEMENT
   ;  char* p
   ;  int* tbl = tr.tbl

   ;  if (!mcxTRLoadTable(spec, spec, &tr, complement))
      return NULL

   ;  for (i=0;i<256;i++)
      {  if
         (  (complement && tbl[i] <= 0)
         || (!complement && tbl[i] > 0)
         )
         set->str[n_set++] = (unsigned char) i
   ;  }
      set->str[n_set] = '\0'
   ;  set->len = n_set

   ;  if ((p = strchr(set->str, '-')))
      {  *p = set->str[0]
      ;  set->str[0] = '-'
   ;  }
      if ((p = strchr(set->str, '\\')))
      {  *p = set->str[set->len-1]
      ;  set->str[set->len-1] = '\\'
   ;  }
      return set
;  }



int mcxTingTranslate
(  mcxTing* src
,  mcxTR*   tr
)
   {  src->len  =  mcxTRTranslate(src->str, tr)
   ;  return src->len
;  }



/* fixme conceivably mcxTRTranslate could return -1 as
 * error condition
*/
int mcxTingTr
(  mcxTing*       txt
,  const char*    src
,  const char*    dst
,  mcxbits        flags
)
   {  mcxTR tr
   ;  if (!mcxTRLoadTable(src, dst, &tr, flags))
      return -1
   ;  txt->len  =  mcxTRTranslate(txt->str, &tr)
   ;  return txt->len
;  }
