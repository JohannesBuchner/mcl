/* (c) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


#ifndef util_heap_h
#define util_heap_h


enum
{  MCX_MIN_HEAP = 10000           /*    find large elements                 */
,  MCX_MAX_HEAP                   /*    find small elements                 */
}  ;


typedef struct
{  void     *base
;  int      heapSize
;  int      elemSize
;  int      (*cmp)(const void* lft, const void* rgt)
;  int      type
;  int      n_inserted
;
}  mcxHeap  ;


mcxHeap* mcxHeapInit
(  void*    heap
)  ;


mcxHeap* mcxHeapNew
(  mcxHeap* heap
,  int      heapSize
,  int      elemSize
,  int      (*cmp)(const void* lft, const void* rgt)
,  int      HEAPTYPE          /* MCX_MIN_HEAP or MCX_MAX_HEAP */
)  ;


void mcxHeapFree
(  mcxHeap**   heap
)  ;

void mcxHeapRelease
(  void* heapv
)  ;  

void mcxHeapClean
(  mcxHeap*   heap
)  ;  

void mcxHeapInsert
(  mcxHeap* heap
,  void*    elem
)  ;


#endif

