/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "ting.h"
#include "ding.h"
#include "minmax.h"
#include "alloc.h"
#include "array.h"
#include "hash.h"
#include "types.h"
#include "err.h"
#include "tr.h"



void mcxTingRelease
(  void* tingv
)
   {  if (tingv && ((mcxTing*)tingv)->str)
      mcxFree(((mcxTing*)tingv)->str)
;  }


void mcxTingFree_v
(  void  *tingpp
)
   {  mcxTingFree((mcxTing**) tingpp)
;  }


mcxTing* mcxTingShrink
(  mcxTing*    ting
,  int         len
)
   {  if (len < 0)
      len = ting->len + len

   ;  if (len < 0 || len > ting->len)
      {  mcxErr("mcxTingShrink","funny len <%d> newlen <%d> combo",ting->len,len)
      ;  return ting
   ;  }
      else
      {  *(ting->str+len) = '\0'
      ;  ting->len = len;
   ;  }
      return ting
;  }


mcxTing* mcxTingEnsure
(  mcxTing*    ting
,  int         len
)  
   {  if (!ting && !(ting = (mcxTing*) mcxTingInit(NULL)))
      return NULL
   
   ;  if (len < 0)
      {  mcxErr("mcxTingEnsure PBD", "request for negative length <%d>", len)
      ;  return ting
   ;  }
      else if (len <= ting->mxl)
   ;  else
      {  char* t = mcxRealloc(ting->str, len+1, RETURN_ON_FAIL)
      ;  if (!t)
         return NULL
      ;  ting->str = t
      ;  ting->mxl = len
      ;  if (1)
         *(ting->str+ting->mxl) =  '\0'

        /* there should be no need to do this;
         * and for large allocations this may fail.
        */
   ;  }

      return ting
;  }


mcxTing*  mcxTing_print_
(  mcxTing*    dst
,  const char* fmt
,  va_list     *args
)
#define PRINT_BUF_SIZE 200
   {  char buf[PRINT_BUF_SIZE], *src
   ;  mcxTing* txtbuf = NULL
   ;  int m = PRINT_BUF_SIZE
   ;  int n

   ;  n = vsnprintf(buf, PRINT_BUF_SIZE, fmt, *args)

   ;  if (n < 0 || n >= PRINT_BUF_SIZE)
      {  m  =  n >= PRINT_BUF_SIZE ? n : 2 * m
      ;  while (1)
         {  if (!(txtbuf = mcxTingEmpty(txtbuf, m)))
            {  mcxTingFree(&txtbuf)
            ;  return NULL
         ;  }
            n = vsnprintf(txtbuf->str, m+1, fmt, *args)
         ;  if (n < 0 || n > m)
            m *= 2
         ;  else
            break
      ;  }
         src = txtbuf->str
   ;  }
      else
      src = buf

   ;  dst = mcxTingWrite(dst, src)

   ;  mcxTingFree(&txtbuf)
   ;  return dst
#undef PRINT_BUF_SIZE
;  }


mcxTing*  mcxTingPrintSplice
(  mcxTing*    dst
,  int offset
,  int delete
,  const char* fmt
,  ...
)
   {  va_list  args
   ;  mcxTing *infix = NULL

   ;  va_start(args, fmt)
   ;  infix = mcxTing_print_(NULL, fmt, &args)
   ;  va_end(args)

   ;  if (!infix)
      return NULL

   ;  if (!dst)
      return infix

   ;  if
      (  mcxTingSplice(dst, infix->str, offset, delete, infix->len)
      != STATUS_OK
      )
      {  mcxTingFree(&infix)
      ;  return NULL
   ;  }

      mcxTingFree(&infix)
   ;  return dst
;  }


mcxTing*  mcxTingPrint
(  mcxTing*    dst
,  const char* fmt
,  ...
)
   {  va_list  args

   ;  va_start(args, fmt)
   ;  dst = mcxTing_print_(dst, fmt, &args)
   ;  va_end(args)
   ;  return dst
;  }


mcxTing*  mcxTingPrintAfter
(  mcxTing*    dst
,  const char* fmt
,  ...
)
   {  va_list  args
   ;  mcxTing *affix = NULL

   ;  va_start(args, fmt)
   ;  affix = mcxTing_print_(affix, fmt, &args)
   ;  va_end(args)

   ;  if (!dst)
      return affix

   ;  if (!affix)    /* presumably malloc failure */
      return NULL

   ;  if (!mcxTingAppend(dst, affix->str))
      {  mcxTingFree(&affix)
      ;  return NULL
   ;  }

      mcxTingFree(&affix)
   ;  return dst
;  }


static const char* roman[40] =
{  "" , "i" , "ii", "iii", "iv"  , "v" , "vi", "vii", "viii", "ix"
,  "" , "x" , "xx", "xxx", "xl"  , "l" , "lx", "lxx", "lxxx", "xc"
,  "" , "c" , "cc", "ccc", "cd"  , "d" , "dc", "dcc", "dccc", "cm"
,  "" , "m" , "mm", "mmm", "mmmm", ""  ,  "" ,    "",     "", ""
}  ;


mcxTing*  mcxTingRoman
(  mcxTing*  dst
,  int       a
,  mcxbool   ucase
)
   {  int i, x, c, m
   ;  char* p

   ;  if (a >= 5000 || a <= 0)
      return mcxTingWrite(dst, "-")

   ;  i  =  a % 10
   ;  a /=  10
   ;  x  =  a % 10
   ;  a /=  10
   ;  c  =  a % 10
   ;  a /=  10
   ;  m  =  a

   ;  dst = mcxTingPrint
      (  dst
      ,  "%s%s%s%s"
      ,  roman[30+m]
      ,  roman[20+c]
      ,  roman[10+x]
      ,  roman[ 0+i]
      )
   ;  if (dst && ucase)
      for (p=dst->str;p<dst->str+dst->len;p++)
      *p += 'A' - 'a'

   ;  return dst
;  }


mcxTing*  mcxTingDouble
(  mcxTing* dst
,  double   x
,  int      decimals
)
   {  char num[500]
   ;  char* p
   ;  int len = snprintf(num, 500, "%f", x)

   ;  if (decimals < 0)
      {  mcxErr("mcxTingDouble PBD", "negative decimals arg")
      ;  decimals = 6
   ;  }

      if (len < 0 || len >= 500)
      return mcxTingWrite(dst, "[]")

   ;  p = num+len-1
                              /* GNU libcx bug workaround         */
                              /* it returns 2 for length of 'inf' */ 
   ;  if (decimals && strcmp(num, "inf"))
      {  while (*p == '0')
         p--
      ;  if (*p == '.')
         *++p = '0'
      ;  *++p = '\0'
   ;  }

      return mcxTingWrite(dst, num)
;  }


mcxTing*  mcxTingInteger
(  mcxTing*  dst
,  long     x
)
   {  char num[128]
   ;  int len = snprintf(num, 128, "%ld", x)
   ;  if (len < 0 || len >= 128)
      return mcxTingWrite(dst, "[]")
   ;  return mcxTingWrite(dst, num)
;  }


void* mcxTingInit
(  void *  tingv
)
   {  mcxTing *ting  =  (mcxTing*) tingv

   ;  if (!ting)
      {  if (!(ting =  mcxAlloc(sizeof(mcxTing), RETURN_ON_FAIL)))
         return NULL
   ;  }

      if (!(ting->str  =  (char*) mcxAlloc(sizeof(char), RETURN_ON_FAIL)))
      return NULL

   ;  *(ting->str+0) =  '\0'
   ;  ting->len      =  0
   ;  ting->mxl      =  0

   ;  return  ting
;  }


void mcxTingFree
(  mcxTing            **tingpp
)
   {  mcxTing*  ting   =    *tingpp

   ;  if (ting)
      {  if (ting->str)
         mcxFree(ting->str)
      ;  mcxFree(ting)
      ;  *tingpp = NULL
   ;  }
   }


/*
 *    Take string into an existing ting or into a new ting
*/

mcxTing* mcxTingInstantiate
(  mcxTing*          ting
,  const char*       string
)
   {  int length =   string ? strlen(string) : 0

                     /* ensure handles ting==NULL and/or length==0  cases */

   ;  if (!(ting = mcxTingEnsure(ting, length)))
      return NULL

	;  if (string)
      {  strncpy(ting->str, string, length)
      ;  *(ting->str+length)  =  '\0'
   ;  }

      ting->len = length
	;  return ting
;  }


int mcxTingRevCmp
(  const void* t1
,  const void* t2
)
   {  return (strcmp(((mcxTing*)t2)->str, ((mcxTing*)t1)->str))
;  }


int mcxTingCmp
(  const void* t1
,  const void* t2
)
   {  return (strcmp(((mcxTing*)t1)->str, ((mcxTing*)t2)->str))
;  }


int mcxTingPCmp
(  const void* t1
,  const void* t2
)
   {  return (strcmp(((mcxTing**)t1)[0]->str, ((mcxTing**)t2)[0]->str))
;  }


int mcxTingPRevCmp
(  const void* t1
,  const void* t2
)
   {  return (strcmp(((mcxTing**)t2)[0]->str, ((mcxTing**)t1)[0]->str))
;  }


int mcxPKeyTingCmp
(  const void* k1
,  const void* k2
)
   {  return
      strcmp
      (  ((mcxTing*) ((mcxKV**)k1)[0]->key)->str
      ,  ((mcxTing*) ((mcxKV**)k2)[0]->key)->str
      )
;  }


int mcxPKeyTingRevCmp
(  const void* k1
,  const void* k2
)
   {  return
      strcmp
      (  ((mcxTing*) ((mcxKV**)k2)[0]->key)->str
      ,  ((mcxTing*) ((mcxKV**)k1)[0]->key)->str
      )
;  }


mcxstatus mcxTingSplice
(  mcxTing*       ting
,  const char*    pstr
,  int            offset
,  int            n_delete
,  int            n_copy
)
   {  int newlen

   ;  if (!ting)
      {  mcxErr("mcxTingSplice PBD", "void ting argument")
      ;  return STATUS_FAIL
   ;  }
      
      if (offset < 0)
      offset = MAX(0, ting->len + offset + 1)
     /*
      * -1 denotes appending at the end,
      * -2 denotes inserting at the last-but-one position.
     */

   ;  offset = MIN(ting->len, offset)
     /*
      * offset should now be ok.
     */

   ;  if (n_delete == TING_INS_CENTER)
      {  n_delete = MIN(ting->len, n_copy)
      ;  n_delete = MAX(n_delete, 0)
      ;  offset = MAX((ting->len - n_delete) / 2, 0)
   ;  }

      else if (n_delete == TING_INS_OVERWRITE)
      n_delete = MIN(ting->len - offset, n_copy)

   ;  else if (n_delete == TING_INS_OVERRUN || n_delete < 0)
      n_delete = ting->len - offset

   ;  else if (offset + n_delete > ting->len)
      n_delete = ting->len - offset


   ;  if ((newlen = ting->len - n_delete + n_copy) < 0)
      {  mcxErr("mcxTingSplice PBD", "arguments result in negative length")
      ;  return STATUS_FAIL
   ;  }

     /*
      *  essential: mcxSplice does not know to allocate room for '\0'
     */
      if (!mcxTingEnsure(ting, newlen))
      return STATUS_FAIL

   ;  if
      (  mcxSplice
         (  &(ting->str)
         ,  pstr
         ,  sizeof(char)
         ,  &(ting->len)
         ,  &(ting->mxl)
         ,  offset
         ,  n_delete
         ,  n_copy
         )
      != STATUS_OK
      )
      return STATUS_FAIL

  /*
   *  essential: mcxSplice might have realloced, so has to be done afterwards.
  */
   ;  *(ting->str+newlen)  =  '\0'

   ;  if (ting->len != newlen)
      {  mcxErr("mcxTingSplice PBD", "mcxSplice gives unexpected result")
      ;  mcxErr("mcxTingSplice", "nasal demons might fly")
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }


mcxTing* mcxTingNew
(  const char* str
)
   {  return mcxTingInstantiate(NULL, str)
;  }


mcxTing* mcxTingNNew
(  const char*       str
,  int               n
)
   {  mcxTing* ting = mcxTingEnsure(NULL, n)

   ;  if (!ting)
      return NULL

   ;  if (str && n)
      strncpy(ting->str, str, n)
   ;  *(ting->str+n) = '\0'
   ;  ting->len = n
   ;  return ting
;  }


mcxTing* mcxTingEmpty
(  mcxTing*          ting
,  int               len
)
   {  if (!(ting = mcxTingEnsure(ting, len)))
      return NULL

   ;  *(ting->str+0)     =  '\0'
   ,  ting->len          =  0
   ;  return ting
;  }


mcxTing* mcxTingWrite
(  mcxTing*          ting
,  const char*       str
)
   {  return mcxTingInstantiate(ting, str)
;  }


mcxTing* mcxTingNWrite
(  mcxTing*          ting
,  const char*       str
,  int               n
)
   {  if (!(ting = mcxTingEnsure(ting, n)))
      return NULL

   ;  strncpy(ting->str, str, n)

   ;  *(ting->str+n) = '\0'
   ;  ting->len = n

   ;  return ting
;  }


char* mcxTingSubStr
(  const mcxTing*    ting
,  int         offset  
,  int         length
)
   {  char* str

   ;  if (offset < 0 || offset > ting->len)
      offset = ting->len

   ;  if (length < 0 || offset+length > ting->len)
      length = ting->len - offset
      
   ;  if (!(str = mcxAlloc(length+1, RETURN_ON_FAIL)))
      return NULL

   ;  if (length)
      memcpy(str, ting->str+offset, length)         

   ;  *(str+length) = '\0'
   ;  return str
;  }


mcxTing* mcxTingify
(  char* str
)
   {  mcxTing* ting = mcxAlloc(sizeof(mcxTing), RETURN_ON_FAIL)
   ;  ting->str = str
   ;  ting->len = strlen(str)
   ;  return ting
;  }


char* mcxTinguish
(  mcxTing*    ting
)
   {  char* str = ting->str
   ;  mcxFree(ting)
   ;  return str
;  }


char* mcxTingStr
(  const mcxTing*       ting
)
   {  return mcxTingSubStr(ting, 0, ting->len)
;  }



mcxTing* mcxTingAppend
(  mcxTing*             ting
,  const char*          str
)
   {  if (!ting)
      return mcxTingNew(str)

   ;  if
      (  mcxTingSplice
         (  ting
         ,  str
         , -1                             /*    splice offset     */
         ,  0                             /*    delete nothing    */
         ,  str? strlen(str) : 0          /*    string length     */
         )
      != STATUS_OK
      )
      return NULL

   ;  return ting
;  }


mcxTing* mcxTingKAppend
(  mcxTing*    ting
,  const char* str
,  int         n
)
   {  int len = strlen(str)

   ;  while (n--)
      if (!(ting = mcxTingNAppend(ting, str, len)))
      return NULL

   ;  return ting
;  }


mcxTing* mcxTingNAppend
(  mcxTing*             ting
,  const char*          str
,  int                  n
)
   {  if (!ting)
      return mcxTingNWrite(NULL, str, n)

   ;  if
      (  mcxTingSplice
         (  ting
         ,  str
         , -1                             /*    splice offset     */
         ,  0                             /*    delete nothing    */
         ,  n                             /*    string length     */
         )
      != STATUS_OK
      )
      return NULL

   ;  return ting
;  }


mcxTing* mcxTingInsert
(  mcxTing*             ting
,  const char*          str
,  int                  offset
)
   {  if (!ting)
      return mcxTingNew(str)

   ;  if
      (  mcxTingSplice
         (  ting
         ,  str
         ,  offset                        /*    splice offset     */
         ,  0                             /*    delete nothing    */
         ,  str? strlen(str) : 0          /*    string length     */
         )
      != STATUS_OK
      )
      return NULL

   ;  return ting
;  }


mcxTing* mcxTingNInsert
(  mcxTing*          ting
,  const char*       str
,  int               offset
,  int               length
)
   {  if (!ting)
      return mcxTingNWrite(NULL, str, length)

   ;  if
      (  mcxTingSplice
         (  ting
         ,  str
         ,  offset                        /*    splice offset     */
         ,  0                             /*    delete nothing    */
         ,  length                        /*    length of str     */
         )
      != STATUS_OK
      )
      return NULL

   ;  return ting
;  }


mcxTing* mcxTingDelete
(  mcxTing*          ting
,  int               offset
,  int               width
)
   {  if (!ting)
      return NULL

   ;  if
      (  mcxTingSplice
         (  ting
         ,  NULL
         ,  offset                        /*    splice offset     */
         ,  width                         /*    delete width      */
         ,  0                             /*    string length     */
         )
      != STATUS_OK
      )
      return NULL

   ;  return ting
;  }


u32 mcxTingDJBhash
(  const void* ting
)
   {  return(mcxBJhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingBDBhash
(  const void* ting
)
   {  return(mcxBDBhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingBJhash
(  const void* ting
)
   {  return(mcxBJhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingCThash
(  const void* ting
)
   {  return(mcxCThash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingDPhash
(  const void* ting
)
   {  return(mcxDPhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingGEhash
(  const void* ting
)
   {  return(mcxGEhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingOAThash
(  const void* ting
)
   {  return(mcxOAThash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingELFhash
(  const void* ting
)
   {  return(mcxELFhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 (*mcxTingHFieByName(const char* id))(const void* ting)
   {  if (!strcmp(id, "dp"))
      return mcxTingDPhash
   ;  else if (!strcmp(id, "bj"))
      return mcxTingBJhash
   ;  else if (!strcmp(id, "elf"))
      return mcxTingELFhash
   ;  else if (!strcmp(id, "djb"))
      return mcxTingDJBhash
   ;  else if (!strcmp(id, "bdb"))
      return mcxTingBDBhash
   ;  else if (!strcmp(id, "ge"))
      return mcxTingGEhash
   ;  else if (!strcmp(id, "oat"))
      return mcxTingOAThash
   ;  else if (!strcmp(id, "svd"))
      return mcxTingSvDhash
   ;  else if (!strcmp(id, "svd2"))
      return mcxTingSvD2hash
   ;  else if (!strcmp(id, "svd1"))
      return mcxTingSvD1hash
   ;  else if (!strcmp(id, "ct"))
      return mcxTingCThash
   ;  else
      return NULL
;  }


u32 mcxTingSvDhash
(  const void* ting
)
   {  return(mcxSvDhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingSvD2hash
(  const void* ting
)
   {  return(mcxSvD2hash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingSvD1hash
(  const void* ting
)
   {  return(mcxSvD1hash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }


u32 mcxTingHash
(  const void* ting
)
   {  return(mcxDPhash(((mcxTing*) ting)->str, ((mcxTing*) ting)->len))
;  }



/* Fails only for memory reason */

mcxstatus mcxTingTackc
(  mcxTing*  ting
,  unsigned char c
)
   {  if (!ting || !mcxTingEnsure(ting, ting->len+1))
      return STATUS_FAIL
   ;  ting->str[ting->len] = c
   ;  ting->str[ting->len+1] = '\0'
   ;  ting->len += 1
   ;  return STATUS_OK
;  }


/* Fails if last char is not the same as c */

mcxstatus mcxTingTickc
(  mcxTing*  ting
,  unsigned char c
)
   {  if (!ting || !ting->len)
      return STATUS_FAIL
   ;  if ((unsigned char) ting->str[ting->len-1] == c)
      {  mcxTingShrink(ting, -1)
      ;  return STATUS_OK
   ;  }
      return STATUS_FAIL
;  }


