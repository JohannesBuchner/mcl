/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/*
 *  For a stochastic vector u and a cluster P, the coverage cov(P,u) equals
 *
 *                  1       ( ---            ---            ) 
 *       |P|  -  -------- * ( \       u_i  - \          u_i )
 *                ctr(u)    ( /__i[=P        /__i[=P^C      )
 *  1 -  ------------------------------------------------------
 *                         | P v S |
 *
 *  Where S is the set of nonzero entries in u.
 *  So for arbitrary nonngative v we need to compute:
 *
 *  div, normalized by sum(v)  (the large term on the upper right)
 *  ctr, sum of squares normalized by sum(v)^2
 *
*/


/*    TODO:
 *    support simultaneous support in mclvScore for complement and meet ?
 *    The needed data structure would feel unwieldy somehow.
 *    e.g.  max_intern
 *          min_intern
 *          max_extern
 *          min_extern
 *    etc
 *    and how about the index for which max/minima are achieved ?
 *
 *    The current setup is still very experimental and bound to be rewritten
 *    some day.
 *
 *    fixme:!
 *    mclxScan will implicitly act on meet if domain is not a subdomain.
 *    this is not so nice; strict interface is better.
 *    not nice because e.g. average coverage is returned, but if subdomain
 *    is not required than the number over which is averaged is not clear.
*/


#ifndef impala_scan_h
#define impala_scan_h


#include "matrix.h"
#include "util/types.h"


typedef struct
{  int         n_vdif   /* vector diff */
;  int         n_meet
;  int         n_ddif   /* domain diff */
;  double      max_i
;  double      min_i
;  double      sum_i
;  double      ssq_i
;  double      max_o
;  double      min_o
;  double      sum_o
;  double      ssq_o
;
}  mclvScore   ;


typedef struct
{  long        n_elem_i
;  long        n_elem_o
;  double      max_i
;  double      min_i
;  double      sum_i
;  double      ssq_i
;  double      max_o
;  double      min_o
;  double      sum_o
;  double      ssq_o
;  double      cov      /* this is the true coverage measure */
;  double      covmax   /* this is the true coverage measure */
;
}  mclxScore   ;



void mclvScan
(  const mclVector*  vec
,  mclvScore* score
)  ;


/*
 * The 'cov' number is the sum of mass of entries in vec that
 * are in dom, minus the sum of mass of entries in vec that are
 * not in dom. It can be used to compute the 'coverage' of dom wrt vec.
*/

void mclvSubScan
(  const mclVector*  vec
,  const mclVector*  dom
,  mclvScore* score
)  ;


/* compute coverage measures from a score struct */

void mclvScoreCoverage
(  mclvScore* score
,  double*   cov
,  double*   covmax
)  ;



void mclxScan
(  const mclMatrix*  mx
,  mclxScore* score
)  ;



void mclxSubScan
(  const mclMatrix*  mx
,  const mclVector*  dom_cols
,  const mclVector*  dom_rows
,  mclxScore* score
)  ;



#endif

