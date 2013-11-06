/*   Copyright (C) 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea under
 * the terms of the GNU General Public License; either version 2 of the License
 * or (at your option) any later version.  You should have received a copy of
 * the GPL along with tingea, in the file COPYING.
*/

#ifndef util_tr
#define util_tr

#include <string.h>

#include "alloc.h"
#include "types.h"
#include "ting.h"


/*
 * dst and src specifications must have identical range patterns,
 * unless there is no matching part in dst because dst is truncated.
 *
 * excepts concatenation of single characters or ranges.
 * ranges can be of the form
 *    a-z
 *    \040-\177
 *    \x20-\x80
 *
 * Apart from the fact that you cannot have '\0' in C strings, everything
 * here should work for '\0' as well - specifically the mcxTrTable structure.
 * However, the current interface uses C strings for dst and src and C strings
 * for data.
 *
 * More documentation to follow.
 *
*/


typedef struct
{  int      tbl[256]
;  mcxbits  modes
;
}  mcxTR    ;


#define MCX_TR_DEFAULT 0
#define MCX_TR_SQUASH 1
#define MCX_TR_DELETE 2
#define MCX_TR_COMPLEMENT 4

mcxbool mcxTRLoadTable
(  const char*    src
,  const char*    dst
,  mcxTR*         tr
,  mcxbits        flags
)  ;


  /*  returns new length of string.
   *  fixme: document map/squash semantics.
  */
int mcxTRTranslate
(  char*    src
,  mcxTR*   tr
)  ;



   /* excepts only MCX_TR_COMPLEMENT.
    * returns a string in native character set
    * order, except that
    *    1) a hyphen is put in front.
    *    2) a backslash is put at the end,
    * so that the result can be used as a valid specification.
    *
    * This interface is still subject to change.
   */
mcxTing* mcxTRExpandSpec
(  const char* spec
,  mcxbits  bits
)  ;


int mcxTingTranslate
(  mcxTing*       src
,  mcxTR*         tr
)  ;

int mcxTingTr
(  mcxTing*       txt
,  const char*    src
,  const char*    dst
,  mcxbits        flags
)  ;


#endif

