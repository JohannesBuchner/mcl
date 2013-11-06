/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef mcl_alg_h
#define mcl_alg_h

#include "impala/matrix.h"

#include "util/opt.h"
#include "util/hash.h"

#include "proc.h"
#include "impala/tab.h"


#define ALG_OPT_INFO MCX_OPT_UNUSED

extern mcxOptAnchor mclAlgOptions[];


typedef struct
{  mcxIO*               xfout
;  mclProcParam*        mpp

;  int                  expandDigits

;  double               pre_center
;  double               pre_ctrmaxavg
;  double               pre_diag

;  int                  pre_maxnum
;  double               pre_inflation
;  double               pre_in_gq
;  mcxbool              foundOverlap

#  define   ALG_DO_APPEND_LOG          1
#  define   ALG_DO_ANALYZE             2
#  define   ALG_DO_SHOW_LOG            4
#  define   ALG_DO_KEEP_OVERLAP        8
#  define   ALG_DO_CHECK_CONNECTED    16
#  define   ALG_DO_FORCE_CONNECTED    32
#  define   ALG_DO_OUTPUT_LIMIT       64

#  define   ALG_DO_FULL_MONTY    ((unsigned long) ~0)

;  mcxbits              modes

;  mcxbits              stream_modes
;  mclTab*              tab
;  mcxTing*             stream_tab_wname
;  mcxTing*             stream_tab_rname
;  mcxbool              stream_write_labels
;  mcxTing*             stream_yield

;  mcxTing*             stream_transform_spec
;  mclpAR*              stream_transform

;  mcxTing*             transform_spec
;  mclpAR*              transform

;  mcxTing*             cache_mxin
;  mcxTing*             cache_mxtf
;  mcxbool              is_transformed    /* when reading back cached graph */

;  int                  writeMode
;  int                  sortMode
;  mcxTing*             cline
;  mcxTing*             fnin
;
}  mclAlgParam          ;


mclAlgParam* mclAlgParamNew
(  mclProcParam* mpp
,  const char* prog
,  const char* fn
)  ;

void mclAlgParamFree
(  mclAlgParam** app
)  ;

enum
{  ALG_INIT_OK    =  0
,  ALG_INIT_DONE
,  ALG_INIT_FAIL
}  ;

/* returns one of the above */

mcxstatus mclAlgorithmInit
(  const mcxOption* opts
,  mcxHash*       hashedOpts
,  const char*    fname
,  mclAlgParam*   map
)  ;

void mclAlgOptionsInit
(  void
)  ;

mcxstatus mclAlgorithm
(  mclMatrix*     themx
,  mclAlgParam*   map
)  ;

mclx* mclAlgorithmRead
(  mclAlgParam* map
,  mcxbool reread
)  ;

int mclAlgorithmTransform
(  mclx* mx  
,  mclAlgParam* mlp
)  ;

mcxstatus mclAlgorithmCacheGraph
(  mclx* mx
,  mclAlgParam* mlp
,  char ord
)  ;

#endif

