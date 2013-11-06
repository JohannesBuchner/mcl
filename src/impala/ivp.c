/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ivp.h"

#include "util/err.h"
#include "util/alloc.h"
#include "util/types.h"
#include "util/minmax.h"


mclIvp* mclpInstantiate
(  mclIvp*   ivp
,  long      index
,  double    value
)
   {  if (!ivp)
      ivp = mcxAlloc(sizeof(mclIvp), EXIT_ON_FAIL)

   ;  ivp->idx       =  index
   ;  ivp->val       =  value
   
   ;  return ivp
;  }


mclpAR* mclpARinit
(  mclpAR* mclpar
)
   {  if (!mclpar)
      {  mclpar =  mcxAlloc(sizeof(mclpAR), EXIT_ON_FAIL)
      ;  mclpar->ivps = NULL
      ;  mclpar->n_ivps = 0
      ;  mclpar->n_alloc = 0
   ;  }
      return mclpar
;  }


mclpAR* mclpARensure
(  mclpAR* mclpar
,  int     n
)
   {  if (!mclpar && !(mclpar =  mclpARinit(NULL)))
      return NULL

   ;  if
      (  (n > mclpar->n_alloc)
      &&!( mclpar->ivps
         =  mcxNRealloc
            (  mclpar->ivps
            ,  n
            ,  mclpar->n_ivps
            ,  sizeof(mclp)
            ,  mclpInit_v
            ,  RETURN_ON_FAIL
            )
         )
      )
      return NULL

   ;  mclpar->n_alloc = n
   ;  return mclpar
;  }


mclpAR* mclpARfromIvps
(  mclpAR*  mclpar
,  mclp*    ivps
,  int      n
)
   {  mclpar = mclpARensure(mclpar, n)
   ;  if (!mclpar)
      return NULL
   ;  if (n)
      memcpy(mclpar->ivps, ivps, n * sizeof(mclIvp))
   ;  mclpar->n_ivps = n
   ;  return mclpar
;  }


void mclpARfree
(  mclpAR**    mclparp
)  
   {  if (*mclparp)
      {  mcxFree(mclparp[0]->ivps)
      ;  mcxFree(*mclparp)   
      ;  *mclparp   =  NULL
   ;  }
   }


mcxbool mclpGivenValGt
(  mclIvp*        ivp
,  void*          arg
)
   {  double* f = (double*) arg
   ;  if (ivp->val >= *f)
      return TRUE
   ;  return FALSE
;  }


int mclpIdxGeq
(  const void*             i1
,  const void*             i2
)
   {  return ((mclIvp*)i1)->idx >= ((mclIvp*)i2)->idx
;  }


int mclpIdxCmp
(  const void*             i1
,  const void*             i2
)
   {  long d = ((mclIvp*)i1)->idx - ((mclIvp*)i2)->idx
   ;  return d < 0 ? -1 : d > 0 ? 1 : 0
;  }


int mclpIdxRevCmp
(  const void*             i1
,  const void*             i2
)
   {  long d = ((mclIvp*)i2)->idx - ((mclIvp*)i1)->idx
   ;  return d < 0 ? -1 : d > 0 ? 1 : 0
;  }


int mclpValCmp
(  const void*             i1
,  const void*             i2
)
   {  int     s  =  SIGN(((mclIvp*)i1)->val - ((mclIvp*)i2)->val)
   ;  return (s ? s : ((mclIvp*)i1)->idx - ((mclIvp*)i2)->idx)
;  }


int mclpValRevCmp
(  const void*             i1
,  const void*             i2
)
   {  int     s  =  SIGN(((mclIvp*)i2)->val - ((mclIvp*)i1)->val)
   ;  return (s ? s : ((mclIvp*)i1)->idx - ((mclIvp*)i2)->idx)
;  }


void mclpMergeLeft
(  void*                   i1
,  const void*             i2
)
   {
;  }


void mclpMergeRight
(  void*                   i1
,  const void*             i2
)
   {  ((mclIvp*)i1)->val = ((mclIvp*)i2)->val
;  }


void mclpMergeAdd
(  void*                   i1
,  const void*             i2
)
   {  ((mclIvp*)i1)->val += ((mclIvp*)i2)->val
;  }


void mclpMergeMax
(  void*                   i1
,  const void*             i2
)
   {  ((mclIvp*)i1)->val = MAX(((mclIvp*)i1)->val,((mclIvp*)i2)->val)
;  }


void mclpMergeMul
(  void*                   i1
,  const void*             i2
)
   {  ((mclIvp*)i1)->val *= ((mclIvp*)i2)->val
;  }


mclIvp* mclpInit
(  mclIvp*                 ivp
)  
   {  return mclpInstantiate(ivp, -1, 1.0)
;  }


void* mclpInit_v
(  void*                  ivp
)  
   {  return (void*) mclpInstantiate((mclIvp*)ivp, -1, 1.0)
;  }


mclIvp* mclpCreate
(  long   idx
,  double   value
)  
   {  return mclpInstantiate(NULL, idx, value)
;  }


void mclpFree
(  mclIvp**                   p_ivp
)  
   {  if (*p_ivp)
      {  mcxFree(*p_ivp)
      ;  *p_ivp   =  NULL
   ;  }
   }


