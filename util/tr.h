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
 * README
 *    This interface is not at all POSIX compliant. It might evolve to
 *    optionally be indeed.
 *    However, given some of the braindeadliness of POSIX tr compliance,
 *    I don't think the worlds needs another tr implementation.
 *    My gripe is mainly about derailed syntax such as '[:alpha:0'.
 *    It should go down in a ball of flames, not happily parse.
 *    To be honest, I don't know for sure whether this is a POSIX
 *    lack of requirement or an implementation choice.
 *
 *    I did choose to follow most of the POSIX syntax. It is probably
 *    a sign of weakness.
 *    This interface should be able to do everything a POSIX interface can,
 *    possibly more.
 *
 * -  It allows separate specification of src, dst, del and squash sets.
 * -  Provisionally we accept "^spec" to indicate complement,
 *       for any of src dst del squash sets.
 * -  It uses [*c*20] to denote repeats, rather than [c*20].
 *       rationale: do not slam door shut on new syntax.
 * -  It does not recognize '[a-z]' ranges, only 'a-z'.
 *       rationale: none. If ever, notation will be [-a-z] or similar.
 * -  The magic repeat operator [*c*] stops on boundaries
 *       rationale: I like it.
 *       A boundary is introduced by stop/start of ranges and classes.
 * -  For now, the interface does 1) deletion, 2) translation, 3) squashing.
 *       in the future it may provide a custom order of doing things.
 * 
 *
 * Apart from the fact that you cannot have '\0' in C strings, everything
 * here should work for '\0' as well - specifically the mcxTrTable structure.
 * However, the current interface uses C strings for dst and src and C strings
 * for data.
 *
 * More documentation to follow.
 *
*/

extern const char* mcx_tr_err;
extern mcxbool     mcx_tr_debug;


typedef struct
{  u32      tlt[256]
;  mcxbits  modes
;
}  mcxTR    ;


#define MCX_TR_DEFAULT           0
#define MCX_TR_TRANSLATE   1 <<  1
#define MCX_TR_SQUASH      1 <<  2
#define MCX_TR_DELETE      1 <<  3
#define MCX_TR_SOURCE_C    1 <<  4
#define MCX_TR_DEST_C      1 <<  5
#define MCX_TR_DELETE_C    1 <<  6
#define MCX_TR_SQUASH_C    1 <<  7


#define MCX_TR_COMPLEMENT  1 << 10


mcxstatus mcxTRloadTable
(  const char* src
,  const char* dst
,  const char* delete
,  const char* squash
,  mcxTR*      tr
,  mcxbits     modes
)  ;


  /*  returns new length of string.
   *  fixme: document map/squash semantics.
  */
int mcxTRtranslate
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
    * Currently, there is even no implementation.
   */
mcxTing* mcxTRexpandSpec
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
,  const char*    delete
,  const char*    squash
,  mcxbits        flags
)  ;



#endif

