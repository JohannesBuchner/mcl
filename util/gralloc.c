/* (c) Copyright 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


#include "alloc.h"
#include "gralloc.h"


/* TODO
 *    calloc stuff.
 *    implement arithmetic capacity strategy.
*/



#define DEBUG 0

typedef struct memnext
{  struct memnext*  next
;
}  memnext           ;

      /* future idea; use higher bita as perbytage style coefficients. duh. */

#define n1(grim)     ((grim->flags >> 16) & 0xFF)
#define n2(grim)     ((grim->flags >> 24) & 0xFF)


typedef struct grim_buf
{  char*             units       /* n_units * (sizeof(memnext) + sz_unit)     */
;  long              n_units     /* number of units available for this struct */
;  struct grim_buf*  prev
;
}  grim_buf          ;



struct mcxGrim
{  grim_buf*         buf
;  long              sz_unit     /* size of one unit                          */
;  memnext*          na          /* next available                            */
;  long              ct          /* number in use                             */
;  mcxbits           flags       /* allocation strategy, numbers              */
;
}  ;


grim_buf* grim_buf_new
(  long      sz_unit
,  long      n_units
)
   {  long i
   ;  grim_buf* buf
   ;  char* units
   ;  long sz_load = sizeof(memnext) + sz_unit
      
   ;  if (!(buf = mcxAlloc(sizeof(grim_buf), RETURN_ON_FAIL)))
      return NULL

   ;  if
      ( !(buf->units
      =  units
      =  mcxAlloc(n_units * sz_load, RETURN_ON_FAIL)
      )  )
      {  mcxFree(buf)
      ;  return NULL
   ;  }

      buf->prev =  NULL
   ;  buf->n_units =  n_units

;if (DEBUG)
fprintf (stderr, "Extending grim with <%ld> units\n", n_units);

   ;  for (i=0;i<n_units-1;i++)
         ((memnext*) (units + i * sz_load))->next
      =   (memnext*) (units + (i+1) * sz_load)

   ;  ((memnext*) (buf->units + (n_units-1) * sz_load))->next = NULL

   ;  return buf
;  }


mcxGrim* mcxGrimNew
(  long sz_unit
,  long n_units
,  mcxbits options
)  
   {  mcxGrim* src = mcxAlloc(sizeof(mcxGrim), RETURN_ON_FAIL)
   ;  if (!src)
      return NULL

   ;  if (!(src->buf = grim_buf_new(sz_unit, n_units)))
      {  mcxFree(src)
      ;  return NULL
   ;  }

      src->buf->prev = src->buf

   ;  src->flags =  options 
   ;  src->na    =  (void*) src->buf->units
   ;  src->ct    =  0
   ;  src->sz_unit =  sz_unit

   ;  return src
;  }


mcxbool mcx_grim_extend
(  mcxGrim*  src
)
   {  grim_buf* prevbuf = src->buf->prev
   ;  long diff         =  prevbuf->n_units - prevbuf->prev->n_units
   ;  long n_units      =     (src->flags & MCX_GRIM_GEOMETRIC) || !diff
                           ?     2 * prevbuf->n_units
                           :     prevbuf->n_units + (diff > 0 ? diff : -diff)
   ;  grim_buf* newbuf  =  grim_buf_new(src->sz_unit, n_units)
   ;  if (!newbuf)
      return FALSE

   ;  newbuf->prev  =  src->buf->prev
   ;  src->buf->prev =  newbuf
   ;  src->na = (void*) newbuf->units
   ;  return TRUE
;  }


void  mcxGrimFree
(  mcxGrim** srcp
)
   {  grim_buf *buf = (*srcp)->buf, *this = buf->prev
   ;  buf->prev = NULL

   ;  while (this)
      {  grim_buf* tmp = this->prev
      ;  mcxFree(this->units)
      ;  mcxFree(this)
      ;  this = tmp
   ;  }
      mcxFree(*srcp)
   ;  *srcp = NULL
;  }


void mcxGrimLet
(  mcxGrim* src
,  void* mem
)
   {  memnext* na =  (void*) ((char*) mem - sizeof(memnext))
   ;  na->next    =  src->na ? src->na->next : NULL
   ;  src->na     =  na
   ;  src->ct--
;  }


void* mcxGrimGet
(  mcxGrim*   src
)
   {  void* mem
   ;  if (!src->na && !mcx_grim_extend(src))
      return NULL
   ;  mem = ((char*) src->na) + sizeof(memnext)
   ;  src->na = src->na->next
   ;  src->ct++
   ;  return mem
;  }


long mcxGrimCount
(  mcxGrim* src
)
   {  return src->ct
;  }


#undef DEBUG

