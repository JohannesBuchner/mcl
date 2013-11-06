/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/*
 * TODO
 *    catch exp errors.
*/

#ifndef impala_pval_h
#define impala_pval_h

#include "ivptypes.h"


#define  MCLX_EQT_LT  1
#define  MCLX_EQT_LQ  2
#define  MCLX_EQT_GQ  4
#define  MCLX_EQT_GT  8


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


double fltxConst
(  pval     flt
,  void*    p_constant
)  ;


double fltxGQ
(  pval     d
,  void*    arg
)  ;

double fltxGT
(  pval     d
,  void*    arg
)  ;

double fltxLT
(  pval     d
,  void*    arg
)  ;

double fltxLQ
(  pval     d
,  void*    arg
)  ;

double fltxCeil
(  pval     flt
,  void*    val
)  ;

double fltxFloor
(  pval     flt
,  void*    val
)  ;

double fltxExp
(  pval     power
,  void*    flt
)  ;

double fltxCopy
(  pval     flt
,  void*    ignore
)  ;


double fltxRand
(  pval     flt
,  void*    pbb_keep
)  ;


double fltxMul
(  pval     flt
,  void*    p_factor
)  ;


double fltxScale
(  pval     flt
,  void*    p_scale  /* divide */
)  ;


double fltxPropagateMax
(  pval     flt
,  void*    p_max
)  ;


double fltxPower
(  pval     flt
,  void*    power
)  ;

double fltxAdd
(  pval     flt
,  void*    shift
)  ;

double fltxLog
(  pval     flt
,  void*    base
)  ;

double fltxNeglog
(  pval     flt
,  void*    base
)  ;


double fltLeft
(  pval     d1
,  pval     d2
)  ;


double fltRight
(  pval     d1
,  pval     d2
)  ;


double fltAdd
(  pval     d1
,  pval     d2
)  ;


double fltSubtract
(  pval     d1
,  pval     d2
)  ;


/* This one is funny.  It assumes zero values correspond with missing values
 * that are actually implicitly equal to one, unless two zero values are
 * present. It returns the product, so
 *
 *  d1 &&  d2  ?  d1*d2
 *  d1 && !d2  ?  d1
 * !d1 &&  d2  ?  d2
 * !d1 && !d2  ?  0.0
*/

double fltCross
(  pval     d1
,  pval     d2
)  ;


double fltMultiply
(  pval     d1
,  pval     d2
)  ;


double fltMax
(  pval     d1
,  pval     d2
)  ;


double fltLoR
(  pval     lft
,  pval     rgt
)  ;



double fltLaNR
(  pval     lft
,  pval     rgt
)  ;


double fltLaR
(  pval     lft
,  pval     rgt
)  ;


#endif

