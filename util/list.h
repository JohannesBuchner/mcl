/* (c) Copyright 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_list_h
#define util_list_h

#include "types.h"
#include "gralloc.h"


/*
 * History.
 *    The data structure used is similar to that by Jan van der Steen's pool.c
 *    code, which used to be part of this library. So kudos to Jan.  The
 *    present implementation, which is a bit different, was not directly copied
 *    nor modified. It descended from a botched linked list implementation
 *    having it's own buffered storage. Only at that time I realized that the
 *    right solution for lists and hashes is to have a private pool of gridmem,
 *    rather than deriving it from a global pool (so that we still can defer
 *    thread-safety to malloc). Right?
 *    The present implementation was then derived from the botched linked list
 *    and sanitized afterwards. The linked list.c is still slightly botched.
*/


/* Description
 *    This provides a doubly linked list/link interface with buffered storage
 *    (invisible to the user).  The current interface operates entirely via
 *    links, the commanding structure is hidden. Other interface types may
 *    arise later.
 *   
 *    You can create a number of chains drawing memory from the same pool.
 *   
 *    It only provides basic link insertions and deletions as a convenience
 *    interface, and does not maintain consistency checks, neither on links,
 *    nor on storage.
*/

/* TODO:
 *    -  make prev xor next link optional.
 *    -  make hidden pointer optional.
 *    -  provide interface that uses list struct.
 *    -  convenience interface for tying two chains together.
 *    -  list-to-array interface.
*/


typedef struct mcxLink
{  struct mcxLink*   next
;  struct mcxLink*   prev
;  void*             val
;
}  mcxLink                 ;


/* options:
 * same as for mcxGrimNew
*/

mcxLink*  mcxLinkNew
(  long     capacity_start
,  void*    val
,  mcxbits  options
)  ;


/* Creates new chain, that can later be tied to other chains.
*/

mcxLink*  mcxLinkSpawn
(  mcxLink* lk
,  void* val
)  ;


/* !!!!! freeval doesn't do anything yet
*/

void  mcxLinkFree
(  mcxLink**   lk
,  void        freeval(void* valpp)    /* (yourtype1** valpp)     */
)  ;


mcxLink*  mcxLinkAfter
(  mcxLink*    prev
,  void*       val
)  ;

mcxLink*  mcxLinkBefore
(  mcxLink*    prev
,  void*       val
)  ;

mcxstatus mcxLinkClose
(  mcxLink* need_next
,  mcxLink* need_prev
)  ;


/* You can use the val pointer, immediately after deleting.
*/

mcxLink*  mcxLinkDelete
(  mcxLink*    lk
)  ;


mcxGrim* mcxLinkGrim
(  mcxLink* lk
)  ;


#endif

