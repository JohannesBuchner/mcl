/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/



/* TODO
 *  Return partial input when failing?
 *  implies more cumbersome interface.
 *
 *  Recoding needed if other ops are added. There is now a 'normal form'
 *  step that reduces stuff. This is easy as everything is now cast
 *  in the range operator.
 *  Computing a normal form is harder/less useful with modulo ranges.
 *
 *  Who is responsible for the normal form? Currently make_int_set
 *
 *  The discard logic clunks up the code.
 *
 *  Obey min/max with makeSet.
*/

#ifndef DEBUG
#  define DEBUG 0
#else
#  define DEBUG_DEFINED
#endif

#include    <string.h>
#include    <ctype.h>

#include    "parse.h"
#include    "ilist.h"

#include    "util/types.h"
#include    "util/ting.h"
#include    "util/ding.h"
#include    "util/array.h"
#include    "util/err.h"
#include    "util/minmax.h"
#include    "util/tok.h"
#include    "util/list.h"

#include "impala/matrix.h"


enum
{  STATE_EXPECT_NUM
,  STATE_EXPECT_OP
,  STATE_COLLECT        /* collect, continue */
,  STATE_BREAK  = 60    /* collect, break    */
,  STATE_FAIL           /* break             */
}  ;


struct ispec
{  unsigned char op
;  int ops[2]
;
}  ;


struct ispec* parse_int_set
(  mcxLink*       string
,  long*          min
,  long*          max
,  mcxOnFail      ON_FAIL
,  int*           n_ispecs
)  ;


int ispec_cmp
(  const void*  p1
,  const void*  p2
)
   {  const struct ispec *i1 = p1
   ;  const struct ispec *i2 = p2
   ;  if (i1->ops[0] < i2->ops[0])
      return -1
   ;  else if (i1->ops[0] > i2->ops[0])
      return 1
   ;  return 0
;  }


void ispecs_print
(  struct ispec* specs
,  int n_specs
)
   {  struct ispec* spec
   ;  fprintf(stdout, "----- range specs -----\n")
   ;  for (spec=specs;spec<specs+n_specs;spec++)
      {  if (spec->op == 'u')
         fprintf(stdout, "<u> %4d\n", spec->ops[0])
      ;  else if (spec->op == '-')
         fprintf(stdout, "<-> %4d %4d\n", spec->ops[0], spec->ops[1])
      ;  else if (spec->op == 'D')
         fprintf(stdout, "<D> %4d %4d\n", spec->ops[0], spec->ops[1])
   ;  }
   }


int ispecs_nf
(  struct ispec* specs
,  int n_specs
,  long* min
,  long* max
)
   {  int i
   ;  struct ispec* pivot = specs
   ;  const char* me = "ilSpecToVec"
   ;  int n_specs_nf = 0

   ;  for (i=0;i<n_specs;i++)
      {  struct ispec* sp = specs+i
      ;  if (sp->op != '-')
         {  mcxErr(me, "PANIC unexpected op <%c>", (int) sp->op)
         ;  sp->op = '-'
      ;  }
         if (sp->ops[0] > sp->ops[1])
         {  int t = sp->ops[0]
         ;  mcxErr(me, "rearranging ops [%d, %d]", sp->ops[0], sp->ops[1])
         ;  sp->ops[0] = sp->ops[1]
         ;  sp->ops[1] = t
      ;  }
         if (min)
         {  if (sp->ops[1] < *min)
               mcxErr
               (  me
               ,  "discarding out-of-bounds interval [%d,%d]"
               ,  sp->ops[0]
               ,  sp->ops[1]
               )
            ,  sp->op = 'D'
         ;  else if (sp->ops[0] < *min)
               mcxErr(me, "adjusting lo %d to min %ld", sp->ops[0], *min)
            ,  sp->ops[0] = *min
      ;  }
         if (max)
         {  if (sp->ops[0] > *max)
               mcxErr
               (  me
               ,  "discarding out-of-bounds interval [%d,%d]"
               ,  sp->ops[0]
               ,  sp->ops[1]
               )
            ,  sp->op = 'D'
         ;  else if (sp->ops[1] > *max)
               mcxErr(me, "adjusting hi %d to max %ld", sp->ops[1], *max)
            ,  sp->ops[1] = *max
      ;  }
         if (sp->op != 'D')
         {  if (pivot != sp)
            *pivot = *sp         /* struct assignment with array members ok ? */
         ;  pivot++
      ;  }
      }

      n_specs_nf = pivot - specs
   ;  if (n_specs_nf)
      qsort(specs, n_specs_nf, sizeof(struct ispec), ispec_cmp)
   ;  return n_specs_nf
;  }


/* Input should be syntactically correct, but could be rubbish
*/

mclVector* make_int_set
(  struct ispec* specs
,  int n_specs
,  long*   min
,  long*   max
)
   {  int n_ints = 0
   ;  int i, j
   ;  mclv* vec
   ;  mcxstatus status = STATUS_OK
   ;  struct ispec* spec, *pivot
   ;  int n_specs_nr = 0
   ;  const char* me = "ilSpecToVec"

; if (DEBUG) ispecs_print(specs, n_specs)

   ;  if (!n_specs)
      return mclvInit(NULL)

   ;  while (1)
      {  int n_specs_nf
      
      ;  n_specs_nf = ispecs_nf(specs, n_specs, min, max)
; if (DEBUG) ispecs_print(specs, n_specs_nf)

      ;  if (!n_specs_nf)
         return mclvInit(NULL)

      ;  pivot = specs+0

      ;  for (spec=pivot+1;spec<specs+n_specs_nf;spec++)
         {  if (spec->ops[0] > pivot->ops[1]+1)
            {  pivot++
            ;  if (pivot != spec)
                  pivot->ops[0] = spec->ops[0]
               ,  pivot->ops[1] = spec->ops[1]
         ;  }
            else if
            (  spec->ops[0] <= pivot->ops[1]+1
            && spec->ops[1]  > pivot->ops[1]
            )
            pivot->ops[1] = spec->ops[1]
      ;  }

         n_specs_nr = pivot-specs+1
; if (DEBUG) ispecs_print(specs, n_specs_nr)
      ;  for (spec=specs;spec<specs+n_specs_nr;spec++)
         n_ints += spec->ops[1] - spec->ops[0] +1
; if (DEBUG) fprintf(stderr, "have %d ints\n", n_ints)

      ;  if (!(vec = mclvNew(NULL, n_ints)))
         {  status = STATUS_FAIL
         ;  break
      ;  }

         j = 0
      ;  for (spec=specs;spec<specs+n_specs_nr;spec++)
         {  if (j+spec->ops[1]-spec->ops[0]+1>n_ints)
            {  mcxErr(me, "PANIC unexpected overflow")
            ;  break
         ;  }
  if (DEBUG)
  fprintf(stderr, "add [%d-%d]\n", spec->ops[0], spec->ops[1])
;
            for (i=spec->ops[0];i<=spec->ops[1];i++)
               vec->ivps[j].idx = i
            ,  vec->ivps[j++].val = 1.0
      ;  }
         vec->n_ivps = j
      ;  break
   ;  }

      if (status)
      mclvFree(&vec)

   ;  return vec
;  }


mclVector* ilSpecToVec
(  mcxLink*       string
,  long*          min
,  long*          max
,  mcxOnFail      ON_FAIL
)
   {  int n_specs = 0
   ;  mclVector* vec = NULL

   ;  struct ispec* specs =
      parse_int_set
      (  string
      ,  min
      ,  max
      ,  ON_FAIL
      ,  &n_specs
      )

   ;  if (n_specs < 0)
      return NULL

   ;  vec =    n_specs
            ?  make_int_set(specs, n_specs, min, max)
            :  mclvInit(NULL)

   ;  mcxFree(specs)

   ;  if (!vec && ON_FAIL == EXIT_ON_FAIL)
      mcxDie(1, "ilSpecToVec", "curtains")

   ;  return vec
;  }


/* TODO
 *    separate parsing single spec into a subroutine
*/

struct ispec* parse_int_set
(  mcxLink*       string
,  long*          min
,  long*          max
,  mcxOnFail      ON_FAIL
,  int*           n_ispecs
)
   {  mcxBuf specbuf
   ;  mcxLink*    lk    =  string
   ;  const char* me    =  "ilSpecToVec"
   ;  int n_specs       =  0
   ;  mcxstatus status  =  STATUS_OK
   ;  struct ispec* specs = NULL, *specsk

   ;  *n_ispecs = 0

   ;  mcxBufInit(&specbuf, &specs, sizeof(struct ispec), 20)

   ;  while(lk && lk->val)
      {  int   n = 0
      ;  long  x[2]
      ;  int   n_ops = 0
      ;  const mcxTing* field = (mcxTing*) (lk->val)
      ;  const char* s = field->str
      ;  const char* z = s + field->len

      ;  status = STATUS_OK

      ;  if (!mcxStrChrAint(s, isspace, z-s))
         {  lk = lk->next
         ;  continue
      ;  }

         while (1)
         {  status = STATUS_FAIL
         ;  if (1 != sscanf(s, "%ld%n", x+0, &n))
            {  mcxErr(me, "expected integer: <%s>", s)
            ;  break
         ;  }

            n_ops++
         ;  s += n

         ;  if (!(s = mcxTokSkip(s, isspace, z-s)))
            {  status = STATUS_OK
;if(0) fprintf(stderr, "taurus: parsed <%ld>\n", x[0])
            ;  x[1] = x[0]
            ;  break
         ;  }

            if ((unsigned char) *s != '-')
            {  mcxErr(me, "op not supported: <%c>", (int) ((unsigned char) *s))
            ;  break
         ;  }

            s++

         ;  if (1 != sscanf(s, "%ld%n", x+1, &n))
            {  mcxErr(me, "expected integer: <%s>", s)
            ;  break
         ;  }

            n_ops++
         ;  s += n
;if(0) fprintf(stderr, "parsed <%ld> <%ld>\n", x[0], x[1])

         ;  if (mcxTokSkip(s, isspace, z-s))
            {  mcxErr(me, "spurious data: <%s>", s)
            ;  break
         ;  }

            status = STATUS_OK
         ;  break
      ;  }

         if (status)
         break

      ;  if (!(specsk = mcxBufExtend(&specbuf, 1, ON_FAIL)))
         {  status = STATUS_FAIL
         ;  break
      ;  }

         specsk->op = '-'
      ;  specsk->ops[0] = x[0]
      ;  specsk->ops[1] = x[1]
      ;  n_specs++

      ;  lk = lk->next
   ;  }

      if (status)
      {  mcxFree(specs)
      ;  mcxErr(me, "parse failure")
      ;  *n_ispecs = -1
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, me, "off to hades now")
      ;  return NULL
   ;  }

      *n_ispecs = n_specs
   ;  return specs
;  }


#ifndef DEBUG_DEFINED
#  undef  DEBUG
#else
#  undef  DEBUG_DEFINED
#endif


