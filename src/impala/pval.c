/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <math.h>
#include <float.h>
#include <stdlib.h>

#include "util/minmax.h"
#include "util/rand.h"
#include "pval.h"

double fltxConst
(  pval     d
,  void*    arg
)
   {  return *((double*)arg)
;  }


double fltxGQ
(  pval     d
,  void*    arg
)
   {  return d >= *((double*)arg) ? d : 0.0
;  }


double fltxGT
(  pval     d
,  void*    arg
)
   {  return d > *((double*)arg) ? d : 0.0
;  }


double fltxLT
(  pval     d
,  void*    arg
)
   {  return d < *((double*)arg) ? d : 0.0
;  }


double fltxLQ
(  pval     d
,  void*    arg
)
   {  return d <= *((double*)arg) ? d : 0.0
;  }


double fltxCopy
(  pval     flt
,  void*    ignore
)
   {  return flt
;  }


double fltxScale
(  pval     d
,  void*    arg
)
   {  double e = *((double*) arg)
   ;  return e ? d/e : 0.0
;  }


double fltxMul
(  pval     d
,  void*    arg
)
   {  return d * (*((double*)arg))
;  }


double fltxRand
(  pval     flt
,  void*    pbb_keep
)
   {  long d = random()
   ;  return (d / (double) RANDOM_MAX) <= *((double*)pbb_keep) ? flt : 0.0
;  }


double fltxAdd
(  pval     flt
,  void     *add
)
   {  return flt + *((double*) add)
;  }


double fltxPower
(  pval     flt
,  void     *power
)
   {  return pow(flt, *((double*) power))
;  }


double fltxLog
(  pval     flt
,  void*    basep
)
   {  double base = basep ? *((double*) basep) : -1
   ;  if (base > 0 && flt > 0)
      return log(flt) / log(base)
   ;  else if ((!base || !basep) && flt > 0)
      return log(flt)
   ;  else if (!flt)
      return -FLT_MAX
   ;  else
      return 0.0
;  }


double fltxNeglog
(  pval     flt
,  void*    base
)
   {  return -fltxLog(flt, base)
;  }


double fltxExp
(  pval     power
,  void*    flt
)
   {  double base = *((double*) flt)
   ;  if (!base)
      return exp(power)
   ;  else
      return pow(*((double*) flt), power)
;  }


double fltxCeil
(  pval     flt
,  void*    val
)
   {  return flt > *((double*) val) ? *((double*) val) : flt
;  }


double fltxFloor
(  pval     flt
,  void*    val
)
   {  return flt < *((double*) val) ? *((double*) val) : flt
;  }


double fltxPropagateMax
(  pval     d
,  void*    arg
)
   {  if (d > *((double*)arg))
      *((double*)arg) = d
   ;  return d
;  }


double fltLeft
(  pval     d1
,  pval     d2
)
   {  return d1
;  }


double fltRight
(  pval     d1
,  pval     d2
)
   {  return d2
;  }


double fltAdd
(  pval     d1
,  pval     d2
)
   {  return d1 + d2
;  }


double fltSubtract
(  pval     d1
,  pval     d2
)
   {  return d1 - d2
;  }


double fltMultiply
(  pval     d1
,  pval     d2
)
   {  return  d1 * d2
;  }


double fltCross
(  pval     d1
,  pval     d2
)
   {  return  d1 && d2 ? d1 * d2 : d1 ? d1 : d2
;  }


double fltMax
(  pval     d1
,  pval     d2
)
   {  return (d1 > d2) ? d1 : d2
;  }


double fltLoR
(  pval     lft
,  pval     rgt
)
   {  return lft ? lft : rgt
;  }


double fltLaNR
(  pval     lft
,  pval     rgt
)
   {  return rgt ? 0.0 : lft
;  }


double fltLaR
(  pval     lft
,  pval     rgt
)
   {  return lft && rgt ? lft : 0.0
;  }


