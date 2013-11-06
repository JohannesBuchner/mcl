/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 3 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


/* TODO
 *    interface that copies old element someplace.
 *    interface that copies new element destination someplace.
 *    heap merges (for mcxdiameter application [binomial heaps])
 *    removal by key.
 *    key lookup/increment.
*/

#ifndef tingea_heap_h
#define tingea_heap_h


/*    With a min-heap the parents are smaller than the children.
 *    Use a min-heap to compute the K largest elements.
 *    (notice that a new element only has to be compared with the root (when
 *    the heap is at full capacity) to know whether it should enter the heap
 *    and evict the root.
 *
 *    Vice versa, compute K smallest elements with a max-heap.
*/

enum
{  MCX_MIN_HEAP = 10000   /*    find large elements                 */
,  MCX_MAX_HEAP           /*    find small elements                 */
}  ;


typedef struct
{  void     *base
;  dim      heapSize
;  dim      elemSize
;  int      (*cmp)(const void* lft, const void* rgt)
;  mcxenum  type
;  dim      n_inserted
;
}  mcxHeap  ;


mcxHeap* mcxHeapInit
(  void*    heap
)  ;


mcxHeap* mcxHeapNew
(  mcxHeap* heap
,  dim      heapSize
,  dim      elemSize
,  int      (*cmp)(const void* lft, const void* rgt)
,  mcxenum  HEAPTYPE          /* MCX_MIN_HEAP or MCX_MAX_HEAP */
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

