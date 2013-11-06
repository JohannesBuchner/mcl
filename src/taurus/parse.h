/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef taurus_io_h
#define taurus_io_h

#include    "ilist.h"

#include    "impala/vector.h"
#include    "util/types.h"
#include    "util/list.h"

mcxIL*   ilParseIntSet
(  const char*    string
,  long           max
,  mcxOnFail      ON_FAIL
)  ;


/*  Parses stuff like '1,3,8,4-10,-4--20,18->,<'
 *  where '<' and '>' are placeholders for *min and *max (if given)
*/

mclVector*   ilSpecToVec
(  mcxLink*       string
,  long*          min
,  long*          max
,  mcxOnFail      ON_FAIL
)  ;

#endif

