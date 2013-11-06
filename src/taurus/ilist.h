/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef taurus_ilist_h
#define taurus_ilist_h

#include <stdlib.h>
#include <stdio.h>

#include "util/equate.h"

typedef struct
{  int*  L
;  int   n
;
}  mcxIL ;


mcxIL*   ilInit
(  mcxIL*   il
)  ;


mcxIL*   ilInstantiate
(  mcxIL*   il
,  int      n
,  int*     ints
)  ;


mcxIL*   ilVA
(  mcxIL*   il
,  int      k
,  ...
)  ;


void  ilSet
(  mcxIL*   il
,  int      k
)  ;


mcxIL*   ilNew
(  int   n
,  int*  ints
)  ;


mcxIL*   ilComplete
(  mcxIL*   il
,  int      n
)  ;


int ilWriteFile
(  const mcxIL*   il
,  FILE*          f_out
)  ;


mcxIL*   ilInvert
(  mcxIL*   src
)  ;


mcxIL*   ilCon
(  mcxIL*   dst
,  int*     ls
,  int      n
)  ;

int   ilIsOneOne
(  mcxIL*   src
)  ;

int   ilIsNonDescending
(  mcxIL*   src
)  ;

int   ilIsAscending
(  mcxIL*   src
)  ;

int   ilIsNonAscending
(  mcxIL*   src
)  ;

int   ilIsDescending
(  mcxIL*   src
)  ;

int   ilIsMonotone
(  mcxIL*   src
,  int      gradient
,  int      min_diff
)  ;

mcxIL* ilResize
(  mcxIL*   il
,  int      n
)  ;


void    ilPrint
(  mcxIL*   il
,  const char msg[]
)  ;


void   ilAccumulate
(  mcxIL*   il
)  ;


int      ilSqum
(  mcxIL*   il
)  ;

int      ilSum
(  mcxIL*   il
)  ;


void     ilTranslate
(  mcxIL*   il
,  int      dist
)  ;


mcxIL*     ilRandPermutation
(  int      lb
,  int      rb
)  ;


/*
// these three do not belong here,
// should rather be part of revised stats package
// or something alike.
*/

float      ilAverage
(  mcxIL*   il
)  ;

float   ilCenter
(  mcxIL*   il
)  ;

float      ilDeviation
(  mcxIL*   il
)  ;


void     ilFree
(  mcxIL**  ilp
)  ;

void  ilSort
(  mcxIL*   il
)  ;

void  ilRevSort
(  mcxIL*   il
)  ;

mcxIL* ilLottery
(  int      lb
,  int      rb
,  float    p
,  int      times
)  ;


int ilSelectRltBar
(  mcxIL*   il
,  int      i1
,  int      i2
,  int      (*rlt1)(const void*, const void*)
,  int      (*rlt2)(const void*, const void*)
,  int      onlyCount
)  ;



int ilSelectGqBar
(  mcxIL*   il
,  int      ilft
)  ;


int ilSelectLtBar
(  mcxIL*   il
,  int      irgt
)  ;


int ilCountLtBar
(  mcxIL*   il
,  int      ilft
)  ;


                               /* create random partitions at grid--level */
mcxIL*  ilGridRandPartitionSizes
(  int      w
,  int      gridsize
)  ;

                               /* create random partition */
mcxIL*     ilRandPartitionSizes
(  int      w
)  ;


#endif

