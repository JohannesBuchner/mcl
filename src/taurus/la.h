/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef taurus_la_h
#define taurus_la_h

#include "ilist.h"

#include "impala/matrix.h"


mclMatrix* genMatrix
(  mcxIL*   pblock
,  float    p
,  float    q
,  double   ppower
,  double   qpower
)  ;


mclVector* mclvFromIlist
(  mclVector*  vec
,  mcxIL*      il
,  float       f
)  ;  


mclMatrix* genClustering
(  mcxIL*   pblock
)  ;


int idxShare
(  const   mclVector*   v1
,  const   mclVector*   v2
,  mclVector*           m
)  ;


#if 0
mclMatrix* mclxPermute
(  mclMatrix*  src
,  mclMatrix*  dst
,  mcxIL*   il_col
,  mcxIL*   il_row
)  ;
#endif


#endif


