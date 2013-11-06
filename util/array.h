/*  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef util_array_h
#define util_array_h


#include "types.h"
#include "alloc.h"


mcxstatus mcxSplice
(  void*           base1pp   /*  _address_ of pointer to elements       */
,  const void*     base2p    /*  pointer to elements                    */
,  int             size      /*  size of base1 and base2 members        */
,  int          *n_base1     /*  total length of elements after base1   */
,  int          *N_base1     /*  number of alloc'ed elements for base1  */
,  int           o_base1     /*  splice relative to this ofset          */
,  int           d_base1     /*  delete this number of elements         */
,  int           c_base2     /*  number of elements to copy             */
)  ;


int mcxDedup
(  void*          base     
,  int            nmemb    
,  int            size     
,  int            (*cmp)(const void *, const void *)
,  void           (*merge)(void *, const void *)
)  ;


mcxstatus mcxResize
(  void*          mempp
,  int            size        /* ho hum, should be size_t */
,  int*           ct          /* ho hum, should be size_t* */
,  int            newct
,  mcxOnFail      ON_FAIL
)  ;



typedef struct
{  void*       mempptr
;  int         size
;  int         n
;  int         n_alloc
;  float       factor
;  mcxbool     bFinalized
;
}  mcxBuf      ;



/*    
 *    *mempptr should be peekable; NULL or valid memory pointer. 
*/

mcxstatus mcxBufInit
(  mcxBuf*     buf
,  void*       mempptr
,  int         size
,  int         n
)  ;


/*
 *    Extends the buffer by n_request unitialized chunks and returns a pointer
 *    to this space. It is the caller's responsibility to treat this space
 *    consistently. The counter buf->n is increased by n_request.
 *
 *    If we cannot extend (realloc), a NULL pointer is returned;
 *    the original space is left intact.
 *
 *    Returns NULL on (alloc) failure
*/

void* mcxBufExtend
(  mcxBuf*     buf
,  int         n_request
,  mcxOnFail   ON_FAIL
)  ;


/*
 *    Make superfluous memory reclaimable by system,
 *    prepare for discarding buf (but not *(buf->memptr)!)
 *
 *    If for some bizarre reason we cannot shrink (realloc), -1 is returned.
 *    the original space is left intact. Its size is in buf.n .
*/

int mcxBufFinalize
(  mcxBuf*  buf
)  ;


/*
 *    Make buffer refer to a new variable. Size cannot be changed,
 *    so variable should be of same type as before.
*/

void mcxBufReset
(  mcxBuf*  buf
,  void*    mempptr
)  ;

#endif

