/* (c) Copyright 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_tok
#define util_tok

#include <string.h>
#include "types.h"
#include "list.h"
#include "ting.h"



/*  This is a first sketchy attempt at some parse/tokenize routines.
 *  The scope is not very well defined yet. Should it do basic constructs
 *  only, or aim for more power, possibly aided by callbacks, and
 *  god forbid, a state description?
 *
 *  TODO
 *    quoted strings not yet implemented!
 *    SGML not yet implemented!
 *
 *    unify with mcxIOExpect, possibly stuff from ding.h
 *    wide chars?
*/


#define MCX_TOK_MODE_UNIX     1  /* Unix escapes, including esc newlines */
#define MCX_TOK_MODE_QUOTED   2  /* Quotes delimit tokens, hide brackets */ 
#define MCX_TOK_MODE_PLAIN    4
#define MCX_TOK_MODE_SGML     8  /* &code; other? */

#define MCX_TOK_DEL_WS        16 /* only delimiting whitespace */


/*    Returns first character not matching fbool, NULL if none.
*/

char* mcxTokSkip
(  const char* ofs
,  int (*fbool)(int c)
,  int  len
)  ;


/*
 *  Accounts for nesting.
 *  Will do '}', ')', ']', '>', assuming one of several conventions.
*/

mcxstatus mcxTokMatch
(  const char* ofs
,  char**      end
,  mcxbits     mode
,  int         len            /* considered if >= 0 */
)  ;


/*
 * Find some token, skipping over expressions.
 * Either *pos == NULL and retval == STATUS_FAIL
 * or     *pos != NULL and retval == STATUS_OK
 * or     *pos != NULL and retval == STATUS_DONE
*/

mcxstatus mcxTokFind
(  const char* ofs
,  char*       tok            /* Only tok[0] considered for now! */
,  char**      pos
,  mcxbits     mode
,  int         len            /* considered if >= 0 */
)  ;



mcxLink* mcxTokArgs
(  const char* str
,  long        str_len
,  int*        n_args
,  mcxbits     opts
)  ;


typedef struct
{  mcxTing*    key
;  mcxLink*    args
;  mcxbits     opts
;
}  mcxTokFunc  ;


void mcxTokFuncFree
(  mcxTokFunc* tf
)  ;

mcxstatus mcxTokExpectFunc
(  mcxTokFunc* tf
,  const char* str
,  long        str_len
,  char**      z
,  int         n_min
,  int         n_max
,  int        *n_args
)  ;


#endif


