/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_label_h
#define impala_label_h

#include "vector.h"
#include "util/types.h"
#include "util/io.h"


typedef struct
{  mclVector*     domain
;  char**         labels
;  char*          na          /* not available */
;
}  mclTab       ;



/* If dom is nonNULL, demand equality.
*/

mclTab* mclTabRead
(  mcxIO*         xf
,  const mclv*    dom
,  mcxOnFail      ON_FAIL  
)  ;


char* mclTabGet
(  mclTab*  tab
,  long     id
,  long*    ofs
)  ;


void mclTabFree
(  mclTab**       tabpp
)  ;


#endif

