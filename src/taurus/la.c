/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

#include "ilist.h"
#include "la.h"

#include "util/types.h"
#include "util/alloc.h"
#include "util/array.h"

#include "impala/io.h"
#include "impala/matrix.h"



int   idxShare
(  const   mclVector*   v1
,  const   mclVector*   v2
,  mclVector*           m
)
   {  m  =  mcldMeet(v1, v2, m)
   ;  if (m->n_ivps)
         return (m->ivps+0)->idx
   ;  return -1
;  }


mclVector*   mclvFromIlist
(  mclVector*  vec
,  mcxIL*      il
,  float       f
)
   {  int   i

   ;  if (!il)
      return mclvResize(vec, 0)

   ;  vec = mclvResize(vec, il->n)

   ;  for (i=0;i<il->n;i++)
      {  mclIvp*  ivp   =  vec->ivps+i
      ;  ivp->idx       =  *(il->L+i)
      ;  ivp->val       =  f
   ;  }  

      mclvSort(vec, NULL)
   ;  mclvUniqueIdx(vec, mclpMergeLeft)

   ;  return vec
;  }


mclMatrix*     genClustering
(  mcxIL*   il
)
   {  int  i, j,  prev_offset, n_seen
   ;  mclMatrix*  cl

   ;  if (!ilIsAscending(il) || *(il->L+0) <= 0)
      {  fprintf
         (  stderr
         ,  "[genMatrix] mcxIL argument not strictly ascending and positive\n"
         )
      ;  ilPrint(il, "offending list")
      ;  exit(1)
   ;  }
                                  /* one garbage vector, that will be empty */
   ;  cl             =  mclxAllocZero(NULL, NULL)
   ;  prev_offset    =  0
   ;  n_seen         =  0

   ;  for (i=0;i<il->n;i++)
      {  mclVector*  vec      =  cl->cols+i
      ;  int      vecsize  =  *(il->L+i) - prev_offset
      ;  mclvResize(vec, vecsize)

      ;  for (j=0;j< vecsize; j++)
         {  (vec->ivps+j)->idx  =   n_seen++
         ;  (vec->ivps+j)->val  =   1.0
      ;  }
      ;  prev_offset =  *(il->L+i)
   ;  }
   ;  return cl
;  }


mclMatrix*     genMatrix
(  mcxIL*   il_part
,  float    p
,  float    q
,  double   ppower
,  double   qpower
)
   {  int      dimension   =  *(il_part->L+il_part->n -1)
   ;  int      c, r
   ;  long     t
   ;  mclMatrix   *mx, *mxt, *mxs
   ;  int      c_p         =  0              /* c belongs to partition c_p */

   ;  long     pbar        =  (long) (p * LONG_MAX)
   ;  long     qbar        =  (long) (q * LONG_MAX)
   ;  int      c_clustersize, r_clustersize
   ;  float    qfactor, pfactor

   ;  mcxBuf     ivpbuf

   ;  if (!ilIsAscending(il_part) || *(il_part->L+0) <= 0)
      {  fprintf
         (  stderr
         ,  "[genMatrix] mcxIL argument not strictly ascending and positive\n"
         )
      ;  ilPrint(il_part, "offending list")
      ;  exit(1)
   ;  }

      mx                =  mclxAllocZero(NULL, NULL)

   ;  c_clustersize     =  *(il_part->L+0)
   ;  r_clustersize     =  *(il_part->L+0)
   ;  qfactor           =  qpower && (c_clustersize < dimension)
                           ?  pow   (  (double) c_clustersize
                                    /  (dimension - c_clustersize)
                                    ,  qpower
                                    )
                           :  1.0
   ;  pfactor           =  ppower && (c_clustersize < dimension)
                           ?  pow   (  (double) c_clustersize
                                    /  (dimension - c_clustersize)
                                    ,  ppower
                                    )
                           :  1.0

   ;  for (c=0;c<dimension;c++)
      {  mclVector*  vec   =  (mx->cols+c)
      ;  int      r_p      =  c_p          /* do only sub diagonal part     */

      ;  mcxBufInit(&ivpbuf, &(vec->ivps), sizeof(mclIvp), 30)

      ;  if (c >= *(il_part->L+c_p))
         {  c_p++
         ;  c_clustersize  =  *(il_part->L+c_p) - *(il_part->L+c_p-1)
         ;  qfactor        =     qpower
                              && (c_clustersize + r_clustersize < dimension)
                              ?  pow(  (double) (c_clustersize + r_clustersize)
                                             /
                                       (dimension-c_clustersize-r_clustersize)
                                    ,  qpower
                                    )
                              :  1.0
         ;  pfactor        =     ppower
                              && (c_clustersize < dimension)
                              ?  pow(  (double) c_clustersize
                                             /
                                       (dimension - c_clustersize)
                                    ,  ppower
                                    )
                              :  1.0
      ;  }

      ;  for (r=c+1;r<dimension;r++)
         {  if (r >= *(il_part->L+r_p))
            {  r_p++
            ;  r_clustersize  =  *(il_part->L+r_p) - *(il_part->L+r_p-1)
         ;  }
            t     =  rand()
         ;  if
            (  (c_p == r_p && t <= (long) (pbar / pfactor))
            || (t <= (long) (qbar * qfactor))
            )
            {  mclIvp* ivp    =  (mclIvp*) mcxBufExtend(&ivpbuf, 1, EXIT_ON_FAIL)
            ;  ivp->idx       =  r
            ;  ivp->val       =  1.0
         ;  }
         }

      ;  vec->n_ivps   =  mcxBufFinalize(&ivpbuf)
   ;  }

      mxt   =  mclxTranspose(mx)
   ;  mxs   =  mclxAdd(mx, mxt)
   ;  mclxMakeCharacteristic(mxs)
   ;  mclxFree(&mx)
   ;  mclxFree(&mxt)
   ;  return mxs
;  }


#if 0
/* if reinstated it needs to take dom_cols and dom_rows into account */
mclMatrix*     mclxPermute
(  mclMatrix*  src
,  mclMatrix*  dst
,  mcxIL*   pm_col
,  mcxIL*   pm_row
)
   {  mclVector*  cols  =  NULL
   ;  int      i
   ;  mcxIL*   pmi

   ;  int      n_col    =  N_COLS(src)
   ;  int      n_row    =  N_ROWS(src)

   ;  if ((pm_col && pm_col->n != n_col) || (pm_row && pm_row->n != n_row))
      {  fprintf
         (  stderr
         ,  "[mclxPermute] mclMatrix dimensions (%d, %d)\n"
            "  do not fit permutation dimensions (%d, %d)\n"
         ,  n_col, n_row
         ,  pm_col->n, pm_row->n
         )
      ;  exit(1)
   ;  }

      if (dst == src)
   ;  else
      {  if (dst)
         {  mclxFree(&dst)
      ;  }
      ;  dst = mclxCopy(src)
   ;  }

   ;  if (pm_col)
      {  pmi      =  ilInvert(pm_col)

      ;  cols  =  (mclVector*) mcxRealloc
                     (  cols
                     ,  n_col * sizeof(mclVector)
                     ,  RETURN_ON_FAIL
                     )
      ;  if (n_col && !cols)
            mcxMemDenied(stderr, "mclxPermute", "mclVector", n_col)
         ,  exit(1)

      ;  memcpy(cols, dst->cols, n_col * sizeof(mclVector))

      ;  for (i=0;i<n_col;i++)
         *(dst->cols+i) = *(cols + *(pmi->L+i))
      ;  ilFree(&pmi)
   ;  }

      if (pm_row)
      {  mclMatrix*  dst_tp   =  mclxTranspose(dst)
      ;  pmi                  =  ilInvert(pm_row)

      ;  cols  =  (mclVector*) mcxRealloc
                     (  cols
                     ,  n_row * sizeof(mclVector)
                     ,  RETURN_ON_FAIL
                     )

      ;  if (n_row && !cols)
            mcxMemDenied(stderr, "mclxPermute", "mclVector", n_row)
         ,  exit(1)

      ;  memcpy(cols, dst_tp->cols, n_row * sizeof(mclVector))

      ;  for (i=0;i<n_row;i++)
         *(dst_tp->cols+i) = *(cols + *(pmi->L+i))

      ;  mclxFree(&dst)
      ;  dst   =  mclxTranspose(dst_tp)
      ;  mclxFree(&dst_tp)
      ;  ilFree(&pmi)
   ;  }

      mcxFree(cols)
   ;  return dst
;  }
#endif

