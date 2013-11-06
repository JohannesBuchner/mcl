/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef mcl_expand_h
#define mcl_expand_h

#include <stdio.h>
#include <pthread.h>

#include "util/ting.h"
#include "util/heap.h"
#include "util/types.h"

#include "taurus/ilist.h"
#include "impala/matrix.h"

#define  MCL_PRUNING_RIGID   1
#define  MCL_PRUNING_ADAPT  2

#define  MCL_EXPAND_DENSE  1           
#define  MCL_EXPAND_SPARSE 2    

extern int mclDefaultWindowSizes[];
extern dim mcl_n_windows;


typedef struct
{  double            chaosMax
;  double            chaosAvg
;  double            homgMax
;  double            homgMin
;  double            homgAvg
;  mclv*             homgVec
;  double            lap

;  int               n_selects
;  int               n_recoveries
;  int               n_below_pct
;  int               n_cols         /* nr of cols computed */

;  mclMatrix*        mx_chr

;  mcxTing*          levels_expand
;  mcxTing*          levels_prune

;  mcxIL*            il_levels_expand
;  mcxIL*            il_levels_prune

;  double*           mass_prune_low
;  double*           mass_final_low
;  double            mass_prune_all
;  double            mass_final_all

;  dim               n_windows
;  int*              window_sizes
;  mcxHeap*          windowPrune
;  mcxHeap*          windowFinal

;  int               ny
;  int               nx

;  pthread_mutex_t   mutex
;
}  mclExpandStats    ;


typedef struct
{  mclExpandStats*   stats
;  int               n_ethreads
;  mcxbool           cloneMatrices
;  mcxbool           cloneBarrier

;  int               modePruning
;  int               modeExpand

;  double            precision
;  double            pct

;  dim               num_prune
;  dim               num_select
;  dim               num_recover
;  int               scheme
;  int               my_scheme

;  double            cutCof
;  double            cutExp

#define  XPNVB(mxp, bit)   (mxp->verbosity & bit)

#define  XPNVB_PRUNING     1 << 0
#define  XPNVB_EXPLAIN     1 << 1
#define  XPNVB_CLUSTERS    1 << 2

;  mcxbits           verbosity
;  int               vectorProgression

;  int               warnFactor
;  double            warnPct

;  dim               nl             /* number of iterations to log   */
;  dim               nw             /* number of windows to log      */
;  dim               nx             /* window index (monitored)      */
;  dim               ny             /* window index (monitored)      */
;  dim               nj             /* window index of jury          */
;  mcxIL             *windowSizes
;  dim               n_windows

;  int               dimension
;  double            inflation      /* for computing homg vector     */

;
}  mclExpandParam    ;


mclMatrix* mclExpand
(  const mclMatrix*  mx
,  const mclMatrix*  mxright
,  mclExpandParam*   mxp
)  ;


mclExpandStats* mclExpandStatsNew
(  dim   n_windows
,  int*  window_sizes
,  int   nx          /* one of the heaps */
,  int   ny          /* one of the heaps */
,  const mclVector* dom_cols
)  ;  


void mclExpandParamDim
(  mclExpandParam*  mxp
,  const mclMatrix* mx
)  ;


void mclExpandStatsReset
(  mclExpandStats* stats
)  ;


void mclExpandStatsFree
(  mclExpandStats** statspp
)  ;


void mclExpandStatsPrint
(  mclExpandStats*  stats
,  FILE*             fp
)  ;

void mclExpandAppendLog
(  mcxTing* Log
,  mclExpandStats *s
,  int n_ite
)  ;

void mclExpandInitLog
(  mcxTing* Log
,  mclExpandParam* mxp  
)  ;

void mclExpandStatsHeader
(  FILE* vbfp
,  mclExpandStats* stats
,  mclExpandParam*   mxp
)  ;


mclExpandParam* mclExpandParamNew
(  void
)  ;

void mclExpandParamFree
(  mclExpandParam** epp
)  ;


#endif

