/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* NOTE
 *    This interface is still in progress.
 *
 * TODO
 *    See mclxCatRead TODO.
*/


#ifndef impala_cat_h
#define impala_cat_h

#include <stdio.h>

#include "matrix.h"
#include "vector.h"
#include "tab.h"

#include "util/types.h"


typedef struct
{  mclx*       mx
;  mclx*       mxtp
;  void*       usr
;
}  mclxAnnot   ;


/* TODO
 *    put mclxCatPush callback in the struct
*/

typedef struct
{  mclxAnnot*  level
;  dim         n_level
;  dim         n_alloc  
;  mcxTing*    name
;
}  mclxCat   ;


   /* This define should perhaps be in matrix.h or io.h */
#define     MCLX_READ_SKELETON   MCLX_MODE_UNUSED

#define     MCLX_CATREAD_CLUSTERTREE                     \
                              (  MCLX_PRODUCE_DOMTREE    \
                              |  MCLX_REQUIRE_NESTED     \
                              |  MCLX_REQUIRE_PARTITION  \
                              )

#define     MCLX_CATREAD_CLUSTERSTACK                    \
                              (  MCLX_PRODUCE_DOMSTACK   \
                              |  MCLX_REQUIRE_NESTED     \
                              |  MCLX_REQUIRE_PARTITION  \
                              )

#define     MCLX_CATREAD_MODES                           \
                              (  MCLX_PRODUCE_DOMTREE    \
                              |  MCLX_PRODUCE_DOMSTACK   \
                              |  MCLX_REQUIRE_DOMTREE    \
                              |  MCLX_REQUIRE_DOMSTACK   \
                              |  MCLX_REQUIRE_NESTED     \
                              |  MCLX_ENSURE_ROOT        \
                              |  MCLX_REQUIRE_PARTITION  \
                              |  MCLX_REQUIRE_CANONICAL  \
                              |  MCLX_REQUIRE_GRAPH      \
                              |  MCLX_READ_SKELETON      \
                              )
                                             

/* TODO:
 * Put most of the parameters in an mclxCatParam structure.
 *
 * Later add interface to stream matrices without keeping them
 * all in memory, while keeping the consistency checks.
 *
 * The param structure may also hold callbacks; they will
 * receive a pointer to the cat structure itself and a (void*).
 *
 * currently base_** parameters, if given, require equality.
 *
*/

mcxstatus mclxCatRead
(  mcxIO*      xf
,  mclxCat*    cat
,  dim         n_max
,  mclv*       base_dom_cols
,  mclv*       base_dom_rows
,  mcxbits     bits
)  ;


void mclxCatInit
(  mclxCat*    cat
,  const char* str
)  ;


mcxstatus mclxCatPush
(  mclxCat*    cat
,  mclx*       mx
,  mcxstatus   (*cb1) (mclx* mx, void* cb_data)
,  void*       cb1_data
,  mcxstatus   (*cb2) (mclx* left, mclx* right, void* cb_data)
,  void*       cb2_data
)  ;


mcxstatus mclxCBdomTree
(  mclx* left
,  mclx* right
,  void* cb_data
)  ;


mcxstatus mclxCBdomStack
(  mclx* left
,  mclx* right
,  void* cb_data
)  ;


mcxstatus mclxCBunary
(  mclx* mx
,  void* cb_data
)  ;


mcxstatus mclxCatConify
(  mclxCat* st
)  ;


mcxstatus mclxCatUnconify
(  mclxCat* st
)  ;


void mclxCatReverse
(  mclxCat*    cat
)  ;


mcxstatus mclxCatWrite
(  mcxIO*      xf
,  mclxCat*    cat
,  int         valdigits
,  mcxOnFail   ON_FAIL
)  ;

#define  ENSTRICT_KEEP_OVERLAP   1
#define  ENSTRICT_LEAVE_MISSING  2
#define  ENSTRICT_KEEP_EMPTY     4
#define  ENSTRICT_SPLIT_OVERLAP  8
#define  ENSTRICT_JOIN_MISSING  16

#define  ENSTRICT_TRULY          0
#define  ENSTRICT_REPORT_ONLY    ENSTRICT_KEEP_OVERLAP\
                              |  ENSTRICT_LEAVE_MISSING\
                              |  ENSTRICT_KEEP_EMPTY

/* May change cl->cols and accordingly dom_cols, N_COLS(cl),
 * and the vid members of the columns.
 *
 * ENSTRICT_SPLIT_OVERLAP negates ENSTRICT_KEEP_EMPTY
*/

dim clmEnstrict
(  mclMatrix*  c1
,  dim         *overlap
,  dim         *missing
,  dim         *empty
,  mcxbits     flags
)  ;


/* column domain is that of cl, so information is accessible
 * via the cluster ids in cl
*/

mclMatrix*  clmContingency
(  const mclMatrix*  cl
,  const mclMatrix*  dl
)  ;


#define  MCLX_NEWICK_NONL        1 << 0
#define  MCLX_NEWICK_NOINDENT    1 << 1
#define  MCLX_NEWICK_NONUM       1 << 2
#define  MCLX_NEWICK_NOPTHS      1 << 3      /* singletons do not get parentheses */

mcxTing* mclxCatNewick
(  mclxCat*  cat
,  mclTab*   tab
,  mcxbits   bits
)  ;


#endif

