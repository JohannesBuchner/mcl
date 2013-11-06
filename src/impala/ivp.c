/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
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
#include "pval.h"

#include "util/err.h"
#include "util/alloc.h"
#include "util/array.h"
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
      mclpar =  mcxAlloc(sizeof(mclpAR), EXIT_ON_FAIL)

   ;  if (!mclpar)
      return NULL

   ;  mclpar->ivps      =  NULL
   ;  mclpar->n_ivps    =  0
   ;  mclpar->n_alloc   =  0
   ;  mclpar->sorted    =  3

   ;  return mclpar
;  }


void* mclpARinit_v
(  void* mclpar
)
   {  return mclpARinit(mclpar)
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
            ,  mclpar->n_alloc
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


mcxstatus mclpARextend
(  mclpAR*  ar
,  long     idx
,  double   val
)
   {  mclp* ivp = NULL
   ;  if (ar->n_ivps >= ar->n_alloc)
      {  long n_new_alloc = 4 + 1.44 * ar->n_alloc
      ;  if
         (! (  ar->ivps
            =  mcxNRealloc
               (  ar->ivps
               ,  n_new_alloc
               ,  ar->n_alloc
               ,  sizeof(mclp)
               ,  mclpInit_v
               ,  RETURN_ON_FAIL
         )  )  )
         return STATUS_FAIL
      ;  ar->n_alloc = n_new_alloc
   ;  }

      ivp = ar->ivps + ar->n_ivps
   ;  ivp->val =  val
   ;  ivp->idx =  idx

   ;  if (ar->n_ivps && ivp[-1].idx >= idx)
      {  if (ivp[-1].idx > idx)
         BIT_OFF(ar->sorted, 3)
      ;  else
         BIT_OFF(ar->sorted, 2)
   ;  }

      ar->n_ivps++
   ;  return STATUS_OK
;  }


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
   {  return mclpInstantiate(ivp, -1, 1.0)
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


#if 0
enum
{  MCLX_UNARY_LT  =  0
,  MCLX_UNARY_LQ
,  MCLX_UNARY_GQ
,  MCLX_UNARY_GT
,  MCLX_UNARY_RAND
,  MCLX_UNARY_MUL
,  MCLX_UNARY_SCALE
,  MCLX_UNARY_ADD 
,  MCLX_UNARY_CEIL
,  MCLX_UNARY_FLOOR 
,  MCLX_UNARY_POW
,  MCLX_UNARY_EXP
,  MCLX_UNARY_LOG
,  MCLX_UNARY_NEGLOG
,  MCLX_UNARY_UNUSED
}  ;
#endif

double (*mclp_unary_tab[])(pval, void*)
=
{  fltxLT
,  fltxLQ
,  fltxGT
,  fltxGT
,  fltxRand
,  fltxMul
,  fltxScale
,  fltxAdd 
,  fltxCeil
,  fltxFloor 
,  fltxPower
,  fltxExp
,  fltxLog
,  fltxNeglog
,  NULL           /* (double (f*)(pval* flt, void*arg)) NULL */
}  ;


double mclpUnary
(  mclp*    ivp
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)
   {  int i
   ;  double val = ivp->val
   ;  for (i=0;i<ar->n_ivps;i++)
      {  int mode = ar->ivps[i].idx
      ;  double arg = ar->ivps[i].val
      ;  if (mode < 0 || mode >= MCLX_UNARY_UNUSED)
         {  mcxErr("mclpUnary", "not a mode: %d", mode)
         ;  break
      ;  }
         val = mclp_unary_tab[mode](val, &arg)
   ;  }
      return val
;  }


