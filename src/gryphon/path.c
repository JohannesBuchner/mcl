/*   (C) Copyright 2006, 2007 Stijn van Dongen
 *
*/

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <limits.h>

#include "path.h"

#include "impala/matrix.h"
#include "impala/io.h"

#include "util/alloc.h"
#include "util/err.h"


#if 0
#define dbg(format, ...) if(1)fprintf(stderr, format, ## __VA_ARGS__);
#else
#define dbg(format, ...) ;
#endif


/* mq make this work for sparse domains */


void mclgSSPxyFree
(  SSPxy** sspopp
)
   {  SSPxy* sspo = * sspopp
   ;  mcxFree(sspo->seen)
   ;  mcxFree(sspo->aow)
   ;  mcxFree(sspo)
   ;  *sspopp = NULL
;  }


SSPxy* mclgSSPxyNew
(  const mclx* mx
)
   {  SSPxy* sspo =  mcxAlloc(sizeof sspo[0], EXIT_ON_FAIL)

                                             /* only need 2-bit array
                                              * so seen can be shortened
                                              * Then use x >> 2 as address,
                                              * use bit positions 
                                              *       a = 1 << ((x & 3) << 1)
                                              *  a << 1
                                             */
   ;  sspo->seen  =  calloc(N_COLS(mx), 1)
                                             /* alternation-o-waves;
                                              * overallocate because final
                                              * wave overlaps (if hit)
                                             */

   ;  sspo->aow   =  calloc(2 * N_COLS(mx), sizeof sspo->aow[0])

   ;  sspo->aow_n =  0
   ;  sspo->mx    =  mx
   ;  sspo->pathmx=  NULL
   ;  sspo->length = 0
   ;  sspo->n_considered = 0
   ;  sspo->n_involved = 0
   ;  return sspo
;  }


void mclgSSPxyReset
(  SSPxy* sspo
)
   {  dim i
   ;  for (i=0;i<sspo->aow_n;i++)
      sspo->seen[sspo->aow[i]] = 0
   ;  sspo->aow_n = 0
   ;  sspo->length = 0
   ;  sspo->n_considered = 0
   ;  sspo->n_involved = 0
   ;  sspo->src = -1
   ;  sspo->dst = -1
   ;  mclxFree(&(sspo->pathmx))
;  }


static void sspx_dump_aow_unused
(  u8* seen
,  long* aow
,  dim sz_aow
)
   {  dim t
   ;  fprintf(stderr, "-->\n")
   ;  for (t=0;t<sz_aow;t++)
      fprintf
      (  stderr
      , "i %d idx %d tag %d\n"
      , (int) t
      , (int) aow[t]
      , (int) seen[aow[t]]
      )
   ;  fprintf(stderr, "<--\n")
;  }



static mcxstatus sspxy_flood
(  SSPxy* sspo
,  long  x
,  long  y
)
   {  dim  sz_aow    =  2
   ;  dim  aow_1ofs  =  0
   ;  dim  aow_1len  =  1
   ;  dim  aow_2ofs  =  1
   ;  dim  aow_2len  =  1

   ;  dim  length    =  1
   ;  mcxbool seen_hit = FALSE

   ;  const mclx* mx =  sspo->mx
   ;  u8*   seen     =  sspo->seen     /* size N_COLS */
   ;  long* aow      =  sspo->aow      /* size N_COLS */
   ;  dim*  aow_n    =  &(sspo->aow_n)

   ;  mcxbits  wave_bit  =  1    /* start with x wave */

   ;  aow[0]         =  x
   ;  aow[1]         =  y

   ;  seen[aow[0]]   =  1     /* sentinel bit for x wave */
   ;  seen[aow[1]]   =  2     /* sentinel bit for y wave */

   ;  *aow_n         =  0

   ;  while (1)
      {  dim i
      ;  dim sz_aow_cache = sz_aow

      ;  for (i=aow_1ofs ; i<aow_1ofs+aow_1len; i++)
         {  mclv* ls = mx->cols+aow[i]
         ;  mclp* newivp = ls->ivps, *newivpmax = newivp + ls->n_ivps

         ;  while (newivp < newivpmax)
            {  u8* tst = seen+(newivp->idx)
            ;  int bits = *tst & 3
            ;  if (bits & wave_bit)
               NOTHING        /* seen as current, skip */
            ;  else
               {  if (bits & (wave_bit ^ 3))    /* the other wave */
                  seen_hit = TRUE
               ;  else
                     aow[sz_aow++] = newivp->idx
                  ,  sspo->n_considered++
               ;  *tst |= wave_bit
            ;  }
               newivp++
         ;  }
         }

         if (seen_hit)
         {  for (i=aow_2ofs; i<aow_2ofs+aow_2len; i++)
            seen[aow[i]] |= 4
         ;  for (i=sz_aow_cache;i<sz_aow;i++)   /* clean up because ..     */
            seen[aow[i]] = 0
         ;  sz_aow = sz_aow_cache               /* .. we reset length here */
      ;  }

dbg("sz_aow %d sz_aow_cache %d\n", (int) sz_aow, (int) sz_aow_cache)

                     /* fixme should below clause for clarity just be seen_hit? (see above) */
         if (sz_aow_cache == sz_aow)
         break

      ;  length++
      ;  aow_1ofs = aow_2ofs
      ;  aow_1len = aow_2len
      ;  aow_2ofs = sz_aow_cache
      ;  aow_2len = sz_aow - sz_aow_cache
      ;  wave_bit ^= 3     /* toggle between x and y wave */
   ;  }

      *aow_n = sz_aow

#if 0
      sspxy_dump_aow(seen, aow, sz_aow)
#endif

   ;  sspo->length = seen_hit ? length : -1
   ;  return sspo->length > 0 ? STATUS_OK : STATUS_FAIL
;  }


static void sspx_make_vector
(  u8*   seen
,  long* nums
,  dim   sz
,  mclv* dst
)
   {  dim d, o
   ;  mclvResize(dst, sz)
   ;  for (d=0, o=0;d<sz;d++)
      {  if (seen[nums[d]])
            dst->ivps[o].idx = nums[d]
         ,  dst->ivps[o].val = 1.0
         ,  o++
   ;  }
      mclvResize(dst, o)
   ;  mclvSort(dst, mclpIdxCmp)   
;  }


static void sspxy_rm_dead_ends
(  SSPxy* sspo
)
   {  const mclx* mx =  sspo->mx
   ;  u8* seen       =  sspo->seen
   ;  long* aow      =  sspo->aow
   ;  dim i          =  sspo->aow_n, n_rm = 0, end_run = 0
   ;  mcxbits tag = 0
      
                                 /* prune dead ends from center wave */
   ;  while (seen[aow[i-1]] & 4)
      {  i--
      ;  if ((seen[aow[i]] & 3) != 3)
            seen[aow[i]] = 0
         ,  n_rm++
   ;  }

      end_run = i
   ;  tag = seen[aow[i-1]] & 3

   ;  while (i-- > 0)
      {  mclv* ls = mx->cols+aow[i]
      ;  mclp* ivp, *ivpmax

                                    /* pretty ugly code,
                                     * with 0 special cased below loop.
                                    */
      ;  if (tag != (seen[aow[i]] & 3))
         {  dim t
         ;  for (t=i+1; t<end_run; t++)
            if (seen[aow[t]] & 8)   /* make permanent, */
            seen[aow[t]] ^= 12      /* switch 8 bit to 4 bit */
         ;  end_run = i+1
         ;  tag = seen[aow[i]] & 3
      ;  }

         for (ivp=ls->ivps, ivpmax=ivp+ls->n_ivps; ivp < ivpmax; ivp++)
         if (seen[ivp->idx] & 4)
         break

      ;  if (ivp != ivpmax)
         seen[aow[i]] |= 8          /* flag provisionally */
      ;  else
            seen[aow[i]] = 0
         ,  n_rm++
   ;  }

      seen[aow[0]] = 5

#if 0
      sspxy_dump_aow(seen, aow, sspo->aow_n)
#endif

   ;  sspo->n_involved = sspo->aow_n - n_rm
;  }


static mcxstatus sspxy_make_pathmx
(  SSPxy* sspo
)
   {  mclv* dom_cols = mclvInit(NULL)
   ;  mclx* pathmx

   ;  mcxbits tag
   ;  dim ofs = 0, i, px = 0, py

   ;  u8* seen =  sspo->seen
   ;  long* aow   =  sspo->aow
   ;  dim aow_n   =  sspo->aow_n
   ;  dim length  =  sspo->length

   ;  py = length
   ;  tag = seen[aow[0]] & 7
         
   ;  sspx_make_vector(seen, aow+0, aow_n, dom_cols)
   ;  pathmx = mclxAllocZero(mclvCanonical(NULL, length+1, 1.0), dom_cols)

   ;  for (i=1; i< aow_n; i++)
      {  mcxbits tag_next = seen[aow[i]] & 7
      ;  if (tag_next && tag_next != tag)
         {  if (tag == 5)
            sspx_make_vector(seen, aow+ofs, i-ofs, pathmx->cols+px++)
         ;  else if (tag == 6)
            sspx_make_vector(seen, aow+ofs, i-ofs, pathmx->cols+py--)
         ;  tag = tag_next
         ;  ofs = i
      ;  }
         if (tag_next == 7)
         break
   ;  }

      if (px != py)
      {  mcxErr
         (  "mclgSSPxyQuery"
         ,  "panic: px/py %d/%d do not play\n"
         ,  (int) px
         ,  (int) py
         )
      ;  mclxFree(&pathmx)
      ;  return STATUS_FAIL
   ;  }

      sspx_make_vector(seen, aow+ofs, aow_n-ofs, pathmx->cols+px)

#if 0
      {  mcxIO* x = mcxIOnew("-", "w")
      ;  if (0)
         for (i=0; i< N_COLS(pathmx); i++)
         fprintf
         (  stderr
         ,  "pathmx: vec %d has num %d\n"
         ,  i
         ,  pathmx->cols[i].n_ivps
         )
      ;  mclxWrite(pathmx, x, 0, RETURN_ON_FAIL)
      ;  mcxIOfree(&x)
   ;  }
#endif

   ;  sspo->pathmx = pathmx
   ;  return STATUS_OK
;  }


mcxstatus mclgSSPxyQuery
(  SSPxy* sspo
,  long a
,  long b
)
   {  const char* msg = NULL

   ;  do
      {  dim N = 0

      ;  if (!sspo->mx)
         {  msg = "no matrix"
         ;  break
      ;  }

         N = N_COLS(sspo->mx)

      ;  if (!mclxGraphCanonical(sspo->mx))
         {  msg = "not a canonical domain"
         ;  break
      ;  }

         if (a == b || a < 0 || b < 0 || (dim) a >= N || (dim) b >= N)
         {  msg = "start/end range error"
         ;  break
      ;  }

         sspo->src = a
      ;  sspo->dst = b

      ;  if (sspxy_flood(sspo, a, b))
         break

      ;  sspxy_rm_dead_ends(sspo)

      ;  if (sspxy_make_pathmx(sspo))
         {  msg = "make path error"
         ;  break
      ;  }
      }
      while (0)

   ;  if (msg)
      mcxErr("mclgSSPxyQuery", "%s", msg)
   ;  return msg ? STATUS_FAIL : STATUS_OK
;  }


mclv* mclgSSPd
(  const mclx* graph
,  const mclv* domain
)
   {  mclv* punters = mclvClone(graph->dom_cols), *new = mclvInit(NULL)
   ;  SSPxy* sspo = mclgSSPxyNew(graph)
   ;  dim d

   ;  mclvMakeConstant(punters, 0.5)

   ;  for (d=0;d<domain->n_ivps;d++)
      {  dim e
      ;  long x = domain->ivps[d].idx
      ;  for (e=d+1;e<domain->n_ivps;e++)
         {  long y = domain->ivps[e].idx
         ;  if (!mclgSSPxyQuery(sspo, x, y))
            {  mclgUnionv(sspo->pathmx, NULL, NULL, SCRATCH_READY, new)
            ;  mclvUpdateMeet(punters, new, fltAdd)
         ;  }
            mclgSSPxyReset(sspo)
      ;  }
   ;  }

      mclgSSPxyFree(&sspo)
   ;  mclvFree(&new)

   ;  mclvSelectGqBar(punters, 1.0)
   ;  return punters
;  }



/* If loops are present:
 *    for each pair subtract number of loops.
 *    This becomes in summation:
 *       sz_eff * isLoopy(vec1->vid) + Sum(isloopy(vec1->ivps[j].idx)) 
 * Then caller can provide a sparse vector of all nodes that
 * have loops (to compute Sum()).
*/


double mclgCLCF
(  const mclx* mx
,  const mclv* vec
,  const mclv* has_loop
)
   {  dim j
   ;  mclv* vec2 = NULL
   ;  mcxbool loopy = mclvGetIvp(vec, vec->vid, NULL) ? TRUE : FALSE
   ;  double num = 0.0
   ;  dim sz = vec->n_ivps
   ;  dim sz_eff = sz - (loopy && has_loop ? 1 : 0)

   ;  for (j=0; j<sz; j++)
      {  long id = vec->ivps[j].idx
      ;  dim meet
      ;  if (id == vec->vid)
         continue
      ;  vec2 = mclxGetVector(mx, id, RETURN_ON_FAIL, vec2)
      ;  mcldCountParts(vec2, vec, NULL, &meet, NULL)
      ;  num += meet
   ;  }

      if (num && has_loop)
      {  dim m = 0
      ;  double delta
      ;  mcldCountParts(vec, has_loop, NULL, &m, NULL)
      ;  delta =     m  -  (loopy ? 1.0 : 0.0)  /* /_\ with loopy neighbours */
                  +  sz_eff * (loopy ? 1.0 : 0.0)   /* /_\ with loopy self   */

      ;  if (delta <= num+0.5)     /* finite precision precaution */
         num -= delta
   ;  }

      if (sz_eff > 1)
      num /=  sz_eff * (sz_eff - 1)
   ;  return num
;  }


dim mclgEcc
(  mclv*       vec
,  mclx*       mx
)
   {  return mclgEcc2(vec, mx, mx->dom_rows)
;  }


dim mclgEcc2
(  mclv*          vec
,  const mclx*    mx
,  mclv*          scratch
)
   {  mclv* wave1 =  mclvInsertIdx(NULL, vec->vid, 1.0), *wave2
   ;  dim ecc = 0

   ;  mclgUnionvInitNode2(scratch, vec->vid)

   ;  while (1)
      {  wave2 = mclgUnionv2(mx, wave1, NULL, SCRATCH_UPDATE, NULL, scratch)
      ;  mclvFree(&wave1)
      ;  wave1 = wave2
      ;  if (!wave1->n_ivps)
         break
      ;  ecc++
   ;  }

      mclvFree(&wave1)
   ;  mclgUnionvReset2(scratch)
   ;  return ecc
;  }


