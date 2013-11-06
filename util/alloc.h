/* (c) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_alloc_h
#define util_alloc_h


/* ========================================================================= *
 *
 *    mcxRealloc is notable, for it *frees* memory and returns a NULL pointer
 *    if the newly requested block has size 0.  This must be so in order to
 *    allow routines to do their math and housekeeping. If they register the
 *    new block as having 0 bytes, then they need not and must not attempt to
 *    free it thereafter.
 *
 * ========================================================================= *
*/

#include <stdlib.h>
#include <stdio.h>

#include "types.h"


void* mcxAlloc
(  int               size
,  mcxOnFail         ON_FAIL
)  ;

void* mcxRealloc
(  void*             object
,  int               new_size
,  mcxOnFail         ON_FAIL
)  ;

void mcxFree
(  void*             object
)  ;

void mcxNFree
(  void*             base
,  int               n_elem
,  int               elem_size
,  void            (*obRelease) (void *)
)  ;

void* mcxNAlloc
(  int               n_elem
,  int               elem_size
,  void*           (*obInit) (void *)
,  mcxOnFail         ON_FAIL
)  ;

void* mcxNRealloc
(  void*             mem
,  int               n_elem
,  int               n_elem_prev
,  int               elem_size
,  void* (*obInit) (void *)
,  mcxOnFail         ON_FAIL
)  ;

void mcxMemDenied
(  FILE*             channel
,  const char*       requestee
,  const char*       unittype
,  int               n
)  ;

void mcxAllocLimits
(  long  maxchunksize
,  long  maxtimes
)  ;


#endif

