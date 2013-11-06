/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "vector.h"
#include "ivp.h"
#include "iface.h"
#include "pval.h"

#include "util/compile.h"
#include "util/alloc.h"
#include "util/types.h"
#include "util/array.h"
#include "util/minmax.h"
#include "util/err.h"


void mclvRuntime
(  const mclVector* vec
,  const char* caller
)
   {  if (!vec)
         mcxErr(caller, "void vector argument")
      ,  mcxExit(1)
;  }


#ifdef RUNTIME_INTEGRITY
#  define MCLV_CHECK(vec, caller) mclvRuntime(vec, caller)
#else
#  define MCLV_CHECK(vec, caller)
#endif


mcxstatus mclvCheck
(  const mclVector*        vec
,  long                    min
,  long                    max
,  mcxbits                 bits
,  mcxOnFail               ON_FAIL
)  
   {  mclIvp*  ivp      =  vec->ivps
   ;  mclIvp*  ivpmax   =  vec->ivps+vec->n_ivps
   ;  long     last     =  -1
   ;  const char* me    =  "mclvCheck"
   ;  mcxbool  ok       =  TRUE

   ;  if (vec->n_ivps && !vec->ivps)
         mcxErr(me, "deadly: NULL ivps and %ld n_ivps", (long) vec->n_ivps)
      ,  ok = FALSE
   ;  else if (vec->n_ivps && min >= 0 && MCLV_MINID(vec) < min)
         mcxErr
         (me, "daemons: MINID %ld less than %ld", (long) MCLV_MINID(vec), min)
      ,  ok = FALSE

   ;  while (ok && ivp<ivpmax)
      {  if (ivp->idx <= last)
         {  mcxErr
            (  me
            ,  "deadly: index %s <%ld, %ld> at ivp <%ld>"
            ,  ivp->idx == last
                  ?  "repeat"
                  :  "descent"
            ,  (long) last
            ,  (long) ivp->idx
            ,  (long) (ivp - vec->ivps)
            )
         ;  ok    =  FALSE
         ;  break
      ;  }

         if
         (  (bits & MCLV_CHECK_POSITIVE && ivp->val < 0.0)
         || (bits & MCLV_CHECK_NONZERO && ivp->val == 0.0)
         )
         {  mcxErr
            (  me
            ,  "error: value <%f> at ivp <%ld>"
            ,  (double) ivp->val
            ,  (long) (ivp - vec->ivps)
            )
         ;  ok    =  FALSE
         ;  break
      ;  }

         last = ivp->idx
      ;  ivp++
   ;  }

      if (ok && max >= 0 && last > max)
      {  mcxErr
         (  me
         ,  "deadly: index <%ld> tops range <%ld> at ivp <%ld>"
         ,  (long) last
         ,  (long) max
         ,  (long) (ivp - 1 - vec->ivps)
         )
      ;  ok    =  FALSE
   ;  }

      if (!ok && (ON_FAIL == EXIT_ON_FAIL))
      mcxExit(1)

   ;  return ok ? STATUS_OK : STATUS_FAIL
;  }


void* mclvInit_v
(  void* vecv
)  
   {  mclv *vec = vecv
   ;  if (!vec && !(vec = mcxAlloc(sizeof(mclVector), RETURN_ON_FAIL)))
      return NULL
   ;  vec->ivps   =  NULL
   ;  vec->n_ivps =  0
   ;  vec->vid    =  -1
   ;  vec->val    =  0.0
   ;  return vec
;  }


mclVector* mclvInit
(  mclVector*              vec
)  
   {  return mclvInit_v(vec)
;  }


/* this routine should always work also for non-conforming vectors */

mclVector* mclvInstantiate
(  mclVector*     dst_vec
,  int            new_n_ivps
,  const mclIvp*  src_ivps
)
   {  mclIvp*     new_ivps
   ;  int         old_n_ivps

   ;  if (!dst_vec && !(dst_vec = mclvInit(NULL)))    /* create */
      return NULL

   ;  old_n_ivps = dst_vec->n_ivps

   ;  if
      ( !(  dst_vec->ivps
         =  mcxRealloc
            (  dst_vec->ivps
            ,  new_n_ivps * sizeof(mclIvp)
            ,  RETURN_ON_FAIL
            )
         )
      && new_n_ivps
      )
      {  mcxMemDenied(stderr, "mclvInstantiate", "mclIvp", new_n_ivps)
     /*  do not free; *dst_vec could be array element */
      ;  return NULL
   ;  }

      new_ivps = dst_vec->ivps

   ;  if (!src_ivps)                                  /* resize */
      {  int k = old_n_ivps
      ;  while (k < new_n_ivps)
         {  mclpInit(new_ivps + k)
         ;  k++
      ;  }
      }
      else if (src_ivps && new_n_ivps)                /* copy   */
      memcpy(new_ivps, src_ivps, new_n_ivps * sizeof(mclIvp))

   ;  dst_vec->n_ivps = new_n_ivps
   ;  return dst_vec
;  }



mclVector* mclvNew
(  mclp*    ivps
,  int      n_ivps
)
   {  return mclvInstantiate(NULL, n_ivps, ivps)
;  }


mclVector* mclvRenew
(  mclv*    dst
,  mclp*    ivps
,  int      n_ivps
)
   {  return mclvInstantiate(dst, n_ivps, ivps)
;  }


mclVector*  mclvResize
(  mclVector*              vec
,  int                     n_ivps
)  
   {  return mclvInstantiate(vec, n_ivps, NULL)
;  }


double mclvIn
(  const mclVector*        lft
,  const mclVector*        rgt
)
   {  double ip = 0.0
   ;  mclp 
         *ivp1    = lft->ivps
      ,  *ivp2    = rgt->ivps
      ,  *ivp1max = ivp1 + lft->n_ivps
      ,  *ivp2max = ivp2 +rgt->n_ivps

   ;  while (ivp1 < ivp1max && ivp2 < ivp2max)
      {  pval val1 =  0.0
      ;  pval val2 =  0.0

      ;  if (ivp1->idx < ivp2->idx)
         val1  =  (ivp1++)->val
      ;  else if (ivp1->idx > ivp2->idx)
         val2  =  (ivp2++)->val
      ;  else
            val1  =  (ivp1++)->val
         ,  val2  =  (ivp2++)->val

      ;  ip += val1 * val2
   ;  }
      return ip
;  }


mclVector* mclvClone
(  const mclVector*  src
)
   {  return mclvCopy(NULL, src)
;  }


mclVector* mclvCopy
(  mclVector*        dst
,  const mclVector*  src
)  
   {  if (!src)
      return NULL
   ;  return mclvInstantiate(dst, src->n_ivps, src->ivps)
;  }


void mclvRelease
(  mclVector*        vec
)
   {  if (!vec)
      return
   ;  mcxFree(vec->ivps)
   ;  vec->ivps = NULL
   ;  vec->n_ivps = 0
;  }


void mclvFree
(  mclVector**                 vecpp
)  
   {  if (*vecpp)
      {  mcxFree((*vecpp)->ivps)
      ;  mcxFree(*vecpp)
      ;  (*vecpp) = NULL
   ;  }
;  }


void mclvFree_v
(  void*  vecpp
)  
   {  mclvFree(vecpp)
;  }


void mclvCleanse
(  mclVector*  vec
)
   {  mclvSort(vec, mclpIdxCmp)
   ;  mclvUniqueIdx(vec, mclpMergeLeft)
;  }


void mclvSortDescVal
(  mclVector*              vec
)
   {  mclvSort(vec, mclpValRevCmp)
;  }


void mclvSortAscVal
(  mclVector*              vec
)
   {  mclvSort(vec, mclpValCmp)
;  }


void mclvSort
(  mclVector*   vec
,  int         (*cmp)(const void*, const void*)
)  
   {  MCLV_CHECK(vec, "mclvSort")

   ;  if (!cmp)
      cmp = mclpIdxCmp

   ;  if (vec->n_ivps)
      qsort(vec->ivps, vec->n_ivps, sizeof(mclIvp), cmp)
;  }


int mclvUniqueIdx
(  mclVector*              vec
,  void (*merge)(void* ivp1, const void* ivp2)
)  
   {  int n = 0, diff = 0
   ;  MCLV_CHECK(vec, "mclvUniqueIdx")

   ;  if (vec->n_ivps)
      n = mcxDedup
      (  vec->ivps
      ,  vec->n_ivps
      ,  sizeof(mclIvp)
      ,  mclpIdxCmp
      ,  merge
      )
   ;  diff = vec->n_ivps - n
   ;  vec->n_ivps = n
   /* fixme; should/could realloc */
   ;  return diff
;  }


long  mclvTop
(  mclVector      *vec
,  double         psum
,  double*        maxptr
,  double*        minptr
,  mcxbool        select
)
   {  long i = 0
   ;  double acc = 0.0

   ;  mclvSort(vec, mclpValRevCmp)  /* DANGER vector now not conforming */

   ;  while (i<vec->n_ivps && acc < psum)
         acc += vec->ivps[i].val
      ,  i++

   ;  if (select)
      mclvResize(vec, i)

   ;  if (maxptr)
      *maxptr = i ? vec->ivps[0].val : DBL_MAX

   ;  if (minptr && i)
      *minptr = i ? vec->ivps[i-1].val : -DBL_MAX

   ;  mclvSort(vec, mclpIdxCmp)
   ;  return i
;  }


double mclvKBar
(  mclVector   *vec
,  int         k
,  double      ignore            /*    ignore elements relative to this  */
,  int         mode
)  
   {  int      bEven       =  (k+1) % 2
   ;  int      n_inserted  =  0
   ;  double   ans         =  0.0

   ;  mclIvp*  vecIvp      =  vec->ivps
   ;  mclIvp*  vecMaxIvp   =  vecIvp + vec->n_ivps

   ;  pval *heap

   ;  if (k >= vec->n_ivps)
      return(-1.0)

   ;  heap =  mcxAlloc ((k+bEven)*sizeof(pval), EXIT_ON_FAIL)
      /* fixme; enomem */

   ;  if (mode == KBAR_SELECT_LARGE)
      {  if (bEven)
         *(heap+k) =  PVAL_MAX

      ;  while(vecIvp < vecMaxIvp)
         {  pval val =  vecIvp->val

         ;  if (val >= ignore)
            {
         ;  }
            else if (n_inserted < k)
            {  int i =  n_inserted

            ;  while (i != 0 && *(heap+(i-1)/2) > val)
               { *(heap+i) =  *(heap+(i-1)/2)
               ;  i = (i-1)/2
            ;  }
               *(heap+i) =  val
            ;  n_inserted++
         ;  }
            else if (val > *heap)
            {  int root  =  0
            ;  int d

            ;  while((d = 2*root+1) < k)
               {  if (*(heap+d) > *(heap+d+1))
                  d++
               ;  if (val > *(heap+d))
                  {  *(heap+root) = *(heap+d)
                  ;  root = d
               ;  }
                  else
                  break
            ;  }
               *(heap+root) = val
         ;  }

            vecIvp++
      ;  }
      }

      else if (mode == KBAR_SELECT_SMALL)
      {  if (bEven)
         *(heap+k) = -PVAL_MAX

      ;  while(vecIvp < vecMaxIvp)
         {  pval val = vecIvp->val

         ;  if (val < ignore)
            {
         ;  }
            else if (n_inserted < k)
            {  int i = n_inserted

            ;  while (i != 0 && *(heap+(i-1)/2) < val)
               { *(heap+i) = *(heap+(i-1)/2)
               ;  i = (i-1)/2
            ;  }
               *(heap+i) = val
            ;  n_inserted++
         ;  }
            else if (val < *heap)
            {  int root = 0
            ;  int d

            ;  while((d = 2*root+1) < k)
               {  if (*(heap+d) < *(heap+d+1))
                  d++
               ;  if (val < *(heap+d))
                  {  *(heap+root) = *(heap+d)
                  ;  root = d
               ;  }
                  else
                  break
            ;  }
               *(heap+root) = val
         ;  }
            vecIvp++
      ;  }
      }
      else
      {  mcxErr("mclvKBar PBD", "invalid mode")
      ;  mcxExit(1)
   ;  }

      ans = *heap
   ;  mcxFree(heap)

   ;  return ans
;  }


double mclvSelectGqBar
(  mclVector* vec
,  double     fbar
)
   {  mclIvp *newivp, *oldivp, *maxivp
   ;  double mass = 0.0      

   ;  MCLV_CHECK(vec, "mclvSelectGqBar")

   ;  newivp = vec->ivps
   ;  oldivp = newivp
   ;  maxivp = newivp+vec->n_ivps

   ;  while (oldivp < maxivp)
      {  if (oldivp->val >= fbar)
         {  mass += oldivp->val
         ;  *newivp = *oldivp
         ;  newivp++
      ;  }
         oldivp++
   ;  }

      mclvResize(vec, newivp - (vec->ivps))
         /* fixme; enomen (conceivable?:shrinking) */
   ;  return mass
;  }


int mclvCountGiven
(  mclVector*     src
,  mcxbool       (*operation)(mclIvp* ivp, void* arg)
,  void*          arg
)  
   {  int         n_src    =  src->n_ivps
   ;  mclIvp      *src_ivp =  src->ivps
   ;  int         n_hits   =  0

   ;  while (--n_src >= 0)
      {  if (operation(src_ivp, arg))
         n_hits++
      ;  src_ivp++
   ;  }
      return n_hits
;  }


typedef struct
{  double*     lft
;  double*     rgt
;  mcxbits     equate   /* 1: lq, 2: gq */
;
}  mclpRange   ;


mcxbool mclpSelectValues
(  mclp     *ivp
,  void     *range
)
   {  mclpRange* rng = (mclpRange*) range
   ;  double val = ivp->val
   ;  double* lft = rng->lft
   ;  double* rgt = rng->rgt

   ;  if
      (  (  rgt
         && 
            (  val > rgt[0]
            || (rng->equate & MCLX_EQT_LT && val >= rgt[0])
            )
         )
      || (  lft
         && 
            (  val < lft[0]
            || (rng->equate & MCLX_EQT_GT && val <= lft[0])
            )
         )
      )
      return FALSE
   ;  return TRUE
;  }


double mclvSelectValues
(  mclv        *src
,  double      *lft
,  double      *rgt
,  mcxbits     equate
,  mclv        *dst
)
   {  mclpRange range
   ;  range.lft = lft
   ;  range.rgt = rgt
   ;  range.equate = equate

   ;  mclvCopyGiven(src, dst, mclpSelectValues, &range, 0)
   ;  return mclvSum(dst)
;  }


mclVector* mclvCopyGiven
(  mclVector*     dst
,  mclVector*     src
,  mcxbool        (*operation)(mclIvp* ivp, void* arg)
,  void*          arg
,  int            sup
)  
   {  int         n_src
   ;  mclIvp      *src_ivp, *dst_ivp

                        /* dst allowed to be NULL */
   ;  if (dst != src)
      dst = mclvInstantiate(dst, sup ? sup : src->n_ivps, NULL)
   ; /*
      * else we must not destroy src before it is copied
     */

      n_src       =  src->n_ivps
   ;  src_ivp     =  src->ivps
   ;  dst_ivp     =  dst->ivps

   /* BEWARE: this routine must work if dst==src */
   
   ;  while (--n_src >= 0 && dst_ivp < dst->ivps + dst->n_ivps)
      {  if (operation(src_ivp, arg))
         {  dst_ivp->idx =  src_ivp->idx
         ;  dst_ivp->val =  src_ivp->val
         ;  dst_ivp++
      ;  }
         src_ivp++
   ;  }

      mclvResize(dst, dst_ivp - dst->ivps)
   ;  return dst
;  }


void mclvUnary
(  mclVector*  vec
,  double     (*operation)(pval val, void* arg)
,  void*       arg
)  
   {  int      n_ivps
   ;  mclIvp   *src_ivp, *dst_ivp

   ;  MCLV_CHECK(vec, "mclvUnary")

   ;  n_ivps   =  vec->n_ivps
   ;  src_ivp  =  vec->ivps
   ;  dst_ivp  =  vec->ivps
   
   ;  while (--n_ivps >= 0)
      {  double val =  operation(src_ivp->val, arg)

      ;  if (val != 0.0)
         {  dst_ivp->idx =  src_ivp->idx
         ;  dst_ivp->val =  val
         ;  dst_ivp++
      ;  }
         src_ivp++
   ;  }
      mclvResize(vec, dst_ivp - vec->ivps)
;  }


static int update_small_large
(  mclVector*  v1
,  const mclVector*  v2
,  double  (*op)(pval mval, pval nval)
)  
   {  mcxbool update_small = FALSE
   ;  const mclv* s, *l
   ;  const mclp* l_ofs, *s_ofs, *s_max
   ;  int n_zeroed = 0

   ;  if (v1->n_ivps < v2->n_ivps)
      {  l = v2
      ;  s = v1
      ;  update_small = TRUE    /* updates have to be made in s, not l */
   ;  }
      else
      {  l = v1
      ;  s = v2
   ;  }

      if (!l->n_ivps || !s->n_ivps)
      return 0

            /* search for largest applicable start in l;
             * if successful then
             *    l->ivps[k].idx <= s->ivps[0].idx
            */
   ;  if ((l_ofs = mclvGetIvpFloor(l, s->ivps[0].idx, NULL)))
      s_ofs = s->ivps+0

            /* search for smallest applicable start in s
             * if successful then
             *    s->ivps[k].idx >= l->ivps[0].idx
             * or
             *    l->ivps[0].idx <= s->ivps[k].idx
            */
   ;  else if ((s_ofs = mclvGetIvpCeil(s, l->ivps[0].idx, NULL)))
      l_ofs = l->ivps+0
   ;  else
      return 0

   ;  s_max = s->ivps + s->n_ivps

            /*
             * pre-condition: s_ofs < s_max.
             * invariant: l_ofs->idx <= s_ofs->idx
            */
   ;  while (l_ofs)
      {  if (s_ofs->idx == l_ofs->idx)
         {  if
            (  update_small
            && !(((mclp*) s_ofs)->val = op(s_ofs->val, l_ofs->val))
            )
            n_zeroed++
         ;  else if ( !(((mclp*) l_ofs)->val = op(l_ofs->val, s_ofs->val)))
            n_zeroed++
      ;  }
         if (++s_ofs >= s_max)
         break
      ;  l_ofs = mclvGetIvpFloor(l, s_ofs->idx, l_ofs)
   ;  }
      return n_zeroed
;  }


static int update_zip
(  mclVector*  v1
,  const mclVector*  v2
,  double  (*op)(pval mval, pval nval)
)
   {  mclp* ivp1 = v1->ivps, *ivp2 = v2->ivps
         ,  *ivp1max = ivp1 + v1->n_ivps
         ,  *ivp2max = ivp2 + v2->n_ivps

   ;  int n_zeroed = 0

   ;  while (ivp1 < ivp1max && ivp2 < ivp2max)
      {  if (ivp1->idx < ivp2->idx)
         ivp1++
      ;  else if (ivp1->idx > ivp2->idx)
         ivp2++
      ;  else if (ivp1->val = op(ivp1->val, ivp2++->val), !ivp1++->val)
         n_zeroed++
   ;  }
      return n_zeroed
;  }


static int update_canonical
(  mclVector*  v1
,  const mclVector*  v2
,  double  (*op)(pval mval, pval nval)
)
   {  int i = 0
   ;  long maxidx = v1->n_ivps-1
   ;  int n_zeroed = 0

   ;  for (i=0;i<v2->n_ivps;i++)
      {  long idx = v2->ivps[i].idx
      ;  if (idx > maxidx)
         break
      ;  v1->ivps[idx].val = op(v1->ivps[idx].val, v2->ivps[i].val)
      ;  if (!v1->ivps[idx].val)
         n_zeroed++
   ;  }
      return n_zeroed
;  }


int mclvUpdateMeet
(  mclVector*  v1
,  const mclVector*  v2
,  double  (*op)(pval mval, pval nval)
)  
   {  if (MCLV_IS_CANONICAL(v1))
      return update_canonical(v1, v2, op)
   ;  else if
      (  v2->n_ivps * log(v1->n_ivps) < v1->n_ivps
      || v1->n_ivps * log(v2->n_ivps) < v2->n_ivps
      )
      return update_small_large(v1, v2, op)
   ;  else
      return update_zip(v1, v2, op)
;  }



mclVector* mclvBinary
(  const mclVector*  vec1
,  const mclVector*  vec2
,  mclVector*        dst
,  double           (*op)(pval arg1, pval arg2)
)  
   {  mclIvp  *ivp1, *ivp2, *ivp1max, *ivp2max, *ivpk, *ivpl
   ;  long n1n2 = vec1->n_ivps+vec2->n_ivps

   ;  MCLV_CHECK(vec1, "mclvBinary lft")
   ;  MCLV_CHECK(vec2, "mclvBinary rgt")

   ;  if (vec1->n_ivps + vec2->n_ivps == 0)
      return mclvInstantiate(dst, 0, NULL)

   ;  ivpl  =  ivpk 
            =  mcxAlloc
               (  n1n2 * sizeof(mclIvp)
               ,  RETURN_ON_FAIL
               )
   ;  if (!ivpk)
      {  mcxMemDenied(stderr, "mclvBinary", "mclIvp", n1n2)
      ;  return NULL
   ;  }

      ivp1     =  vec1->ivps
   ;  ivp2     =  vec2->ivps

   ;  ivp1max  =  ivp1 + vec1->n_ivps
   ;  ivp2max  =  ivp2 + vec2->n_ivps
   ;
      
      {  double rval

      ;  while (ivp1 < ivp1max && ivp2 < ivp2max)
         {  pval val1 =  0.0
         ;  pval val2 =  0.0
         ;  long idx

         ;  if (ivp1->idx < ivp2->idx)
            {  idx   =  ivp1->idx
            ;  val1  =  (ivp1++)->val
         ;  }
            else if (ivp1->idx > ivp2->idx)
            {  idx   =  ivp2->idx
            ;  val2  =  (ivp2++)->val
         ;  }
            else
            {  idx   =  ivp1->idx
            ;  val1  =  (ivp1++)->val
            ;  val2  =  (ivp2++)->val
         ;  }

            if ((rval = op(val1, val2)) != 0.0)
            {  ivpl->idx      =  idx
            ;  (ivpl++)->val  =  rval
         ;  }
         }

         while (ivp1 < ivp1max)
         {  if ((rval = op(ivp1->val, 0.0)) != 0.0)
            {  ivpl->idx      =  ivp1->idx
            ;  (ivpl++)->val  =  rval
         ;  }
            ivp1++
      ;  }

         while (ivp2 < ivp2max)
         {  if ((rval = op(0.0, ivp2->val)) != 0.0)
            {  ivpl->idx      =  ivp2->idx
            ;  (ivpl++)->val  =  rval
         ;  }
            ivp2++
      ;  }
      }

      dst = mclvInstantiate(dst, ivpl-ivpk, ivpk)
   ;  mcxFree(ivpk)
   ;  return dst
;  }


mclVector* mclvMap
(  mclVector*  dst
,  long        mul
,  long        shift
,  mclVector*  src
)
   {  mclIvp*  ivp, *ivpmax

  /*  fixme: add error checking, overflow, sign */

   ;  if (!dst)
      dst = mclvCopy(NULL, src)
   ;  else if (src != dst)
      mclvInstantiate(dst, src->n_ivps, src->ivps)

   ;  ivp = dst->ivps
   ;  ivpmax = ivp + dst->n_ivps

   ;  while (ivp < ivpmax)
      {  ivp->idx = mul*ivp->idx + shift
      ;  ivp++
   ;  }

      return dst         
;  }



mclVector* mclvCanonical
(  mclVector* dst_vec
,  int        nr
,  double     val
)  
   {  mclIvp* ivp
   ;  int i  =  0
   ;  dst_vec  =  mclvResize(dst_vec, nr) 

   ;  ivp = dst_vec->ivps

   ;  while (ivp < dst_vec->ivps+dst_vec->n_ivps)
      {  ivp->idx =  i++
      ;  (ivp++)->val =  val
   ;  }
      return dst_vec
;  }


void mclvScale
(  mclVector*  vec
,  double      fac
)  
   {  int      n_ivps   =  vec->n_ivps
   ;  mclIvp*  ivps     =  vec->ivps

   ;  if (!fac)
      mcxErr("mclvScale PBD", "zero")

   ;  while (--n_ivps >= 0)
      (ivps++)->val /= fac
;  }


double mclvNormalize
(  mclVector*  vec
)  
   {  int      vecsize  = vec->n_ivps
   ;  mclIvp*  vecivps  = vec->ivps
   ;  double   sum      = mclvSum(vec)

   ;  vec->val =  sum

   ;  if (sum == 0.0)
      {  mcxErr
         (  "mclvNormalize"
         ,  "warning: zero sum <%f> for vector <%ld>"
         ,  (double) sum
         ,  (long) vec->vid
         )
      ;  return 0.0
   ;  }
      else if (sum < 0.0)
      mcxErr("mclvNormalize", "warning: negative sum <%f>", (double) sum)

   ;  while (--vecsize >= 0)
      (vecivps++)->val /= sum
   ;  return sum
;  }


double mclvInflate
(  mclVector*  vec
,  double      power
)  
   {  mclIvp*  vecivps
   ;  int      vecsize
   ;  double   powsum   =  0.0

   ;  MCLV_CHECK(vec, "mclvInflate")

   ;  if (!vec->n_ivps)
      return 0.0

   ;  vecivps  =  vec->ivps
   ;  vecsize  =  vec->n_ivps

   ;  while (vecsize-- > 0)
      {  (vecivps)->val = pow((double) (vecivps)->val, power)
      ;  powsum += (vecivps++)->val
   ;  }

     /* fixme static interface */
      if (mclWarningImpala && powsum <= 0.0)
      {  mcxErr("mclvInflate", "warning: nonpositive sum <%f>", (double) powsum)
      ;  mclvResize(vec, 0)
      ;  return 0.0
   ;  }

      vecivps = vec->ivps
   ;  vecsize = vec->n_ivps
   ;  while (vecsize-- > 0)
      (vecivps++)->val /= powsum

   ;  return pow((double) powsum, power > 1.0 ? 1/(power-1) : 1.0)
;  }


double mclvSize
(  const mclVector*   vec
)
   {  return vec ? vec->n_ivps : 0.0
;  }


double mclvSelf
(  const mclVector* vec
)  
   {  return vec ? mclvIdxVal(vec, vec->vid, NULL) :  0.0
;  }


double mclvSum
(  const mclVector* vec
)  
   {  double   sum      =  0.0
   ;  mclIvp*  vecivps  =  vec->ivps
   ;  int      vecsize  =  vec->n_ivps

   ;  MCLV_CHECK(vec, "mclvSum")

   ;  while (--vecsize >= 0)
      {  sum += vecivps->val
      ;  vecivps++  
   ;  }
      return sum
;  }


double mclvPowSum
(  const mclVector* vec
,  double power
)  
   {  mclIvp* vecivps = vec->ivps
   ;  int     vecsize = vec->n_ivps
   ;  double  powsum  = 0.0

   ;  while (vecsize-- > 0)
      powsum += (float) pow((double) (vecivps++)->val, power)
   ;  return powsum
;  }


double mclvNorm
(  const mclVector*        vec
,  double                  power
)  
   {  if(power > 0.0)
         mcxErr("mclvNorm", "pbd: negative power argument <%f>", (double) power)
      ,  mcxExit(1)

   ;  return pow((double) mclvPowSum(vec, power), 1.0 / power)
;  }


int mclvGetIvpOffset
(  const mclVector*  vec
,  long        idx
,  long        offset
)
   {  mclIvp*  match    =  mclvGetIvp
                           (  vec
                           ,  idx
                           ,  offset >= 0
                              ?  vec->ivps + offset
                              :  vec->ivps
                           )
   ;  return match ? match - vec->ivps : -1
;  }


mclIvp* mclvGetIvpCeil
(  const mclVector*  vec
,  long              idx
,  const mclIvp*     offset
)
   {  mclIvp sought
   ;  const mclIvp  *base  =  offset ? offset : vec->ivps
   ;  int      n_ivps   =  vec->n_ivps - (base - vec->ivps)
   ;  mclpInstantiate(&sought, idx, 1.0)

   ;  return
      mcxBsearchCeil(&sought, base, n_ivps, sizeof(mclp), mclpIdxCmp)
;  }


mclIvp* mclvGetIvpFloor
(  const mclVector*  vec
,  long              idx
,  const mclIvp*     offset
)
   {  mclIvp   sought
   ;  const mclIvp   *base    =  offset ? offset : vec->ivps
   ;  int      n_ivps   =  vec->n_ivps - (base - vec->ivps)
   ;  mclpInstantiate(&sought, idx, 1.0)
   ;  return
      mcxBsearchFloor(&sought, base, n_ivps, sizeof(mclp), mclpIdxCmp)
;  }


mclIvp* mclvGetIvp
(  const mclVector*  vec
,  long              idx
,  const mclIvp*     offset
)  
   {  mclIvp   sought
   ;  const mclIvp   *base    =  offset ? offset : vec->ivps
   ;  int      n_ivps   =  vec->n_ivps - (base - vec->ivps)

   ;  mclpInstantiate(&sought, idx, 1.0)

   ;  return
         (vec->n_ivps)
      ?  bsearch(&sought, base, n_ivps, sizeof(mclIvp), mclpIdxCmp)
      :  NULL
;  }


double mclvIdxVal
(  const mclVector*  vec
,  long        idx
,  int*        p_offset
)  
   {  int     offset   =  mclvGetIvpOffset(vec, idx, -1)
   ;  double  value    =  0.0

   ;  if (p_offset)
      *p_offset = offset
      
   ;  if (offset >= 0)
      value = (vec->ivps+offset)->val

   ;  return value
;  }


mclVector* mclvInsertIdx
(  mclVector*  vec
,  long        idx
,  double      val
)  
   {  int   offset
   
   ;  if (!vec)
      {  vec = mclvInstantiate(NULL, 1, NULL)
      ;  mclpInstantiate(vec->ivps+0, idx, val)
   ;  }
      else if ((offset =  mclvGetIvpOffset(vec, idx, -1)) >= 0)
      vec->ivps[offset].val = val
   ;  else
      {  long i = vec->n_ivps
      ;  mclvResize(vec, i+1)
      ;  while (i && vec->ivps[i-1].idx > idx)
            vec->ivps[i] = vec->ivps[i-1]
         ,  i--
      ;  vec->ivps[i].val = val
      ;  vec->ivps[i].idx = idx
   ;  }
      return vec
;  }


void mclvRemoveIdx
(  mclVector*  vec
,  long        idx
)  
   {  int   offset   =  mclvGetIvpOffset(vec, idx, -1)
                     /* check for nonnull vector is done in mclvIdxVal */
   ;  if (offset >= 0)
      {  memmove
         (  vec->ivps + offset
         ,  vec->ivps + offset + 1
         ,  (vec->n_ivps - offset - 1) * sizeof(mclIvp)
         )
      ;  mclvResize(vec, vec->n_ivps - 1)
   ;  }
   }


int mclvVidCmp
(  const void*  p1
,  const void*  p2
)
   {  long diff = ((mclVector*) p1)->vid - ((mclVector*)p2)->vid
   ;  return SIGN(diff)
;  }


int mclvSizeRevCmp
(  const void*  p1
,  const void*  p2
)
   {  long diff  = ((mclVector*)p2)->n_ivps - ((mclVector*)p1)->n_ivps
   ;  if (diff)
      return SIGN(diff)
   ;  else
      return mclvLexCmp(p1, p2)
;  }


int mclvSizeCmp
(  const void*  p1
,  const void*  p2
)
   {  return mclvSizeRevCmp(p2, p1)
;  }


int mclvLexCmp
(  const void*  p1
,  const void*  p2
)
   {  mclIvp*   ivp1    =  ((mclVector*)p1)->ivps
   ;  mclIvp*   ivp2    =  ((mclVector*)p2)->ivps
   ;  long      diff
   ;  int       n_ivps  =  MIN
                           (  ((mclVector*)p1)->n_ivps
                           ,  ((mclVector*)p2)->n_ivps
                           )
  /*
   *  Vectors with low numbers first
  */
   ;  while (--n_ivps >= 0)
      if ((diff = (ivp1++)->idx - (ivp2++)->idx))
      return SIGN(diff)

   ;  diff = ((mclVector*)p1)->n_ivps - ((mclVector*)p2)->n_ivps
   ; return SIGN(diff)
;  }


int mclvSumCmp
(  const void* p1
,  const void* p2
)  
   {  double diff = mclvSum((mclVector*) p1) - mclvSum((mclVector*) p2)
   ;  return SIGN(diff)
;  }


int mclvSumRevCmp
(  const void* p1
,  const void* p2
)  
   {  return mclvSumCmp(p2, p1)
;  }


void mclvSelectHighest
(  mclVector*  vec
,  int         max_n_ivps
)  
   {  double f
      =  vec->n_ivps <= max_n_ivps
         ?  0.0
         :  vec->n_ivps >= 2 * max_n_ivps
            ?  mclvKBar
               (vec, max_n_ivps, PVAL_MAX, KBAR_SELECT_LARGE)
            :  mclvKBar
               (vec, vec->n_ivps - max_n_ivps + 1, -PVAL_MAX, KBAR_SELECT_SMALL)

   ;  mclvSelectGqBar(vec, f)
;  }


void mclvZeroValues
(  mclVector*  vec
)  
   {  mclIvp*  ivp   =  vec->ivps
   ;  mclIvp*  maxivp=  ivp + vec->n_ivps

   ;  while (ivp<maxivp)
      (ivp++)->val = 0.0
;  }


void mclvMakeConstant
(  mclVector*  vec
,  double      val
)  
   {  if (val == 0.0) 
      mclvZeroValues(vec)
   ;  else
      mclvUnary(vec, fltxConst, &val)
;  }


void mclvMakeCharacteristic
(  mclVector* vec
)  
   {  double one = 1.0
   ;  mclvUnary(vec, fltxConst, &one)
;  }


void mclvHdp
(  mclVector* vec
,  double power
)  
   {  mclvUnary(vec, fltxPower, &power)
;  }


mclVector* mclvAdd
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*  dst
)  
   {  return mclvBinary(lft, rgt, dst, fltAdd)
;  }


mclVector* mclvMaskedCopy
(  mclVector*        dst
,  const mclVector*  src
,  const mclVector*  msk
,  int               mask_mode
)  
   {  if   (mask_mode == 0)                              /* positive mask */
      return  mclvBinary(src, msk, dst, fltLaR)
   ;  else                                               /* negative mask */
      return mclvBinary(src, msk, dst, fltLaNR)
;  }


mclVector* mcldMerge
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*  dst
)
   {  return mclvBinary(lft, rgt, dst, fltLoR)
;  }


mclVector* mcldMinus
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*  dst
)  
   {  if (lft == dst)
      {  if (mclvUpdateMeet(dst, rgt, fltLaNR))
         mclvUnary(dst, fltxCopy, NULL)
      ;  return dst
   ;  }
      return mclvBinary(lft, rgt, dst, fltLaNR)
;  }


mclVector* mcldMeet
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*  dst
)  
   {  return mclvBinary(lft, rgt, dst, fltLaR)
;  }


mcxbool mcldEquate
(  const mclVector* dom1
,  const mclVector* dom2
,  mcxenum    mode
)
   {  int meet, ldif, rdif
   ;  mcldCountParts(dom1, dom2, &ldif, &meet, &rdif)

   ;  switch(mode)
      {  case MCLD_EQ_SUPER
      :  return rdif == 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_SUB
      :  return ldif == 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_EQUAL
      :  return ldif + rdif == 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_DISJOINT
      :  return meet == 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_TRISPHERE
      :  return ldif != 0 && rdif != 0 && meet != 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_LDIFF
      :  return ldif != 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_MEET
      :  return meet != 0 ? TRUE : FALSE
      ;

         case MCLD_EQ_RDIFF
      :  return rdif != 0 ? TRUE : FALSE
      ;

         default
      :  mcxErr("mcldEquate PBD", "unknown mode <%d>", (int) mode)
      ;  mcxExit(1)
   ;  }
      return TRUE
;  }


int mcldCountSet
(  const mclVector*  dom1
,  const mclVector*  dom2
,  mcxbits     parts
)
   {  int meet, ldif, rdif, count = 0
   ;  mcldCountParts(dom1, dom2, &ldif, &meet, &rdif)
   ;  if (parts & MCLD_CT_LDIFF)
      count += ldif
   ;  if (parts & MCLD_CT_MEET)
      count += meet
   ;  if (parts & MCLD_CT_RDIFF)
      count += rdif
   ;  return count
;  }


void mcldCountParts
(  const mclVector* dom1
,  const mclVector* dom2
,  int*       ld
,  int*       mt
,  int*       rd
)
   {  mclIvp   *ivp1, *ivp2, *ivp1max, *ivp2max
   ;  int meet = 0, ldif = 0, rdif = 0

   ;  ivp1     =  dom1->ivps
   ;  ivp2     =  dom2->ivps

   ;  ivp1max  =  ivp1 + dom1->n_ivps
   ;  ivp2max  =  ivp2 + dom2->n_ivps

   ;  while (ivp1 < ivp1max && ivp2 < ivp2max)
      {  if (ivp1->idx < ivp2->idx)
            ldif++
         ,  ivp1++
      ;  else if (ivp1->idx > ivp2->idx)
            rdif++
         ,  ivp2++
      ;  else
            meet++
         ,  ivp1++
         ,  ivp2++
   ;  }

      ldif += ivp1max - ivp1
   ;  rdif += ivp2max - ivp2
   ;  if (ld)
      *ld = ldif
   ;  if (rd)
      *rd = rdif
   ;  if (mt)
      *mt = meet
;  }


mclv* mclvFromIvps
(  mclv* dst
,  mclp* ivps
,  int n_ivps
)
   {  return mclvFromIvps_x
      (  dst, ivps, n_ivps, 0, 0, mclpMergeLeft, NULL )
      /* second 0 argument: sortbits, 1 = sorted, 2 = no duplicates */
;  }


/* current dst content is thrown away if fltbinary not used */
mclv* mclvFromIvps_x
(  mclv* dst
,  mclp* ivps
,  int n_ivps
,  mcxbits sortbits
,  mcxbits warnbits
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
,  double (*fltbinary)(pval val1, pval val2)
)
   {  mcxbool warn_re = warnbits & MCLV_WARN_REPEAT_ENTRIES
   ;  mcxbool warn_rv = warnbits & MCLV_WARN_REPEAT_VECTORS
   ;  int n_old = dst ? dst->n_ivps : 0
   ;  const char* me = "mclvFromIvps_x"
   ;  int n_re = 0, n_rv = 0
   ;  if (!dst)
      dst = mclvInit(NULL)

   ;  if (n_ivps)
      {  if (dst->n_ivps && fltbinary)
         {  mclVector* tmpvec = mclvNew(ivps, n_ivps)

         ;  if (!(sortbits & 1))
            mclvSort(tmpvec, NULL)

         ;  if (!(sortbits & 2))
            n_re = mclvUniqueIdx(tmpvec, ivpmerge)

         ;  n_rv += tmpvec->n_ivps
         ;  n_rv += dst->n_ivps
         ;  mclvBinary(dst, tmpvec, dst, fltbinary)
         ;  n_rv -= dst->n_ivps

         ;  mclvFree(&tmpvec)
      ;  }
         else
         {  if (dst->ivps == ivps)
            mcxErr("mlcvFromIvps_x", "DANGER dst->ivps == ivps")

         ;  mclvRenew(dst, ivps, n_ivps)

         ;  if (!(sortbits & 1))
            mclvSort(dst, NULL)

         ;  if (!(sortbits & 2))
            n_re += mclvUniqueIdx(dst, ivpmerge)
      ;  }
      }

      if (warn_re && n_re)
      mcxErr
      (  me
      ,  "<%ld> found <%ld> repeated entries within %svector"
      ,  (long) dst->vid
      ,  (long) n_re
      ,  n_rv ? "repeated " : ""
      )

   ;  if (warn_rv && n_rv)
      mcxErr
      (  me
      ,  "<%ld> new vector has <%ld> overlap with previous amalgam"
      ,  (long) dst->vid
      ,  (long) n_rv
      )

   ;  if (warnbits && n_re + n_rv)
      mcxErr
      (  me
      ,  "<%ld> vector went from <%ld> to <%ld> entries"
      ,  (long) dst->vid
      ,  (long) n_old
      ,  (long) dst->n_ivps
      )
   ;  return dst
;  }


mcxbool mcldIsCanonical
(  mclVector* vec
)
   {  int n = vec->n_ivps
   ;  if (!n)
      return TRUE
   ;  if (vec->ivps[0].idx == 0 && vec->ivps[n-1].idx == n-1)
      return TRUE
   ;  return FALSE
;  }


long mclvHighestIdx
(  mclVector*  vec
)
   {  int n = vec->n_ivps
   ;  if (!n)
      return -1
   ;  return vec->ivps[n-1].idx
;  }


double mclvMaxValue
(  const mclVector*           vec
)  
   {  double max_val = -FLT_MAX
   ;  mclIvp* ivp    = vec->ivps  
   ;  mclIvp* ivpmax = ivp + vec->n_ivps
   ;  while (ivp<ivpmax)
      {  if (ivp->val > max_val)
         max_val = ivp->val
      ;  ivp++
   ;  }
      return  max_val
;  }


long mclvUnaryList
(  mclv*    vec
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)
   {  int      n_ivps
   ;  mclIvp   *src_ivp, *dst_ivp

   ;  MCLV_CHECK(vec, "mclvUnary")

   ;  n_ivps   =  vec->n_ivps
   ;  src_ivp  =  vec->ivps
   ;  dst_ivp  =  vec->ivps
   
   ;  while (--n_ivps >= 0)
      {  double val =  mclpUnary(src_ivp, ar)

      ;  if (val != 0.0)
         {  dst_ivp->idx =  src_ivp->idx
         ;  dst_ivp->val =  val
         ;  dst_ivp++
      ;  }
         src_ivp++
   ;  }

      mclvResize(vec, dst_ivp - vec->ivps)
   ;  return vec->n_ivps
;  }

