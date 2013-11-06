/*   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef mcl_clm_h
#define mcl_clm_h

#include "impala/matrix.h"
#include "taurus/ilist.h"
#include "util/types.h"

#define  ENSTRICT_KEEP_OVERLAP   1
#define  ENSTRICT_LEAVE_MISSING  2
#define  ENSTRICT_KEEP_EMPTY     4

#define  ENSTRICT_TRULY          0
#define  ENSTRICT_REPORT_ONLY    ENSTRICT_KEEP_OVERLAP\
                              |  ENSTRICT_LEAVE_MISSING\
                              |  ENSTRICT_KEEP_EMPTY

int  mclcEnstrict
(  mclMatrix*  c1
,  int         *overlap
,  int         *missing
,  int         *empty
,  mcxbits     flags
)  ;



/* 
 *    variance of information
 *    Comparing Clusterings, Marina Meila, Department of Statistics,
 *    University of Washington.
*/
void mclcVIDistance
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  const mclMatrix*  abmeet
,  double* abdist
,  double* badist
)  ;


void mclcSJDistance     /* split join distance */
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  const mclMatrix*  abmeet
,  const mclMatrix*  bameet
,  int*        abdist
,  int*        badist
)  ;


void mclcJQDistance     /* Jacquard index */
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  const mclMatrix*  abmeet
,  double*     abdist
,  double*     badist
)  ;


mclMatrix*  mclcMeet
(  const mclMatrix*  c1
,  const mclMatrix*  c2
,  mcxstatus   ON_FAIL
)  ;


mclMatrix*  mclcSeparate
(  const mclMatrix*  cl
)  ;


mclMatrix* mclcProject
(  const mclMatrix*  cl
,  const mclVector*  dom
)  ;


/* compute all connected components in the subgraphs induced by dom.
 * Just compute connected components in case dom == NULL.
*/

mclMatrix*  mclcComponents
(  const mclMatrix*  mx
,  const mclMatrix*  dom
)  ;


mclMatrix*  mclcContingency
(  const mclMatrix*  cl
,  const mclMatrix*  dl
)  ;


mcxstatus clmPerformance
(  const mclMatrix* mx
,  const mclMatrix* cl
,  double vals[5]
)  ;


mcxstatus clmGranularity
(  const mclx* cl
,  double vals[8]
)  ;


void clmGranularityPrint
(  FILE* fp
,  const char* info
,  double vals[8]
,  int do_header
)  ;


void clmPerformancePrint
(  FILE* fp
,  const char* info
,  double vals[5]
,  int do_header
)  ;

#endif

