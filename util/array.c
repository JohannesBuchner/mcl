/*   (C) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 3 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "array.h"
#include "alloc.h"
#include "types.h"
#include "err.h"
#include "minmax.h"


mcxstatus mcxSplice
(  void*           base1pptr
,  const void*     base2ptr
,  dim             size      /*  size of base1 and base2 members          */
,  dim         *pn_base1     /*  # base1 elements currently in use        */
,  dim         *pN_base1     /*  # base1 elements for which is malloced   */
,  ofs           O_base1     /*  splice relative to this element          */
,  dim           d_base1     /*  delete this number of elements           */
,  dim           c_base2     /*  number of elements to copy               */
)  
   {  char **ppr1          =  (char **)   base1pptr
   ;  char* dummy
   ;  const char *ptr2     =  (const char *)   base2ptr

   ;  dim   n_base1        =  *pn_base1
   ;  dim   N_base1        =  *pN_base1
   ;  dim   m_base1 = 0, o_base1 = 0

   ;  const char  *errMsg  =  ""
   ;  mcxstatus   stat     =  STATUS_FAIL

   ;  while (1)
      {  if (n_base1 > N_base1)
         {  errMsg = "integer arguments not consistent"
         ;  break
      ;  }
         if (n_base1 + c_base2 < d_base1)
         {  errMsg = "overly deleterious"
         ;  break
      ;  }
         m_base1 =  (n_base1 + c_base2) - d_base1   /* new size */

      ;  if (O_base1 >= 0)
         o_base1 = O_base1
      ;  else
         {  if ((dim) -O_base1 > n_base1 + 1)
            {  errMsg = "offset specification out of bounds"
            ;  break
         ;  }
            o_base1 = (n_base1 + 1) + O_base1
      ;  }

         if (o_base1 > n_base1)
         {  errMsg = "computed splice offset not in bounds"
         ;  break
      ;  }

         if (*ppr1 == NULL && ptr2 == NULL)
         {  errMsg = "source and destination both void"
         ;  break
      ;  }

         if (o_base1 + d_base1 > n_base1)
         {  errMsg = "not that many elements to delete"
         ;  break
      ;  }
         stat = STATUS_OK
      ;  break
   ;  }

      if (stat != STATUS_OK)
      {  mcxErr("[mcxSplice PBD]", "%s", errMsg)
      ;  mcxErr
         (  "[mcxSplice PBD]"
         ,  "[n1, %lu] [N1, %lu] [o1, %lu] [d1, %lu] [c2, %lu]"
         ,     (ulong) n_base1, (ulong) N_base1
            ,  (ulong) O_base1
            ,  (ulong) d_base1, (ulong) c_base2
         )
      ;  return STATUS_FAIL
   ;  }

      if (m_base1 > N_base1)
      {  if (!(dummy = mcxRealloc(*ppr1, size*m_base1, RETURN_ON_FAIL)))
         {  mcxMemDenied(stderr, "mcxSplice", "void", m_base1)
         ;  return STATUS_FAIL
      ;  }
         *pN_base1 = N_base1 = m_base1
      ;  *ppr1 = dummy
   ;  }

      if (o_base1 < n_base1)
      memmove
      (  *ppr1 + size*(o_base1 + c_base2)
      ,  *ppr1 + size*(o_base1 + d_base1)
      ,  size*(n_base1 - o_base1 - d_base1)
      )

   ;  if (c_base2)
      memcpy
      (  *ppr1 + size * (o_base1)
      ,  ptr2
      ,  size*(c_base2)
      )
   ;  *pn_base1      =  m_base1
   ;  return STATUS_OK
;  }


dim mcxDedup
(  void*    base
,  dim      nmemb
,  dim      size
,  int      (*cmp)(const void *, const void *)
,  void     (*merge)(void *, const void *)
)  
   {  dim   k  =  0
   ;  dim   l  =  0 
      
   ;  while (l < nmemb)
      {  if (k != l)
         memcpy(((char*)base) + k * size, ((char*)base) + l * size, size)

      ;  while
         (  ++l < nmemb
         && (  cmp
            ?  (!cmp(((char*)base) + k * size, ((char*)base) + l * size))
            :  (!memcmp(((char*)base) + k*size, ((char*)base) + l*size, size))
            )  
         )  
         {  if (merge)
            merge(((char*)base) + k * size, ((char*)base) + l * size)
      ;  }
         k++
   ;  }

      return k       
;  }


mcxstatus mcxResize
(  void*          mempp
,  dim            size
,  dim*           ct
,  dim            newct
,  mcxOnFail      ON_FAIL
)
   {  char **pp   =  (char **) mempp
   ;  char* ptr   =  *pp

   ;  if (newct && !(ptr = mcxRealloc(ptr, size*newct, ON_FAIL)))
      return STATUS_FAIL

   ;  *pp   =  ptr
   ;  *ct   =  newct
   ;  return STATUS_OK
;  }



mcxstatus mcxBufInit
(  mcxBuf*  buf
,  void*    mempptr
,  dim      size
,  dim      n_alloc
)
   {  char **usrpptr    =     (char **) mempptr
   ;  char* dummy
   ;  buf->mempptr      =     mempptr

   ;  buf->size         =     size
   ;  buf->n            =     0
   ;  buf->bFinalized   =     0
   ;  buf->factor       =     1.41

   ;  dummy             =     mcxRealloc
                              (  *usrpptr
                              ,  n_alloc * size
                              ,  RETURN_ON_FAIL
                              )

   ;  if (n_alloc && !dummy)
      {  mcxMemDenied(stderr, "mcxBufInit", "char", n_alloc * size)
      ;  buf->n_alloc = 0
      ;  return STATUS_FAIL
   ;  }

      buf->n_alloc = n_alloc
   ;  *usrpptr = dummy
   ;  return STATUS_OK
;  }


void* mcxBufExtend
(  mcxBuf*     buf
,  dim         n_request
,  mcxOnFail   ON_FAIL
)
   {  dim   oldsize     =     buf->n
   ;  char **usrpptr    =     (char **) buf->mempptr
   ;  char* dummy

   ;  if (buf->bFinalized)
      mcxErr("mcxBufExtend PBD", "extending finalized buffer")

   ;  if (buf->n_alloc < buf->n + n_request)
      {  dim n_new    
         =  MCX_MAX
            (  (dim) (buf->n_alloc * buf->factor + 8)
            ,  (dim) (buf->n + n_request)
            )

      ;  dummy = mcxRealloc(*usrpptr, n_new * buf->size, ON_FAIL)

      ;  if (n_new && !dummy)
         {  mcxMemDenied(stderr,"mcxBufExtend","char",buf->n*buf->size)
         ;  return NULL
      ;  }

         buf->n_alloc = n_new
      ;  *usrpptr = dummy
   ;  }

      buf->n += n_request
   ;  return *usrpptr + (oldsize * buf->size)
;  }


dim mcxBufFinalize
(  mcxBuf*    buf
)
   {  char **usrpptr    =  (char **) buf->mempptr
   ;  char* dummy

   ;  if (buf->bFinalized)
      mcxErr("mcxBufFinalize PBD", "extending finalized buffer")
   ;  else
      buf->bFinalized   =  1

   ;  dummy             =  mcxRealloc
                           (  *usrpptr
                           ,  buf->n * buf->size
                           ,  RETURN_ON_FAIL
                           )

   ;  if (buf->n && !dummy)
      {  mcxMemDenied(stderr, "mcxBufFinalize", "char", buf->n * buf->size)
      ;  errno = ENOMEM
      ;  return buf->n
   ;  }

      *usrpptr = dummy
   ;  buf->n_alloc =  buf->n

   ;  return buf->n
;  }


void mcxBufReset
(  mcxBuf*     buf
,  void*       mempptr
)
   {  if (!buf->bFinalized)
      mcxErr("mcxBufReset PBD", "buffer not finalized")

   ;  buf->mempptr      =     mempptr
   ;  buf->n            =     0
   ;  buf->n_alloc      =     0
   ;  buf->bFinalized   =     0
;  }



void* mcxBsearchCeil
(  const void *key
,  const void *base
,  dim nmemb
,  dim size
,  int (*cmp)(const void *, const void *)
)
   {  ofs lft                 /* fixme (consider -1 initialization) */
   ;  dim bar, rgt
   ;  if (!nmemb || cmp(key, ((char*)base) + (nmemb-1) * size) > 0)
      return NULL

   ;  if (cmp(key, base) <= 0)

   ;  lft = -1
   ;  rgt = nmemb
   ;  bar = nmemb / 2

   ;  while (lft < 0 || bar < (dim) rgt)
      {  if (cmp(key, ((char*) base) + bar*size) > 0)
         lft = bar
      ;  else
         rgt = bar
      ;  bar = rgt - (rgt-lft) / 2;
   ;  }
      return (((char*) base) + bar * size)
;  }



void* mcxBsearchFloor
(  const void *key
,  const void *base
,  dim nmemb
,  dim size
,  int (*cmp)(const void *, const void *)
)
   {  ofs lft
   ;  dim bar, rgt
   ;  if (!nmemb || cmp(key, base) < 0)
      return NULL

   ;  lft = -1
   ;  rgt = nmemb
   ;  bar = nmemb / 2

   ;  while (lft < 0 || bar > (dim) lft)
      {  if (cmp(key, ((char*) base) + bar*size) < 0)
         rgt = bar
      ;  else
         lft = bar
      ;  bar = lft + (rgt-lft) / 2;
   ;  }
      return (((char*) base) + bar * size)
;  }



