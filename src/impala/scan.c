/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
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
#include <limits.h>

#include "scan.h"
#include "io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/minmax.h"


void mclvScan
(  const mclVector*  vec
,  mclvScore* score
)
   {  mclvSubScan(vec, NULL, score)
;  }


void mclvSubScan
(  const mclVector*  vec
,  const mclVector*  dom
,  mclvScore* score
)
#define STAT_MEET 1
#define STAT_VDIF 2
#define STAT_DDIF 3
   {  int      n_meet   =  0
   ;  int      n_vdif   =  0
   ;  int      n_ddif   =  0
   ;  double   max_i    = -FLT_MAX
   ;  double   max_o    = -FLT_MAX
   ;  double   min_i    =  FLT_MAX
   ;  double   min_o    =  FLT_MAX
   ;  double   sum_i    =  0.0
   ;  double   sum_o    =  0.0
   ;  double   ssq_i    =  0.0
   ;  double   ssq_o    =  0.0
   ;  mclIvp*  ivp1     =  vec->ivps
   ;  mclIvp*  ivp1max  =  ivp1 + vec->n_ivps
   ;  mclIvp*  ivp2, *ivp2max

   ;  if (!dom)
      dom = vec

   ;  ivp2     =  dom->ivps
   ;  ivp2max  =  ivp2 + dom->n_ivps

   ;  while (ivp1 < ivp1max)
      {  double val  =  0.0
      ;  int i1      =  ivp1->idx  
      ;  int i2      =  ivp2 == ivp2max ? -1 : ivp2->idx  
      ;  int stat

                                                /* i1 not in domain */
      ;  if (ivp2 == ivp2max || i1 < i2)
            stat  =  STAT_VDIF
         ,  val   = (ivp1++)->val
                                                /* count, adjust */
      ;  else if (i1 > i2)
            stat  =  STAT_DDIF
         ,  ivp2++
                                                /* i1==i2, i1 in domain */
      ;  else
            stat  =  STAT_MEET
         ,  val   =  (ivp1++)->val
         ,  ivp2++


      ;  if (stat == STAT_VDIF)
         {  min_o  =  MIN(val, min_o)
         ;  max_o  =  MAX(val, max_o)
         ;  sum_o +=  val
         ;  ssq_o +=  val * val
         ;  n_vdif++
      ;  }
         else if (stat == STAT_MEET)
         {  min_i  =  MIN(val, min_i)
         ;  max_i  =  MAX(val, max_i)
         ;  sum_i +=  val
         ;  ssq_i +=  val * val
         ;  n_meet++
      ;  }
         else
         n_ddif++
   ;  }

      score->n_meet   =  n_meet
   ;  score->n_vdif   =  n_vdif
   ;  score->n_ddif   =  n_ddif + (ivp2max - ivp2)
   ;  score->max_i    =  max_i
   ;  score->min_i    =  min_i
   ;  score->sum_i    =  sum_i
   ;  score->ssq_i    =  ssq_i
   ;  score->max_o    =  max_o
   ;  score->min_o    =  min_o
   ;  score->sum_o    =  sum_o
   ;  score->ssq_o    =  ssq_o
#undef STAT_MEET
#undef STAT_VDIF
#undef STAT_VDIF
;  }




void mclxScan
(  const mclMatrix*  mx
,  mclxScore* score
)
   {  mclxSubScan(mx, NULL, NULL, score)
;  }


/* given a finished score computation (by mclvScan)
 * compute the coverage
*/

void mclvScoreCoverage
(  mclvScore* score
,  double*   cov
,  double*   covmax
)
   {  double sum = score->sum_i + score->sum_o
   ;  double ssq = score->ssq_i + score->ssq_o
   ;  double div = score->sum_i - score->sum_o
   ;  double max = MAX(score->max_i, score->max_o)

   ;  *cov = 0.0
   ;  *covmax = 0.0

   ;  if (sum * sum)
      {  double ctr  =  ssq / (sum * sum)
      ;  long n_join =  score->n_vdif+score->n_meet+score->n_ddif
      ;  long n_dom  =  n_join - score->n_vdif

      ;  max /=  sum
      ;  div /=  sum

      ;  if (ctr && n_join)
         *cov     =  1.0 - ((n_dom - (div/ctr)) /  n_join)
      ;  if (max && n_join)
         *covmax  =  1.0 - ((n_dom - (div/max)) /  n_join)

;if(0 && ((*covmax > *cov && div > 0) || (*covmax < *cov && div < 0)))
{  mcxTell(NULL, "max=%f:ctr=%f:div=%f:sum=%f:rat=%f:cov=%f:mcv=%f",max,ctr,div,sum,div/ctr,*cov,*covmax)
,  mcxTell(NULL, "n_join=%ld, n_dom=%ld", n_join, n_dom)
;}
   ;  }
   }


void mclxSubScan
(  const mclMatrix*  mx
,  const mclVector*  dom_cols
,  const mclVector*  dom_rows
,  mclxScore* xscore
)
   {  mclVector*  col   =  NULL
   ;  int n_hits = 0
   ;  mclvScore vscore
   ;  int i

   ;  xscore->max_i     = -FLT_MAX
   ;  xscore->min_i     =  FLT_MAX
   ;  xscore->sum_i     =  0.0
   ;  xscore->ssq_i     =  0.0
   ;  xscore->max_o     = -FLT_MAX
   ;  xscore->min_o     =  FLT_MAX
   ;  xscore->sum_o     =  0.0
   ;  xscore->ssq_o     =  0.0
   ;  xscore->cov       =  0.0
   ;  xscore->covmax    =  0.0
   ;  xscore->n_elem_i  =  0
   ;  xscore->n_elem_o  =  0

   ;  if (!dom_cols)
      dom_cols = mx->dom_cols
      
   ;  if (!dom_rows)
      dom_rows = mx->dom_rows

   ;  for (i=0;i<dom_cols->n_ivps;i++)
      {  long vid = dom_cols->ivps[i].idx

      ;  if ((col = mclxGetVector(mx, vid, RETURN_ON_FAIL, col)))
         {  n_hits++
         ;  mclvSubScan(col, dom_rows, &vscore)
         ;  xscore->max_i =  MAX(vscore.max_i, xscore->max_i)
         ;  xscore->min_i =  MIN(vscore.min_i, xscore->min_i)
         ;  xscore->sum_i += vscore.sum_i
         ;  xscore->ssq_i += vscore.ssq_i
         ;  xscore->max_o =  MAX(vscore.max_o, xscore->max_o)
         ;  xscore->min_o =  MIN(vscore.min_o, xscore->min_o)
         ;  xscore->sum_o += vscore.sum_o
         ;  xscore->ssq_o += vscore.ssq_o
         ;  xscore->n_elem_i += vscore.n_meet
         ;  xscore->n_elem_o += vscore.n_vdif

         ;  if (dom_rows && dom_rows->n_ivps)
            {  double cov, covmax
            ;  mclvScoreCoverage(&vscore, &cov, &covmax)
            ;  xscore->cov += cov
            ;  xscore->covmax += covmax
         ;  }
            col++
      ;  }
      }
      if (dom_rows->n_ivps && n_hits)
      {  xscore->cov /= n_hits
      ;  xscore->covmax /= n_hits
   ;  }
   }



