/*   Copyright (C) 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef util_compile_h
#define util_compile_h


#include <errno.h>
#include <unistd.h>


#ifndef MCX_UTIL_THREADED
#  define MCX_UTIL_THREADED     1
#endif


#ifndef MCX_UTIL_TYPED_MINMAX
#  define MCX_UTIL_TYPED_MINMAX 0
#endif



#ifndef MCX_ON_ALLOC_FAILURE
#  define MCX_ON_ALLOC_FAILURE 2
#endif


#if MCX_ON_ALLOC_FAILURE == 1
#  define MCX_ACT_ON_ALLOC_FAILURE return NULL
#elif MCX_ON_ALLOC_FAILURE == 2
#  define MCX_ACT_ON_ALLOC_FAILURE exit(ENOMEM)
#elif MCX_ON_ALLOC_FAILURE == 3
#  define MCX_ACT_ON_ALLOC_FAILURE  while (1) sleep(28)
#endif


#ifndef __GNUC__
#  define   MCX_GNUC_OK       0
#else
#  define   MCX_GNUC_OK       1
#endif


#endif

