/* (c) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

/* STATUS:
 *    usable:  yes
 *    tested:  yes, stress-tested in zoem and mcl
 *    ad hoc:  somewhat
 *    quirks:  probably a few
 *    support: limited
 *
 * AIMS
 *    -  Provide convenient and efficient wrappers for reading lines, files, searching.
 *    -  Within these wrappers, account bytes and lines read.
 *    It is explicitly not an aim to be an all-encompassing interface, wrapping
 *    everything provided by stdio.h.  The type is not opaque and you are
 *    encouraged to inspect its fp member.
 *
 * TODO:
 * ?  support for pipes
 * -  design {reset,close} framework, esp related to usr member.
*/

#ifndef util_file_h
#define util_file_h

#include <stdio.h>
#include <sys/types.h>

#include "ting.h"
#include "types.h"


/* Uh, the thing below seems a reasonable test for seekability.
 * I tried to look for a common idiom, but so far no luck.
 * Let's agree that the main thing is the encapsulation.
 *
 * A better name would thus be mcxFPseemsSeekable.
*/

#define mcxFPisSeekable(fp) (!fseek(fp, 0, SEEK_CUR))



/*  **************************************************************************
 * *
 **            Implementation notes.
 *
 *
 *    This is meant to be a lightweight layer for file operations.
 *    It is so lightweight that the pivotal data structure is not hidden.
 *
 *    Basic usage:
 *       mcxIO* xf = mcxIOnew(somestr, "r");
 *       mcxIOopen(xf, EXIT_ON_FAIL);
 *
 *
 *    Searching:
 *       mcxIOfind(xf, pattern, ON_FAIL)
 *
 *
 *    Reading lines:
 *       mcxIOreadLine(xf, txt, mode)
 *       modes (xor'ed bits):
 *          MCX_READLINE_CHOMP
 *          MCX_READLINE_SKIP_EMPTY
 *          MCX_READLINE_PAR  (read a paragraph)
 *          MCX_READLINE_BSC  (backslash continues line)
 *          MCX_READLINE_DOT  (single dot on single line ends paragraph)
 *    Reading files:
 *       mcxIOreadFile(xf, txt)
 *
 *
 *    Reading bytes:
 *       int c = mcxIOstep(xf)
 *       mcxIOstepback(c, xf)
 *
 *       These keep track of byte count, line count, and ofset within line.
 *
 *
 *    Reset attributes for file name object - change name or mode.
 *       mcxIOrenew(xf, name, mode)
 *
 *
 *    There are some more small utility functions.
 *
    **************************************************************************
   *
  *
 *
 * TODO:
 *    much todo about everything.
 *
 *    mcxIOdiscardLine
 *    mcxIOskipSpace
 *       Change to instance of sth more general.
 *
*/


#define mcxIOateof(xf)  (xf->ateof)
#define mcxIOstdio(xf)  (xf->stdio)
#define mcxIOlc(xf)     ((long) ((xf->lc) + (xf->lo ? 1 : 0)))
                              /* this also takes care of EOF
                               * not preceded by a newline
                              */

/* As long as you did not use mcxIOopen, feel free to do anything with the fn
 * member, especially right after mcxIOnew.
*/

typedef struct
{  mcxTing*       fn
;  char*          mode
;  FILE*          fp
;  size_t         lc       /*    line count        */
;  size_t         lo       /*    line offset       */
;  size_t         lo_      /*    line offset backup, only valid when lo == 0 */
;  size_t         bc       /*    byte count        */
;  int            ateof
;  int            stdio
;  long           pos      /*    handle for fseek  */
;  void*          usr      /*    user object       */
;  void         (*usr_reset)(void*)    /*  function to reset user object */
;  void         (*usr_free)(void*)     /*  function to free user object  */
;
}  mcxIO    ;


/*
 *    mcxIOrenew does *not* support callback for resetting the usr object
*/

mcxIO* mcxIOrenew
(  mcxIO*         xf
,  const char*    name
,  const char*    mode
)  ;


mcxIO* mcxIOnew
(  const char*    name
,  const char*    mode
)  ;


mcxstatus mcxIOopen
(  mcxIO*         xf
,  mcxOnFail      ON_FAIL
)  ;


mcxstatus mcxIOtestOpen
(  mcxIO*         xf
,  mcxOnFail      ON_FAIL
)  ;


/*
 *    mcxIOfree does *not* support callback for freeing the usr object
*/

void mcxIOfree
(  mcxIO**  xf
)  ;


void mcxIOfree_v
(  void*  xfpp
)  ;  

void mcxIOrelease
(  mcxIO*   xf
)  ;


void mcxIOerr
(  mcxIO*   xf
,  const char     *complainer
,  const char     *complaint
)  ;


/* Currently, for stdin/stdout/stderr clearerr is issued if necessary.
 * This makes e.g. repeated reads from STDIN possible.
*/
void mcxIOclose
(  mcxIO       *xf
)  ;


mcxstatus  mcxIOreadFile
(  mcxIO       *xf
,  mcxTing     *fileTxt
)  ;


#define MCX_READLINE_DEFAULT      0
#define MCX_READLINE_CHOMP        1
#define MCX_READLINE_SKIP_EMPTY   2
#define MCX_READLINE_PAR          4
#define MCX_READLINE_BSC          8
#define MCX_READLINE_DOT          16

mcxstatus  mcxIOreadLine
(  mcxIO       *xf
,  mcxTing     *lineTxt
,  mcxbits     flags
)  ;


ssize_t mcxIOappendChunk
(  mcxIO        *xf
,  mcxTing      *dst
,  ssize_t      sz
,  mcxbits      flags
)  ;


/*    returns count of discarded characters.
*/

int mcxIOdiscardLine
(  mcxIO       *xf
)  ;


/* OK to call this after mcxIOnew, before mcxIOopen */

mcxstatus mcxIOnewName
(  mcxIO*    xf
,  const char* newname
)  ;


/* OK to call this after mcxIOnew, before mcxIOopen */

mcxstatus mcxIOappendName
(  mcxIO*    xf
,  const char* suffix
)  ;


int mcxIOstep
(  mcxIO*    xf
)  ;


int mcxIOstepback
(  int c
,  mcxIO*    xf
)  ;


void mcxIOpos
(  mcxIO*   xf
,  FILE*    channel
)  ;


void mcxIOlistParmodes
(  void
)  ;


/*    
 *    Returns count of trailing characters in str not matching.
*/

int mcxIOexpect
(  mcxIO*         xf
,  const char*    str
,  mcxOnFail      ON_FAIL
)  ;

mcxstatus mcxIOexpectReal
(  mcxIO*         xf
,  double*        dblp
,  mcxOnFail      ON_FAIL
)  ;

mcxstatus mcxIOexpectNum
(  mcxIO*         xf
,  long*          lngp
,  mcxOnFail      ON_FAIL
)  ;


/*
 *    Returns next non-white space char,
 *    which is pushed back onto stream after reading.
*/

int mcxIOskipSpace
(  mcxIO*        xf
)  ;


/*
 *    Purpose: find str in file. If str is found file pointer is set at the end
 *    of match (fgetc or mcxIOstep would retrieve the next byte), otherwise,
 *    the stream is at EOF.
 *
 *    Internally this uses Boyer Moore Horspool (bmh) search.
 *    It processes the stream with fgetc, so the input file need not be
 *    seekable. This means that finding is relatively slow.
 *
 *    An improvement would be to implement faster input munging for seekable
 *    streams, (using reads of size pagesize) and then reposition the stream
 *    after searching.
 *
 *    That raises the questions: is ftell *garantueed* to set EBADF for
 *    non-seekable streams?
*/

mcxstatus mcxIOfind
(  mcxIO*         xf
,  const char*    str
,  mcxOnFail      ON_FAIL
)  ;


mcxbool mcxIOtryCookie
(  mcxIO*        xfin
,  unsigned      number
)  ;

mcxbool mcxIOwriteCookie
(  mcxIO*        xfout
,  unsigned      number
)  ;



#endif

