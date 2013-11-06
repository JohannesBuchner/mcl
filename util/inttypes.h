/* (c) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_inttypes_h
#define util_inttypes_h

#include <limits.h>

#if UINT_MAX >= 4294967295
#  define MCX_UINT32 unsigned int
#  define MCX_INT32  int
#else
#  define MCX_UINT32 unsigned long
#  define MCX_INT32  long
#endif

typedef  MCX_UINT32     u32 ;
typedef  MCX_INT32      i32 ;
typedef  unsigned char  u8  ;

#endif

