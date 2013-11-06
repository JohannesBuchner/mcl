/* (c) Copyright 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_gralloc_h
#define util_gralloc_h

#include "types.h"


/* gralloc; grid memory allocation; allocation of equally sized chunks
 *
*/

typedef struct mcxGrim mcxGrim;

#define MCX_GRIM_GEOMETRIC    1
#define MCX_GRIM_ARITHMETIC   2

mcxGrim* mcxGrimNew
(  long sz_unit
,  long n_units      /* initial capacity */
,  mcxbits options
)  ;  

void* mcxGrimGet
(  mcxGrim*   src
)  ;

void mcxGrimLet
(  mcxGrim* src
,  void* mem
)  ;

long mcxGrimCount
(  mcxGrim* src
)  ;

void mcxGrimFree
(  mcxGrim** src
)  ;

#endif


