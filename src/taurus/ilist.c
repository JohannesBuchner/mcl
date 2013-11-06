/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO remove exit, create single malloc point */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "ilist.h"
#include "perm.h"

#include "util/compile.h"
#include "util/array.h"
#include "util/alloc.h"
#include "util/types.h"
#include "util/equate.h"
#include "util/err.h"
#include "util/io.h"


int ilWriteFile
(  const mcxIL*   src_il
,  FILE*          f_out
)
   {  int   k  =  fwrite(&(src_il->n), sizeof(int), 1, f_out)
   ;  k       +=  fwrite(src_il->L, sizeof(int), src_il->n, f_out)
   ;  if (k != src_il->n + 1)
      {  mcxErr
         (  "ilWriteFile"
         ,  "PANIC wrote %d, should be %d"
         ,  k
         ,  src_il->n + 1
         )
      ;  return 1
   ;  }
      return 0
;  }


mcxIL*   ilVA
(  mcxIL*   il
,  int      k
,  ...
)
   {  va_list     ap
   ;  mcxBuf      ibuf
   ;  int         i

   ;  if (!il)
      il    =  ilInit(NULL)

   ;  if (mcxBufInit(&ibuf, &(il->L), sizeof(int), 8))
      {  ilFree(&il)
      ;  return NULL
   ;  }

      {  va_start(ap, k)

      ;  for (i=0;i<k;i++)
         {  int  *xp =  mcxBufExtend(&ibuf, 1, RETURN_ON_FAIL)
         ;  if (!xp)
            {  ilFree(&il)
            ;  return NULL
         ;  }
            *xp      =  va_arg(ap, int)
      ;  }
         va_end(ap)
   ;  }

      il->n =  mcxBufFinalize(&ibuf)
   ;  return(il)
;  }


mcxIL*   ilComplete
(  mcxIL*  il
,  int     n
)
   {  int   i

   ;  il = ilInstantiate(il, n, NULL)

   ;  for (i=0;i<n;i++)
      *(il->L+i)  =  i

   ;  return(il)
;  }


/* fixme, should release mem for existing il */

mcxIL*   ilInit
(  mcxIL*   il
)
   {  if
      (  !il
      && (!(il = (mcxIL *) mcxAlloc(sizeof(mcxIL), RETURN_ON_FAIL)))
      )
      return NULL

   ;  il->L =  NULL
   ;  il->n    =  0

   ;  return il
;  }


mcxIL* ilInstantiate
(  mcxIL*   il
,  int      n
,  int*     eyes
)
   {  int* L_new = NULL
   ;  int  n_old = 0

   ;  if (n < 0)
      {  errno = EINVAL
      ;  return il
   ;  }

      if
      (  !il
      && (!(il = ilInit(NULL)))
      )
      return NULL

   ;  n_old = il->n

   ;  if
      (! ( L_new
         =  mcxRealloc
            (  il->L
            ,  n * sizeof(int)
            ,  RETURN_ON_FAIL
            )
         )
         && n
      )
      return il
   ;  else
      il->L = L_new

   ;  if (n && eyes)
      memcpy(il->L, eyes, n * sizeof(int))
   ;  else
      {  int i = 0
      ;  while (i < n)
         *(il->L+i++) = 0
   ;  }

      il->n = n
   ;  return il
;  }


mcxIL* ilCon
(  mcxIL*   dst
,  int*     ls
,  int      n
)
   {  if (!dst)
      return ilNew(n, ls)

   ;  if (!ilInstantiate(dst, dst->n + n, ls))
      return NULL

   ;  memcpy((dst->L)+dst->n, ls, n * sizeof(int))
   ;  dst->n  += n

   ;  return dst
;  }


mcxIL* ilResize
(  mcxIL*   il
,  int      n
)
   {  return ilInstantiate(il, n, NULL)
;  }


mcxIL*   ilInvert
(  mcxIL*   src
)
   {  int      i
   ;  mcxIL*   inv

   ;  inv = ilNew(src->n, NULL)

   ;  for (i=0;i<src->n;i++)
      {  int  next = *(src->L+i)
      ;  if (next < 0 || next >= src->n)
         {  mcxErr
            (  "ilInvert"
            ,  "index %d out of range (0, %d>"
            ,  next
            ,  src->n
            )
         ;  ilFree(&inv)
         ;  return NULL
      ;  }
         *(inv->L+next) =  i
   ;  }
      return inv
;  }


int   ilIsMonotone
(  mcxIL*   src
,  int      gradient
,  int      min_diff
)
   {  int   i

   ;  for(i=1;i<src->n;i++)
      {  if ( (*(src->L+i) - *(src->L+i-1)) * gradient < min_diff)
            return 0
   ;  }
   ;  return 1
;  }


int   ilIsOneOne
(  mcxIL*   src
)
   {  int   d
   ;  mcxIL*   inv   =  ilInvert(src)
   ;  mcxIL*   invv  =  ilInvert(inv)
   ;  d = intnCmp(src->L, invv->L, src->n)
   ;  ilFree(&inv)
   ;  ilFree(&invv)
   ;  return d ? 0 : 1
;  }


void   ilAccumulate
(  mcxIL*   il
)  {  int   i     =  0
   ;  int   prev  =  0

   ;  for (i=0;i<il->n;i++)
      {  *(il->L+i) +=  prev
      ;  prev           =  *(il->L+i)
   ;  }
;  }
      

int ilSum
(  mcxIL*   il
)
   {  int   sum   =  0
   ;  int   i

   ;  i          =  il->n
   ;  while(--i >= 0)
      {  sum += *(il->L+i)
   ;  }
   ;  return sum
;  }


void ilPrint
(  mcxIL*   il
,  const char msg[]
)
   {  int      i

   ;  for (i=0;i<il->n;i++)
      {  printf(" %-8d", *(il->L+i))
      ;  if (((i+1) % 8) == 0)
            printf("\n")
   ;  }
   ;  if (i%8 != 0) printf("\n")
   ;  fprintf
      (  stdout, "[ilPrint end%s%s: size is %d]\n"
      ,  msg[0] ? ":" : ""
      ,  msg
      ,  il->n
      )
   ;  fprintf(stdout, " size is %d\n\n", il->n)
;  }

                                    /* translate an integer sequence    */
                                    /* should be generalized via Unary  */
void     ilTranslate
(  mcxIL*   il
,  int      dist
)
   {  int   i
   ;  i  = il->n
   ;  if (!dist) return
   ;  while (--i >= 0)
      {  *(il->L+i) += dist
   ;  }
;  }


void ilFree
(  mcxIL**  ilp
)
   {  if (*ilp)
      {  if ((*ilp)->L)  mcxFree((*ilp)->L)
      ;  mcxFree(*ilp)
   ;  }
   ;  *ilp  =  (void*) 0
;  }

                              /* shuffle an interval of integers        */

mcxIL*     ilRandPermutation
(  int      lb
,  int      rb
)
   {  int      w        =     rb - lb
   ;  int      *ip, i
   ;  mcxIL*   il

   ;  if (w < 0)
      {  mcxErr  
         (  "ilRandPermutation error"
         ,  "bounds [%d, %d] reversed"
         ,  lb
         ,  rb
         )
      ;  return NULL
   ;  }
      else if (w == 0)
      mcxErr  
      (  "ilRandPermutation"
      ,  "WARNING bounds [%d, %d] equal"
      ,  lb
      ,  rb
      )

   ;  if (!(il = ilComplete(NULL, w)))
      return NULL

   ;  ip = il->L

   ;  for (i=w-1;i>=0;i--)                /* shuffle interval               */
      {  int l       =  (int) (rand() % (i+1))
      ;  int draw    =  *(ip+l)
      ;  *(ip+l)     =  *(ip+i)
      ;  *(ip+i)     =  draw
   ;  }

      ilTranslate(il, lb)        /* translate interval to correct offset   */
   ;  return il
;  }


/*
 *   currently not in use.
 *   note the solution in genMatrix:
 *   draw for each number separately.
*/


mcxIL* ilLottery
(  int         lb
,  int         rb
,  float       p
,  int         times
)
   {  int      i
   ;  int      w        =  rb - lb
   ;  int      hits
   ;  long     prev, bar, r

   ;  mcxIL*   il  = ilNew(0, NULL)
   ;  mcxBuf   ibuf

   ;  if (!il)
      return NULL

   ;  if (w <= 0 || rb < 1)
      {  mcxErr
         (  "ilDraw warning"
         ,  "interval [%d, %d> ill defined"
         ,  lb
         ,  rb
         )
      ;  ilFree(&il)
      ;  return NULL
   ;  }

      if (p < 0.0) p = 0.0
   ;  if (p > 1.0) p = 1.0

      /* fixme etc */
   ;  if (mcxBufInit(&ibuf, &(il->L), sizeof(int), times))
      {  ilFree(&il)
      ;  return NULL
   ;  }

      hits =  0
   ;  prev =  rand()
   ;  bar  =  p * LONG_MAX
   
   ;  for (i=0;i<times;i++)
      {  if ((r = rand() % INT_MAX) < bar)
         {  int*  jptr  =  mcxBufExtend(&ibuf, 1, RETURN_ON_FAIL)
         ;  if (!jptr)
            {  ilFree(&il)
            ;  return NULL
         ;  }
            *jptr       =  lb + ((r + prev) % w)
      ;  }
         prev = r % w
   ;  }

      il->n = mcxBufFinalize(&ibuf)
   ;  return il
;  }


                           /* create random partitions at grid--level   */
mcxIL*  ilGridRandPartitionSizes
(  int      w
,  int      gridsize
)
   {  int      n_blocks, i
   ;  mcxIL*   il_all   =     (void*) 0
   ;  mcxIL*   il_one   =     (void*) 0


   ;  if (gridsize > w)
         return ilRandPartitionSizes(w)

   ;  n_blocks  =  w / gridsize
   ;  il_all    =  ilInit(NULL)

   ;  for (i=0;i<n_blocks;i++)
      {  il_one         =     ilRandPartitionSizes(gridsize)
      ;  ilCon(il_all, il_one->L, il_one->n)
      ;  ilFree(&il_one)
   ;  }

   ;  if (n_blocks * gridsize < w)
      {  il_one         =     ilRandPartitionSizes(w - n_blocks * gridsize)
      ;  ilCon(il_all, il_one->L, il_one->n)
      ;  ilFree(&il_one)
   ;  }

   ;  ilSort(il_all)
   ;  return il_all
;  }


                                    /* create random partition */
mcxIL* ilRandPartitionSizes
(  int      w
)
   {  Pmt*     pmt
   ;  mcxIL*   il

   ;  if (w <= 0)
      {  mcxErr
         (  "ilRandPartition"
         ,  "WARNING width argument %d nonpositive"
         ,  w
         )
      ;  return ilInit(NULL)
   ;  }

      pmt = pmtRand(w)
   ;  il  = pmtGetCycleSizes(pmt)
   ;  pmtFree(&pmt)
   ;  ilSort(il)

   ;  return il
;  }


float ilDeviation
(  mcxIL*   il
)
   {  float    dev   =  0.0
   ;  float    av    =  ilAverage(il)
   ;  int   i
   ;  if (il->n == 0)
         return 0.0

   ;  for (i=0;i<il->n;i++)
      {  dev += ((float ) (*(il->L+i)-av)) * (*(il->L+i)-av)
   ;  }
   ;  return sqrt(dev/il->n)
;  }


float ilAverage
(  mcxIL*   il
)
   {  float    d  =  (float ) ilSum(il)
   ;  return il->n ? d /  il->n : 0.0
;  }


float ilCenter
(  mcxIL*   il
)
   {  int   sum   =  ilSum(il)
   ;  return   sum ? (float ) ilSqum (il) / (float) sum : 0.0 
;  }


int ilSqum
(  mcxIL*   il
)
   {  int   sum   =  0
   ;  int   i     =  il->n
   ;  while(--i >= 0)
      {  sum += *(il->L+i) * *(il->L+i)
   ;  }
   ;  return sum
;  }


int ilSelectRltBar
(  mcxIL*   il
,  int      i1
,  int      i2
,  int      (*rlt1)(const void*, const void*)
,  int      (*rlt2)(const void*, const void*)
,  int      onlyCount
)
   {  int   i
   ;  int   j     =  0

   ;  for(i=0;i<il->n;i++)
      {  if
         (  (!rlt1 || rlt1(il->L+i, &i1))
         && (!rlt2 || rlt2(il->L+i, &i2))
         )
         {  if (!onlyCount && j<i)
            *(il->L+j)   =  *(il->L+i)
         ;  j++
      ;  }
      }

      if (!onlyCount && (i-j))
      ilResize(il, j)

   ;  return j
;  }


void  ilSort
(  mcxIL*   il
)
   {  if (il->L)
      qsort(il->L, il->n, sizeof(int), intCmp)
;  }


void  ilRevSort
(  mcxIL*   il
)
   {  if (il->L)
      qsort(il->L, il->n, sizeof(int), intRevCmp)
;  }


int ilSelectLtBar
(  mcxIL*   il
,  int      i
)  {  return ilSelectRltBar(il, i,  0, intLt, NULL, 0)
;  }


int   ilIsDescending
(  mcxIL*   src
)  {  return ilIsMonotone(src, -1, 1)
;  }


int   ilIsNonAscending
(  mcxIL*   src
)  {  return ilIsMonotone(src, -1, 0)
;  }


int   ilIsNonDescending
(  mcxIL*   src
)  {  return ilIsMonotone(src, 1, 0)
;  }


int   ilIsAscending
(  mcxIL*   src
)  {  return ilIsMonotone(src, 1, 1)
;  }


int ilSelectGqBar
(  mcxIL*   il
,  int      i
)  {  return ilSelectRltBar(il, i,  0, intGq, NULL, 0)
;  }


void  ilSet
(  mcxIL*   il
,  int      k
)
   {  int a
   ;  for (a=0;a<il->n;a++)
      il->L[a] = k
;  }


mcxIL* ilNew
(  int   n
,  int*  eyes
)  {  mcxIL* il =  ilInstantiate(NULL, n, eyes)
   ;  return il
;  }


int ilCountLtBar
(  mcxIL*   il
,  int      i
)  {  return ilSelectRltBar(il, i,  0, intLt, NULL, 1)
;  }


