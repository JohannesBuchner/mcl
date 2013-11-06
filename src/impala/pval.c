/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <math.h>
#include <float.h>

#include "util/minmax.h"
#include "pval.h"

double fltConstant
(  pval     d
,  void*    arg
)
   {  return d ? *((double*)arg) : 0
;  }


double fltGqBar
(  pval     d
,  void*    arg
)
   {  return d >= *((double*)arg) ? d : 0.0
;  }


double fltGtBar
(  pval     d
,  void*    arg
)
   {  return d > *((double*)arg) ? d : 0.0
;  }


double fltLtBar
(  pval     d
,  void*    arg
)
   {  return d < *((double*)arg) ? d : 0.0
;  }


double fltLqBar
(  pval     d
,  void*    arg
)
   {  return d <= *((double*)arg) ? d : 0.0
;  }


double fltCopy
(  pval     flt
,  void*    ignore
)
   {  return flt
;  }


double fltScale
(  pval     d
,  void*    arg
)
   {  return d * (*((double*)arg))
;  }


double fltShift
(  pval     flt
,  void     *shift
)
   {  return flt + *((double*) shift)
;  }


double fltFloor
(  pval     flt
,  void     *lowest
)
   {  return MAX(flt, *((double*) lowest))
;  }


double fltCeil
(  pval     flt
,  void     *highest
)
   {  return MIN(flt, *((double*) highest))
;  }


double fltPower
(  pval     flt
,  void     *power
)
   {  return pow(flt, *((double*) power))
;  }


double fltLog
(  pval     flt
,  void*    basep
)
   {  double base = basep ? *((double*) basep) : -1
   ;  if (base > 0 && flt > 0)
      return log(flt) / log(base)
   ;  else if (!basep && flt > 0)
      return log(flt)
   ;  else if (!flt)
      return -FLT_MAX
   ;  else
      return 0.0
;  }


double fltNeglog
(  pval     flt
,  void*    base
)
   {  return -fltLog(flt, base)
;  }


double fltPropagateMax
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


