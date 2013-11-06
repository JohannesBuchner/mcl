/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef mcl_mcl_h
#define mcl_mcl_h

#include "interpret.h"
#include "expand.h"

#include "impala/matrix.h"
#include "impala/tab.h"
#include "taurus/ilist.h"

#include "util/opt.h"
#include "util/ting.h"

#include <pthread.h>


typedef struct
{  int                  n_ithreads
;  int                  n_ethreads

;  mcxTing*             fname_expanded
;  mclExpandParam       *mxp

;  int                  marks[5]
;  mcxTing              *massLog    /* expand log, really */

;  int                  devel

#define  MCPVB(mpp, bit)   (mpp->dumping & bit)

#define  MCPVB_ITE       1 << 0
#define  MCPVB_CHR       1 << 1     /* characteristics                     */
#define  MCPVB_CLUSTERS  1 << 2
#define  MCPVB_DAG       1 << 3
#define  MCPVB_SUB       1 << 4     /* dump submatrices                    */
#define  MCPVB_LINES     1 << 5     /* line based dump                     */
#define  MCPVB_CAT       1 << 6     /* all to same destination (dump_stem) */
#define  MCPVB_TAB       1 << 8     /* use tab                             */

;  mcxbits              dumping
;  int                  dump_modulo
;  int                  dump_offset
;  int                  dump_bound
;  mcxTing*             dump_stem
;  mclv*                dump_list
;  const mclTab*        dump_tab

;  double               chaosLimit
;  double               lap
;  int                  n_ite

;  mclVector*           vec_attr

;  double               mainInflation
;  int                  mainLoopLength
                                                     
;  double               initInflation
;  int                  initLoopLength
                                                     
;  int                  printMatrix
;  int                  printDigits

;  double               inflate_expanded
;  mcxbool              expandOnly

;  mclInterpretParam*   ipp
;  int                  dimension /* of input matrix */
;
}  mclProcParam         ;


/* default initialization */

mclProcParam* mclProcParamNew
(  void
)  ;

void mclProcParamFree
(  mclProcParam** ppp
)  ;


/* initialization part that depends on the graph cardinality */

void mclProcPrintInfo
(  FILE* fp
,  mclProcParam* mpp  
)  ;


/*
 * Functions in two different modes, depending whether map->mpp->expandOnly
 * is set or not. Returns intermediate matrix if yes, cluster matrix
 * otherwise.
 *
 * In cluster mode, writes last iterand into the first argument.
*/

mclMatrix*  mclProcess
(  mclMatrix**     mx0
,  mclProcParam*   map
)  ;


void mclSigCatch
(  int sig
)  ;


void  mclDumpMatrix
(  mclMatrix*     mx
,  mclProcParam*  mpp  
,  const char*    affix
,  const char*    postfix
,  int            n
,  mcxbool        print_value
)  ;


#endif

