/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#ifndef impala_pval_h
#define impala_pval_h

#include "ivptypes.h"


#define  MCLX_LT  1
#define  MCLX_LQ  2
#define  MCLX_GQ  4
#define  MCLX_GT  8


double fltConstant
(  pval     flt
,  void*    p_constant
)  ;


double fltGqBar
(  pval     d
,  void*    arg
)  ;

double fltGtBar
(  pval     d
,  void*    arg
)  ;

double fltLtBar
(  pval     d
,  void*    arg
)  ;

double fltLqBar
(  pval     d
,  void*    arg
)  ;


double fltCopy
(  pval     flt
,  void*    ignore
)  ;


double fltScale
(  pval     flt
,  void*    p_scale
)  ;


double fltPropagateMax
(  pval     flt
,  void*    p_max
)  ;


double fltPower
(  pval     flt
,  void*    power
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

