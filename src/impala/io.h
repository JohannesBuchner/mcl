/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_io_h
#define impala_io_h

#include <math.h>
#include <stdio.h>

#include "vector.h"
#include "matrix.h"

#include "util/io.h"
#include "util/types.h"
#include "util/ting.h"


/* TODO:
 *    Make ascii format parsing looser (not line based).
 *    The ascii parsing code is not all that great, a bit heavy-weight.
 *    Then, it would be faster if not fgetc based but does buffering.
 *
 *    There should be a clearer framework for rewinding streams
 *    after gathering partial information (readDomains, readDimensions).
 *
 *    There is now a funky callback mclxIOinfoReset, invoked
 *    by mclxIOclose. Look out for implications.
*/

#define mclxCookie   0x0d141831u     /* MTX1 */
#define mclvCookie   0x16050331u     /* VEC1 */


#define MCL_APP_VB_NO   2
#define MCL_APP_VB_YES  8

#define MCL_APP_WB_NO   2
#define MCL_APP_WB_YES  8


/*                               0           1
 * MCLXIOFORMAT                  ascii       binary
 * MCLXIOVERBOSITY               silent      verbose
*/

mcxbool mclxIOgetQMode        /* quad mode */
(  const char* opt
)  ;

mcxbool mclxIOsetQMode        /* quad mode */
(  const char* opt
,  unsigned long mode
)  ;

                              /* get format: 'a' or 'b' */
int mclxIOformat
(  mcxIO* xf
)  ;

 /* *********************
*/

mclMatrix* mclxRead
(  mcxIO*      xfIn
,  mcxOnFail   ON_FAIL
)  ;



 /* *********************
 *
 *   requires equal domains.
*/

#define MCL_READX_GRAPH 1

mclMatrix* mclxReadx
(  mcxIO*      xfIn
,  mcxOnFail   ON_FAIL
,  mcxbits     bits
)  ;


 /* *********************
 *
 *    colmask and rowmask are assimilated into matrix.
*/

mclMatrix* mclxSubRead
(  mcxIO*      xfIn
,  mclVector*  colmask        /* create submatrix on this domain */
,  mclVector*  rowmask        /* create submatrix on this domain */
,  mcxOnFail   ON_FAIL
)  ;


 /* *********************
 *
 *    colmask and rowmask are assimilated into matrix.
*/

mclMatrix* mclxSubReadx
(  mcxIO* xf
,  mclVector* colmask         /* create submatrix on this domain */
,  mclVector* rowmask         /* create submatrix on this domain */
,  mcxOnFail ON_FAIL
,  mcxbits    bits            /* currently only/always checks for equald domains */
)  ;


 /* *********************
 *
 *
 *    Tries to read cookie, then branches of ascii or binary.
*/
mcxstatus mclxReadDomains
(  mcxIO* xf
,  mclv* dom_cols
,  mclv* dom_rows
)  ;


 /* *********************
*/

mcxstatus mclxReadDimensions
(  mcxIO*      xfIn
,  long        *n_cols
,  long        *n_rows
)  ;



#define MCLXIO_VALUE_NONE    -1
#define MCLXIO_VALUE_INT      0
#define MCLXIO_VALUE_GETENV  -2

#define MCLXIO_FORMAT_ASCII   1
#define MCLXIO_FORMAT_BINARY  2
#define MCLXIO_FORMAT_GETENV  3

mcxstatus mclxWrite
(  const mclMatrix*        mx
,  mcxIO*                  xfout
,  int                     valdigits   /* only relevant for ascii writes */
,  mcxOnFail               ON_FAIL
)  ;

mcxstatus mclxaWrite
(  const mclMatrix*  mx
,  mcxIO*            xfOut
,  int               valdigits
,  mcxOnFail         ON_FAIL
)  ;

mcxstatus  mclxbWrite
(  const mclMatrix*  mtx
,  mcxIO*            xfOut
,  mcxOnFail         ON_FAIL
)  ;


/* Provides easy interface for applications that accept --write-binary option,
 * they can simply pass their boolean along
*/

mcxstatus mclxAppWrite
(  const mclx* mx
,  mcxIO*      xf
,  mcxOnFail   ON_FAIL
,  mcxbool     binmode
)  ;


enum
{  MCLXR_ENTRIES_ADD
,  MCLXR_ENTRIES_MAX
,  MCLXR_ENTRIES_MUL
,  MCLXR_ENTRIES_POP
,  MCLXR_ENTRIES_PUSH

,  MCLXR_VECTORS_ADD
,  MCLXR_VECTORS_MAX
,  MCLXR_VECTORS_MUL
,  MCLXR_VECTORS_POP
,  MCLXR_VECTORS_PUSH
}  ;


mclVector* mclvaReadRaw
(  mcxIO          *xf
,  mclpAR*        ar
,  mcxOnFail      ON_FAIL
,  int            fintok     /* e.g. '\n' or EOF or '$' */
,  mcxbits        warn_repeat
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
)  ;


mcxstatus mclxaSubReadRaw
(  mcxIO          *xf
,  mclMatrix      *mx
,  mclv           *dom_cols
,  mclv           *dom_rows
,  mcxOnFail      ON_FAIL
,  int            fintok     /* e.g. EOF or ')' */
,  mcxbits        warn_repeat
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
,  double (*fltbinary)(pval val1, pval val2)
)  ;



void mcxPrettyPrint
(  const mclMatrix*  mx
,  FILE*             fp
,  int               width
,  int               digits
,  const char        msg[]
)  ;


void mclFlowPrettyPrint
(  const mclMatrix*  mx
,  FILE*             fp
,  int               digits
,  const char        msg[]
)  ;


mcxstatus mclxTaggedWrite
(  const mclMatrix*  mx
,  const mclMatrix*  dom
,  mcxIO*            xfOut
,  int               valdigits
,  mcxOnFail         ON_FAIL
)  ;



void                 mclxBoolPrint
(  mclMatrix*        mx
,  int               mode
)  ;


mcxstatus mclvEmbedRead
(  mclVector*        vec
,  mcxIO*            xfIn
,  mcxOnFail         ON_FAIL
)  ;


mcxstatus mclvWrite
(  const mclVector*  vec
,  mcxIO*            xfOut
,  mcxOnFail         ON_FAIL
)  ;


mcxstatus mclvEmbedWrite
(  const mclVector*  vec
,  mcxIO*            xfOut
)  ;


void mclvaWrite
(  const mclVector*  vec
,  FILE*             fp
,  int               valdigits
)  ;


/*
 *    does *not* accept MCLXIO_VALUE_GETENV
*/
void mclvaDump
(  const mclVector*  vec
,  FILE*             fp
,  int               idxwidth
,  int               valdigits
,  mcxbool           doHeader
)  ;


/*
 *    does *not* accept MCLXIO_VALUE_GETENV
 *
 * The default corresponds with a vector printed in a matrix:
 *    no header nor trail, eov values and vid indeed.
*/

#define MCLVA_DUMP_HEADER_YES 1
#define MCLVA_DUMP_VALUE_NO   2
#define MCLVA_DUMP_VID_NO     4
#define MCLVA_DUMP_EOV_NO     8
#define MCLVA_DUMP_TRAIL_YES 16

void mclvaDump2
(  const mclVector*  vec
,  FILE*             fp
,  int               n_per_line
,  int               valdigits
,  const char*       sep
,  mcxbits           opts
)  ;


mclpAR *mclpaReadRaw
(  mcxIO       *xf
,  mcxOnFail   ON_FAIL
,  int         fintok     /* e.g. EOF or '$' */
)  ;

#endif

