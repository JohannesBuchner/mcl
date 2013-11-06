/* (c) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef minmax_h
#define minmax_h

#include "compile.h"

#if MCX_GNUC_OK && MCX_UTIL_TYPED_MINMAX
/* these buggers do not nest, which I dislike */
#  define MAX(x,y)                                 \
   (  {  const typeof(x) _x = x;                   \
         const typeof(y) _y = y;                   \
         (void) (&_x == &_y);                      \
         _x > _y ? _x : _y;                        \
   }  )
#  define MIN(x,y)                                 \
   (  {  const typeof(x) _x = x;                   \
         const typeof(y) _y = y;                   \
         (void) (&_x == &_y);                      \
         _x < _y ? _x : _y;                        \
   }  )
#else
/* The usual brain-damaged min and max, which do nest though. */
#  define  MAX(a,b)  ((a)>(b) ? (a) : (b))
#  define  MIN(a,b)  ((a)<(b) ? (a) : (b))
#endif


#define ABS(x) (x) > 0 ? (x) : (x) < 0 ? (-(x)) : 0



/* The first version cannot be used recursively.
 * I don't like this at all I think, which is why I turned it off.
*/

#if 0 && MCX_GNUC_OK
#  define  SIGN(a)                                 \
         __extension__                             \
         (  {  typedef  _ta   =  (a)               \
            ;  _ta      _a    =  (a)               \
            ;  _a > 0                              \
               ?  1                                \
               :  _a < 0                           \
                  ?  -1                            \
                  :  0                             \
         ;  }                                      \
         )
#else
#  define  SIGN(a)                                 \
         ((a) > 0 ? 1 : !(a) ? 0 : -1)
#endif
#endif


