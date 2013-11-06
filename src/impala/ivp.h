/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_ivp_h
#define impala_ivp_h

#include "ivptypes.h"

#include "util/alloc.h"
#include "util/types.h"


                        /* Index Value Pair */
typedef struct
{  pnum        idx
;  pval        val
;
}  mclIvp      ; 
                        /* from here on, allow the prefix mclp as shorthand for
                         * mclIvp */

#define mclp mclIvp     /* allow it as shorthand for the type as well */


mclIvp* mclpInstantiate
(  mclIvp*  prealloc_ivp
,  long     idx
,  double   value
)  ;


mclIvp* mclpInit
(  mclIvp*  ivp
)  ;


void*  mclpInit_v
(  void*    ivp
)  ;


mclIvp* mclpCreate
(  long     idx
,  double   value
)  ;


void mclpFree
(  mclIvp**   ivp
)  ;


/* arg should be of type double */

mcxbool mclpGivenValGt
(  mclIvp*        ivp
,  void*          arg
)  ;


int mclpIdxGeq
(  const void*             ivp1
,  const void*             ivp2
)  ;


int mclpIdxCmp
(  const void*             ivp1
,  const void*             ivp2
)  ;


int mclpIdxRevCmp
(  const void*             ivp1
,  const void*             ivp2
)  ;


int mclpValCmp
(  const void*             ivp1
,  const void*             ivp2
)  ;


int mclpValRevCmp
(  const void*             ivp1
,  const void*             ivp2
)  ;


/* discard ivp2 */
void mclpMergeLeft
(  void*                   ivp1
,  const void*             ivp2
)  ;


/* discard ivp1 */
void mclpMergeRight
(  void*                   ivp1
,  const void*             ivp2
)  ;


void mclpMergeAdd
(  void*                   ivp1
,  const void*             ivp2
)  ;


void mclpMergeMax
(  void*                   ivp1
,  const void*             ivp2
)  ;


void mclpMergeMul
(  void*                   ivp1
,  const void*             ivp2
)  ;


typedef struct
{  mclIvp*     ivps
;  int         n_ivps
;  int         n_alloc
;  mcxbits     sorted      /* 1 = sorted 2 = no-duplicates */
;
}  mclpAR   ;


mclpAR* mclpARinit
(  mclpAR* mclpar
)  ;


void* mclpARinit_v
(  void* mclpar
)  ;


mclpAR* mclpARensure
(  mclpAR*  mclpar
,  int      n
)  ;


mcxstatus mclpARextend
(  mclpAR*  ar
,  long     idx
,  double   val
)  ;


mclpAR* mclpARfromIvps
(  mclpAR*  mclpar
,  mclp*    ivps
,  int      n
)  ;


void mclpARfree
(  mclpAR**    mclparp
)  ;  


double mclpUnary
(  mclp*    ivp
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)  ;


#endif

