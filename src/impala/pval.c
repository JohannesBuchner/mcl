/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <math.h>

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


double fltPower
(  pval     flt
,  void     *power
)
   {  return pow(flt, *((double*) power))
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


