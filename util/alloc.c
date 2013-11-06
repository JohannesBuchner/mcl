/* (c) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>


#include "alloc.h"
#include "err.h"
#include "err.h"


static long mcx_alloc_maxchunksize = 1048576;
static long mcx_alloc_maxtimes     = -1;
static mcxbool mcx_alloc_limit     = FALSE;


void mcxAllocLimits
(  long  maxchunksize
,  long  maxtimes
)
   {  if (maxchunksize > 0)
      mcx_alloc_maxchunksize = maxchunksize
   ;  if (maxtimes > 0)
      mcx_alloc_maxtimes = maxtimes
   ;  mcx_alloc_limit = TRUE;
;  }


void* mcxRealloc
(  void*             object
,  int               new_size
,  mcxOnFail         ON_FAIL
)
   {  void*          mblock   =  NULL
   ;  int            status   =  0
   ;
      if (new_size < 0)
      {  mcxErr("mcxRealloc PBD", "negative amount <%d> requested", new_size)
      ;  status   =  1
   ;  }
      else if (!new_size)
      {  if (object)
         mcxFree(object)
   ;  }
      else
      {  if
         (  mcx_alloc_limit
         && (!mcx_alloc_maxtimes-- || new_size > mcx_alloc_maxchunksize)
         )
         mblock = NULL
      ;  else
         mblock = object
               ?  realloc(object, new_size)
               :  malloc(new_size)
   ;  }

      if (new_size && (!mblock))
         mcxMemDenied(stderr, "mcxRealloc", "byte", new_size)
      ,  status   =  1
   ;
      if (status)
      {  if (ON_FAIL == SLEEP_ON_FAIL)
         {  mcxTell("mcxRealloc", "pid %d, entering sleep mode", (int) getpid())
         ;  while(1)
            sleep(1000)
      ;  }
         if (ON_FAIL == EXIT_ON_FAIL)
         {  mcxTell("mcxRealloc", "going down")
         ;  exit(1)
      ;  }
      }

      return mblock
;  }


void mcxNFree
(  void*             base
,  int               n_elem
,  int               elem_size
,  void            (*obRelease) (void *)
)
   {  if (obRelease)
      {  char *ob  =  base

      ;  while (--n_elem >= 0)
         {  obRelease(ob)
         ;  ob += elem_size
      ;  }
      }

      mcxFree(base)
;  }


void* mcxNAlloc
(  int               n_elem
,  int               elem_size
,  void* (*obInit) (void *)
,  mcxOnFail         ON_FAIL
)
   {  return mcxNRealloc(NULL, n_elem, 0, elem_size, obInit, ON_FAIL)
;  }


void* mcxNRealloc
(  void*             mem
,  int               n_elem
,  int               n_elem_prev
,  int               elem_size
,  void* (*obInit) (void *)
,  mcxOnFail         ON_FAIL
)
   {  char*    ob
   ;  mem =  mcxRealloc(mem, n_elem * elem_size, ON_FAIL)
   ;
      if (!mem)
      return NULL

   ;  if (obInit && n_elem > n_elem_prev)
      {  ob  =  ((char*) mem) + (elem_size * n_elem_prev)
      ;  while (--n_elem >= n_elem_prev)
         {  obInit(ob)
         ;  ob += elem_size
      ;  }
      }

      return mem
;  }


void mcxMemDenied
(  FILE*             channel
,  const char*       requestee
,  const char*       unittype
,  int               n
)
   {  mcxErrf
      (  channel
      ,  requestee
      ,  "memory shortage: could not alloc [%d] instances of [%s]"
      ,  n
      ,  unittype
      )
;  }


void mcxFree
(  void* object
)
   {  if (object) free(object)
;  }


void* mcxAlloc
(  int               size
,  mcxOnFail         ON_FAIL
)
   {  return mcxRealloc(NULL, size, ON_FAIL)
;  }


