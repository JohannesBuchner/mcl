/* (c) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "heap.h"
#include "types.h"
#include "err.h"



mcxHeap* mcxHeapInit
(  void* h
)
   {  mcxHeap* heap  =     (mcxHeap*) h

   ;  if (!heap && !(heap = mcxAlloc(sizeof(mcxHeap), RETURN_ON_FAIL)))
      return NULL

   ;  heap->base     =     NULL
   ;  heap->heapSize =     0
   ;  heap->elemSize =     0
   ;  heap->cmp      =     NULL
   ;  heap->n_inserted =   0
   ;  heap->type     =     0
   ;  return heap
;  }


mcxHeap* mcxHeapNew
(  mcxHeap*    h
,  int         heapSize
,  int         elemSize
,  int (*cmp)  (const void* lft, const void* rgt)
,  int         type           /* MCX_MIN_HEAP or MCX_MAX_HEAP */
)
   {  mcxHeap* heap     =  mcxHeapInit(h)
   ;  char*    base
   ;  mcxstatus status  =  STATUS_FAIL

   ;  while (1)
      {  if (!heap)
         break

      ;  if (heapSize <= 0)
         {  mcxErr("mcxHeapNew PBD", "non-positive request <%d>", heapSize)
         ;  break
      ;  }

         if (type != MCX_MIN_HEAP && type != MCX_MAX_HEAP)
         {  mcxErr("mcxHeapNew PBD", "unknown heap type")
         ;  break
      ;  }

         if
         (  !heap
         || !(heap->base = mcxAlloc (heapSize*elemSize, RETURN_ON_FAIL))
         )
         {  mcxHeapFree(&heap)
         ;  break
      ;  }
         status = STATUS_OK
      ;  break
   ;  }

      if (status)
      {  mcxHeapFree(&heap)
      ;  return NULL
   ;  }

      heap->heapSize    =  heapSize
   ;  heap->elemSize    =  elemSize

   ;  heap->cmp         =  cmp
   ;  heap->type        =  type
   ;  heap->n_inserted  =  0

   ;  base              =  (char*) heap->base

   ;  return heap
;  }


void mcxHeapClean
(  mcxHeap*   heap
)  
   {  heap->n_inserted = 0
;  }


void mcxHeapRelease
(  void* heapv
)  
   {  mcxHeap* heap = (mcxHeap*) heapv

   ;  if (heap->base)
      mcxFree(heap->base)
   ;  heap->base     = NULL
   ;  heap->heapSize = 0
;  }


void mcxHeapFree
(  mcxHeap**   heap
)  
   {  if (*heap)
      {  if ((*heap)->base)
         mcxFree((*heap)->base)
         
      ;  mcxFree(*heap)
      ;  *heap       =  NULL
   ;  }
   }


void mcxHeapInsert
(  mcxHeap* heap
,  void*    elem
)  
   {  char* heapRoot =  (char *) (heap->base)+0
   ;  char* elemch   =  (char *) elem
   ;  int   elsz     =  heap->elemSize
   ;  int   hpsz     =  heap->heapSize

   ;  int (*cmp)(const void *, const void*)
                     =  heap->cmp

   ;  if (heap->type == MCX_MIN_HEAP)
      {
         if (heap->n_inserted  < hpsz)
         {  int i = heap->n_inserted

         ;  while (i != 0 && (cmp)(heapRoot+elsz*((i-1)/2), elemch) > 0)
            {  memcpy(heapRoot + i*elsz, heapRoot + elsz*((i-1)/2), elsz)
            ;  i = (i-1)/2
         ;  }

            memcpy(heapRoot + i*elsz, elemch, elsz)
         ;  heap->n_inserted++
      ;  }

         else if ((cmp)(elemch, heapRoot) > 0)
         {  int root = 0
         ;  int d

         ;  while ((d = 2*root+1) < hpsz)
            {  if
               (  (d+1 < hpsz)
               && (cmp)(heapRoot + d*elsz, heapRoot + (d+1)*elsz) > 0
               )
               d++

            ;  if ((cmp)(elemch, heapRoot + d*elsz) > 0)
               {  memcpy(heapRoot+root*elsz, heapRoot+d*elsz, elsz)
               ;  root     =  d
            ;  }
               else
               break
         ;  }
            memcpy(heapRoot+root*elsz, elemch, elsz)
      ;  }
      }
      else if (heap->type == MCX_MAX_HEAP)
      {
         if (heap->n_inserted  < hpsz)
         {  int i =  heap->n_inserted

         ;  while (i != 0 && (cmp)(heapRoot+elsz*((i-1)/2), elemch) < 0)
            {  memcpy(heapRoot + i*elsz, heapRoot + elsz*((i-1)/2), elsz)
            ;  i = (i-1)/2
         ;  }

            memcpy(heapRoot + i*elsz, elemch, elsz)
         ;  heap->n_inserted++
      ;  }

         else if ((cmp)(elemch, heapRoot) < 0)
         {  int   root     =  0
         ;  int   d

         ;  while ((d = 2*root+1) < hpsz)
            {  if
               (  (d+1<hpsz)
               && (cmp)(heapRoot + d*elsz, heapRoot + (d+1)*elsz) < 0
               )
               d++

            ;  if ((cmp)(elemch, heapRoot + d*elsz) < 0)
               {  memcpy(heapRoot+root*elsz, heapRoot+d*elsz, elsz)
               ;  root     =  d
            ;  }
               else
               break
         ;  }
            memcpy(heapRoot+root*elsz, elemch, elsz)
      ;  }
      }
   }


