/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "matrix.h"
#include "io.h"

#include "util/compile.h"
#include "util/alloc.h"
#include "util/types.h"
#include "util/err.h"
#include "util/io.h"


/* helper function */
/* should distinguish error condition */

static mcxbool idMap
(  mclx  *map
)
   {  int i
   ;  for (i=0;i<N_COLS(map);i++)
      {  if (map->cols[i].n_ivps != 1)
         return FALSE
      ;  if (map->cols[i].ivps[0].idx != map->dom_cols->ivps[i].idx)
         return FALSE
   ;  }
      return TRUE
;  }


/* helper function */

static mclVector* pmt_vector
(  mclv  *dom
,  mclx  *map
,  mclpAR** ar_dompp
)
   {  mclpAR*  ar_dom = NULL
   ;  mclv*    new_dom_cols = NULL
   ;  mcxstatus status = STATUS_FAIL
   ;  int i
   ;  *ar_dompp = NULL

   ;  while (1)
      {  if (dom->n_ivps != map->dom_cols->n_ivps)
         {  mcxErr
            (  "mclxMapCheck"
            ,  "domains do not match (dom size %ld, map size %ld)"
            ,  (long) dom->n_ivps
            ,  (long) map->dom_cols->n_ivps
            )
         ;  break
      ;  }
         ar_dom = mclpARensure(NULL, dom->n_ivps)

      ;  for (i=0;i<N_COLS(map);i++)
         {  if (map->cols[i].n_ivps != 1)
            {  mcxErr("mclxMapCheck", "not a mapping matrix")
            ;  break
         ;  }
            ar_dom->ivps[i].idx = map->cols[i].ivps[0].idx
         ;  ar_dom->n_ivps++
      ;  }
         if (i != N_COLS(map))
         break

      ;  new_dom_cols = mclvFromIvps(NULL, ar_dom->ivps, ar_dom->n_ivps)

      ;  if (new_dom_cols->n_ivps != ar_dom->n_ivps)
         {  mcxErr("mclxMapCheck", "map is not bijective")
         ;  break
      ;  }
         *ar_dompp = ar_dom
      ;  status = STATUS_OK
      ;  break
   ;  }

      if (status)
      {  mclvFree(&new_dom_cols)
      ;  mclpARfree(&ar_dom)
   ;  }
      return new_dom_cols
;  }


mcxstatus mclxMapCols
(  mclMatrix  *mx
,  mclMatrix  *map
)
   {  mclVector* new_dom_cols = NULL
   ;  mclpAR     *ar_dom = NULL
   ;  int i

   ;  if (map && idMap(map))
      return STATUS_OK

   ;  if (map)
      {  if (!mcldEquate(mx->dom_cols, map->dom_cols, MCLD_EQ_EQUAL))
         {  mcxErr("mclxMapCols", "domains not equal")
         ;  return STATUS_FAIL
      ;  }
         if (!(new_dom_cols = pmt_vector(mx->dom_cols, map, &ar_dom)))
         return STATUS_FAIL
   ;  }
      else
      new_dom_cols = mclvCanonical(NULL, N_COLS(mx), 1.0)

   ;  for (i=0; i<N_COLS(mx); i++)
      mx->cols[i].vid = ar_dom ? ar_dom->ivps[i].idx : i

   ;  qsort(mx->cols, N_COLS(mx), sizeof(mclVector), mclvVidCmp)

   ;  mclvFree(&(mx->dom_cols))
   ;  mx->dom_cols = new_dom_cols
   ;  mclpARfree(&ar_dom)

   ;  return STATUS_OK
;  }


mcxstatus  mclxMapRows
(  mclMatrix  *mx
,  mclMatrix  *map
)
   {  mclVector* new_dom_rows
   ;  mclVector* vec = mx->cols
   ;  mclpAR* ar_dom = NULL

   ;  if (map && idMap(map))
      return STATUS_OK

   ;  if (map)
      {  if (!mcldEquate(mx->dom_rows, map->dom_cols, MCLD_EQ_EQUAL))
         {  mcxErr("mclxMapRows", "domains not equal")
         ;  return STATUS_FAIL
      ;  }
         if (!(new_dom_rows = pmt_vector(mx->dom_rows, map, &ar_dom)))
         return STATUS_FAIL
   ;  }
      else
      new_dom_rows = mclvCanonical(NULL, N_COLS(mx), 1.0)

   ;  while (vec < mx->cols + N_COLS(mx))
      {  mclIvp* rowivp    =  vec->ivps
      ;  mclIvp* rowivpmax =  rowivp + vec->n_ivps
      ;  int offset = -1
      
      ;  while (rowivp < rowivpmax)
         {  offset  =  mclvGetIvpOffset(mx->dom_rows, rowivp->idx, offset)
         ;  if (offset < 0)
               mcxErr
               (  "mclxMapRows PANIC"
               ,  "index <%ld> not in domain for <%ldx%ld> matrix"
               ,  (long) rowivp->idx
               ,  (long) N_COLS(mx)
               ,  N_ROWS(mx)
               )
            ,  mcxExit(1)
         ;  else
            rowivp->idx = ar_dom ? ar_dom->ivps[offset].idx : offset
         ;  rowivp++
      ;  }
         mclvSort(vec, mclpIdxCmp)
      ;  vec++
   ;  }
      
      mclvFree(&(mx->dom_rows))
   ;  mclpARfree(&ar_dom)
   ;  mx->dom_rows = new_dom_rows
   ;  return STATUS_OK
;  }


void mclxInflate
(  mclMatrix*   mx
,  double       power
)
   {  mclVector*     vecPtr          =     mx->cols
   ;  mclVector*     vecPtrMax       =     vecPtr + N_COLS(mx)

   ;  while (vecPtr < vecPtrMax)
      {  mclvInflate(vecPtr, power)
      ;  vecPtr++
   ;  }
   }


mclx* mclxAllocClone
(  const mclx* mx
)
   {  mclv* dom_cols, *dom_rows
   ;  if (!mx)
      {  mcxErr("mclxAllocClone PBD", "void matrix argument")
      ;  return NULL
   ;  }

      dom_cols = mclvClone(mx->dom_cols)
   ;  dom_rows = mclvClone(mx->dom_rows)

   ;  if (!dom_cols || !dom_rows)
      return NULL

   ;  return mclxAllocZero(dom_cols, dom_rows)
;  }



mclMatrix* mclxAllocZero
(  mclVector * dom_cols
,  mclVector * dom_rows
)
   {  int i, n_cols
   ;  mclMatrix *dst
   ;  const char* me = "mclxAllocZero"

   ;  if (!dom_cols || !dom_rows)
      {  mcxErr(me, "got NULL arguments (allocation error?)")
      ;  return NULL
   ;  }

      n_cols  = dom_cols->n_ivps
   
   ;  if (!(dst = mcxAlloc(sizeof(mclMatrix), RETURN_ON_FAIL)))
      return NULL

   ;  if
      (! (dst->cols = mcxAlloc (n_cols * sizeof(mclVector), RETURN_ON_FAIL))
      && n_cols
      )
      {  mcxMemDenied(stderr, me, "mclVector", n_cols)
      ;  return NULL
   ;  }

      dst->dom_cols  =  dom_cols
   ;  dst->dom_rows  =  dom_rows

   ;  for (i=0; i<n_cols; i++)
      {  mclv* col   =  dst->cols+i
      ;  col->vid    =  dom_cols->ivps[i].idx
      ;  col->ivps   =  NULL
      ;  col->val    =  0.0
      ;  col->n_ivps =  0
   ;  }

      return dst
;  }


mclMatrix* mclxCartesian
(  mclVector*     dom_cols
,  mclVector*     dom_rows
,  double         val
)
   {  int i
   ;  mclMatrix*  rect  =  mclxAllocZero(dom_cols, dom_rows)

   ;  for(i=0;i<N_COLS(rect);i++)
      {  mclvCopy(rect->cols+i, dom_rows)
      ;  mclvMakeConstant(rect->cols+i, val)
   ;  }
      return rect
;  }


mcxstatus meet_the_joneses
(  mclx* dst
,  const mclx* src
,  const mclv* col_segment    /* pick these columns from src */
,  const mclv* row_segment    /* and these rows              */
)
   {  mclp* domivp     =  col_segment->ivps
   ;  mclp* domivpmax  =  domivp+col_segment->n_ivps
   ;  mclv* dstvec = NULL, *srcvec = NULL

   ;  while (domivp<domivpmax)
      {  dstvec  =  mclxGetVector(dst, domivp->idx, RETURN_ON_FAIL, dstvec)
      ;  srcvec  =  mclxGetVector(src, domivp->idx, RETURN_ON_FAIL, srcvec)

      ;  if (!dstvec)
         {  mcxErr("mclxSelect panic", "corruption in submatrix")
         ;  return STATUS_FAIL
      ;  }

         if (srcvec)
         {  mcldMeet(srcvec, row_segment, dstvec)
         ;  srcvec++
      ;  }
         domivp++
      ;  dstvec++
   ;  }
      return STATUS_OK
;  }


mclMatrix*  mclxExtSub
(  const mclMatrix*  mx
,  const mclVector*  col_select
,  const mclVector*  row_select
)
   {  mclv *new_dom_cols, *new_dom_rows, *colc_select = NULL
   ;  mcxstatus status = STATUS_FAIL
   ;  mclx* sub = NULL

   ;  if (!col_select)
      col_select = mx->dom_cols
   ;  if (!row_select)
      row_select = mx->dom_rows

   ;  colc_select = mcldMinus(mx->dom_cols, col_select, NULL)

   ;  new_dom_cols = mclvClone(mx->dom_cols)
   ;  new_dom_rows = mclvClone(mx->dom_rows)

   ;  if (!(sub = mclxAllocZero(new_dom_cols, new_dom_rows)))
      return NULL

   ;  while (1)
      {  if (meet_the_joneses(sub, mx, colc_select, row_select))
         break

      ;  if (meet_the_joneses(sub, mx, col_select, new_dom_rows))
         break

      ;  status = STATUS_OK
      ;  break
   ;  }

      mclvFree(&colc_select)

   ;  if (status)
      mclxFree(&sub)

   ;  return sub
;  }



mclMatrix*  mclxSub
(  const mclMatrix*  mx
,  const mclVector*  col_select
,  const mclVector*  row_select
)
   {  mclVector *new_dom_cols, *new_dom_rows
   ;  mclMatrix*  sub = NULL

   ;  if (!col_select)
      col_select = mx->dom_cols
   ;  if (!row_select)
      row_select = mx->dom_rows

   ;  new_dom_cols = mclvClone(col_select)
   ;  new_dom_rows = mclvClone(row_select)

   ;  if (!(sub = mclxAllocZero(new_dom_cols, new_dom_rows)))
      return NULL

   ;  if (meet_the_joneses(sub, mx, new_dom_cols, new_dom_rows))
      mclxFree(&sub)

   ;  return sub
;  }


double mclxSelectValues
(  mclMatrix*  mx
,  double*     lft
,  double*     rgt
,  mcxbits     equate
)
   {  long c
   ;  double sum = 0.0
   ;  for (c=0;c<N_COLS(mx);c++)
      sum += mclvSelectValues(mx->cols+c, lft, rgt, equate, mx->cols+c)
   ;  return sum
;  }


mclMatrix* mclxConstDiag
(  mclVector* vec
,  double c
)
   {  mclMatrix*  m = mclxDiag(vec)
   ;  mclxUnary(m, fltxConst, &c)
   ;  return m
;  }


void mclxScaleDiag
(  mclx* mx
,  double fac
)
   {  int i
   ;  for(i=0;i<N_COLS(mx);i++)
      {  mclv* vec = mx->cols+i
      ;  mclp* self = mclvGetIvp(vec, vec->vid, NULL)
      ;  if (self)
         self->val *= fac
   ;  }
   }


mclMatrix* mclxDiag
(  mclVector* vec
)
   {  mclMatrix* mx = mclxAllocZero(vec, mclvCopy(NULL, vec))
   ;  int i

   ;  if (!mx)
      return NULL

   ;  for(i=0;i<N_COLS(mx);i++)
      mclvInsertIdx(mx->cols+i, vec->ivps[i].idx, vec->ivps[i].val)
      /* fixme; this might fail ... */
   ;  return mx
;  }


mclMatrix* mclxCopy
(  const mclMatrix*     src
)
#if 0                              /* more efficiency checking */
   {  int         n_cols  =   N_COLS(src)
   ;  mclMatrix*  dst     =   mclxAllocZero
                              (  mclvCopy(NULL, src->dom_cols)
                              ,  mclvCopy(NULL, src->dom_rows)
                              )
   ;  const mclVector* src_vec =  src->cols
   ;  mclVector* dst_vec  =  dst->cols

   ;  while (--n_cols >= 0)
      {  if (!mclvRenew(dst_vec, src_vec->ivps, src_vec->n_ivps))
         {  mclxFree(&dst)
         ;  break
      ;  }
         src_vec++
      ;  dst_vec++
   ;  }
      return dst
;  }
#else                               /* pbb sufficiently efficient */
   {  return mclxSub(src, NULL, NULL)
;  }
#endif


void mclxFree
(  mclMatrix**             mxpp
)
   {  mclMatrix* mx = *mxpp
   ;  if (mx)
      {  mclVector*  vec      =  mx->cols
      ;  int         n_cols   =  N_COLS(mx)

      ;  while (--n_cols >= 0)
         {  mcxFree(vec->ivps)
         ;  vec++
      ;  }

         mclvFree(&(mx->dom_rows))
      ;  mclvFree(&(mx->dom_cols))

      ;  mcxFree(mx->cols)
      ;  mcxFree(mx)

      ;  *mxpp = NULL
   ;  }
   }


void mclxMakeStochastic
(  mclMatrix* mx
)  
   {  mclVector* vecPtr    =  mx->cols
   ;  mclVector* vecPtrMax =  vecPtr + N_COLS(mx)

   ;  while (vecPtr < vecPtrMax)
         mclvNormalize(vecPtr)
      ,  vecPtr++
;  }


void mclxMakeSparse
(  mclMatrix* m
,  int        min
,  int        max
)
   {  int  n_cols    =  N_COLS(m)
   ;  mclVector* vec =  m->cols

   ;  while (--n_cols >= 0)
      {  if (max && max < vec->n_ivps)
         mclvSelectHighest(vec, max)
      ;  else if (min && vec->n_ivps < min)
         mclvResize(vec, 0)
      ;  ++vec
   ;  }
   }


void mclxUnary
(  mclMatrix*  src
,  double (*op)(pval, void*)
,  void* arg
)
   {  int         n_cols =  N_COLS(src)
   ;  mclVector*  vec    =  src->cols

   ;  while (--n_cols >= 0)
         mclvUnary(vec, op, arg)
      ,  vec++
;  }


void mclxAccomodate
(  mclx* mx
,  const mclv* dom_cols
,  const mclv* dom_rows
)
   {  if (!mcldEquate(mx->dom_cols, dom_cols, MCLD_EQ_SUPER))
      mclxChangeCDomain(mx, mcldMerge(mx->dom_cols, dom_cols, NULL))
   ;  if (!mcldEquate(mx->dom_rows, dom_rows, MCLD_EQ_SUPER))
      mclxChangeRDomain(mx, mcldMerge(mx->dom_rows, dom_rows, NULL))
;  }



void mclxChangeDomains
(  mclx* mx
,  mclv* dom_cols
,  mclv* dom_rows
)
   {  if (dom_cols)
      mclxChangeCDomain(mx, dom_cols)
   ;  if (dom_rows)
      mclxChangeRDomain(mx, dom_rows)
;  }


void mclxChangeRDomain
(  mclx* mx
,  mclv* domain
)
   {  int i

   ;  if (mcldEquate(mx->dom_rows, domain, MCLD_EQ_LDIFF))
      {  for (i=0;i<N_COLS(mx);i++)
         if (mcldEquate(mx->cols+i, domain, MCLD_EQ_LDIFF))
         mcldMeet(mx->cols+i, domain, mx->cols+i)
   ;  }
      mclvFree(&(mx->dom_rows))
   ;  mx->dom_rows = domain
;  }


void mclxChangeCDomain
(  mclx* mx
,  mclv* domain      /* fixme; check consistency, increasing order */
)
   {  int i
   ;  mclv* new_cols
   ;  mclv* cvec = mx->cols

   ;  if (mcldEquate(mx->dom_cols, domain, MCLD_EQ_EQUAL))
      {  mclvFree(&domain)
      ;  return
   ;  }

      new_cols =  mcxAlloc
                  (  domain->n_ivps * sizeof(mclVector)
                  ,  RETURN_ON_FAIL
                  )
   ;  if (!new_cols && domain->n_ivps)
         mcxMemDenied
         (stderr, "mclxChangeCDomain", "mclVector", domain->n_ivps)
      ,  mcxExit(1)

   ;  for (i=0;i<domain->n_ivps;i++)
      {  mclv* newcol = new_cols+i
      ;  long vid = domain->ivps[i].idx
      ;  cvec  = mclxGetVector(mx, vid, RETURN_ON_FAIL, cvec)

      ;  newcol->vid = vid
      ;  newcol->val = 0.0

      ;  if (cvec)
         {  newcol->ivps   =  cvec->ivps
         ;  newcol->n_ivps =  cvec->n_ivps
         ;  cvec->ivps     =  NULL
         ;  cvec->n_ivps   =  0
         ;  cvec++
      ;  }
         else
         {  newcol->ivps   =  NULL
         ;  newcol->n_ivps =  0
      ;  }
      }

   ;  for (i=0;i<N_COLS(mx);i++)
      mclvRelease(mx->cols+i)
   ;  mcxFree(mx->cols)

   ;  mx->cols = new_cols

   ;  mclvFree(&(mx->dom_cols))
   ;  mx->dom_cols = domain
;  }


/* implementation note:
 *    This may suggest mclvTernary(v1, v2, v3, f, g)
 *    Write f(v1[x], v2[x]) if g(v2[x], v3[x]).
 *    That would get rid of the temporary vector in the inner loop.
 *
 *    How useful would mclvTernary be otherwise?
*/

mclMatrix*  mclxBlocks2
(  const mclMatrix*     mx       /* fixme; check domain equality ? */
,  const mclMatrix*     domain
)
   {  int i
   ;  mclx* blocks  =   mclxAllocZero
                        (  mclvCopy(NULL, domain->dom_rows)
                        ,  mclvCopy(NULL, domain->dom_rows)
                        )

   ;  for (i=0;i<N_COLS(domain);i++)
      {  mclv* dom = domain->cols+i
      ;  long  ofs = -1
      ;  int j
      ;  for (j=0;j<dom->n_ivps;j++)
         {  long idx =  dom->ivps[j].idx
         ;  mclv* meet
         ;  ofs = mclvGetIvpOffset(mx->dom_cols, idx, ofs)
         ;  if (ofs < 0)
            continue
         ;  meet = mcldMeet(mx->cols+ofs, dom, NULL)
         ;  mclvBinary(blocks->cols+ofs, meet, blocks->cols+ofs, fltLoR)
         ;  mclvFree(&meet)
      ;  }
      }
      return blocks
;  }



mclMatrix*  mclxBlocks
(  const mclMatrix*     mx       /* fixme; check domain equality ? */
,  const mclMatrix*     domain
)
   {  int i
   ;  mclx* blocks  =   mclxAllocZero
                        (  mclvCopy(NULL, domain->dom_rows)
                        ,  mclvCopy(NULL, domain->dom_rows)
                        )

   ;  for (i=0;i<N_COLS(domain);i++)
      {  mclv* dom = domain->cols+i
      ;  if (dom->n_ivps)
         {  mclx* sub = mclxSub(mx, dom, dom)
         ;  mclxMerge(blocks, sub, fltLoR)
         ;  mclxFree(&sub)
      ;  }
      }
      return blocks
;  }



/* TODO: allow m1 = NULL
 *    equate can be inefficient for block selection (mclxblock).
 * WARNING:
 *    does not check domains. use mclxAccomodate if necessary.
*/

mclMatrix* mclxMerge
(  mclMatrix* m1
,  const mclMatrix* m2
,  double  (*op)(pval, pval)
)
   {  mclv *m1vec = m1->cols
   ;  int i

   ;  for (i=0;i<N_COLS(m2);i++)
      {  mclv *m2vec = m2->cols+i

      ;  if (!(m1vec = mclxGetVector(m1, m2vec->vid, RETURN_ON_FAIL, m1vec)))
         continue

      ;  if (!mclvBinary(m1vec, m2vec, m1vec, op))
         break    /* fixme; should err, pbb not free */
   ;  }

      return m1
;  }


mclMatrix* mclxBinary
(  const mclMatrix* m1
,  const mclMatrix* m2
,  double  (*op)(pval, pval)
)
   {  mclVector *dom_rows     =  mcldMerge
                                 (  m1->dom_rows
                                 ,  m2->dom_rows
                                 ,  NULL
                                 )
   ;  mclVector *dom_cols     =  mcldMerge
                                 (  m1->dom_cols
                                 ,  m2->dom_cols
                                 ,  NULL
                                 )
   ;  mclMatrix*  m3          =  mclxAllocZero(dom_cols, dom_rows)
   ;  mclVector  *dstvec      =  m3->cols 
   ;  mclVector  *m1vec       =  m1->cols
   ;  mclVector  *m2vec       =  m2->cols
   ;  mclVector  empvec

   ;  mclvInit(&empvec)

   ;  while (dstvec < m3->cols + N_COLS(m3))
      {  m1vec = mclxGetVector(m1, dstvec->vid, RETURN_ON_FAIL, m1vec)
      ;  m2vec = mclxGetVector(m2, dstvec->vid, RETURN_ON_FAIL, m2vec)

      ;  if
         (  !mclvBinary
            (  m1vec ? m1vec : &empvec
            ,  m2vec ? m2vec : &empvec
            ,  dstvec
            ,  op
            )
         )
         {  mclxFree(&m3)
         ;  break
      ;  }
         dstvec++
      ;  if (m1vec)
         m1vec++
      ;  if (m2vec)
         m2vec++
   ;  }

      return m3
;  }


int mclxGetVectorOffset
(  const mclMatrix* mx
,  long  vid
,  mcxOnFail ON_FAIL
,  long  offset
)
   {  mclVector* vec =  mclxGetVector
                        (  mx
                        ,  vid
                        ,  ON_FAIL
                        ,  offset > 0 ? mx->cols+offset : NULL
                        )
   ;  return vec ? vec - mx->cols : -1
;  }


mclVector* mclxGetNextVector
(  const mclMatrix* mx
,  long   vid
,  mcxOnFail ON_FAIL
,  const mclVector* offset
)
   {  const mclVector* max =  mx->cols + N_COLS(mx)

   ;  if (!offset)
      offset = mx->cols

   ;  while (offset < max)
      {  if (offset->vid >= vid)
         break
      ;  else
         offset++
   ;  }
      if (offset >= max || offset->vid > vid)
      {  if (ON_FAIL == RETURN_ON_FAIL)
         return NULL
      ;  else
            mcxErr
            (  "mclxGetNextVector PBD"
            ,  "did not find vector <%ld> in <%ld,%ld> matrix"
            ,  (long) vid
            ,  N_COLS(mx)
            ,  N_ROWS(mx)
            )
         ,  mcxExit(1)
   ;  }
      else
      return (mclVector*) offset
   ;  return NULL
;  }


mclVector* mclxGetVector
(  const mclMatrix* mx
,  long   vid
,  mcxOnFail ON_FAIL
,  const mclVector* offset
)
   {  long n_cols = N_COLS(mx)
   ;  mclVector* found = NULL

   ;  if
      (  !N_COLS(mx)
      || vid < 0
      || vid > mx->cols[n_cols-1].vid
      )
      found = NULL
   ;  else if (mx->cols[0].vid == 0 && mx->cols[n_cols-1].vid == n_cols-1)
      {  if (mx->cols[vid].vid == vid)
         found = mx->cols+vid
      ;  else
         found = NULL
   ;  }
      else if (offset && offset->vid == vid)
      found = (mclVector*) offset      /* const riddance */
   ;  else
      {  mclVector keyvec
      ;  mclvInit(&keyvec)
      ;  keyvec.vid = vid

      ;  if (!offset)
         offset = mx->cols

      ;  n_cols -= (offset - mx->cols)
      ;  found =  bsearch
                  (  &keyvec
                  ,  offset
                  ,  n_cols
                  ,  sizeof(mclVector)
                  ,  mclvVidCmp
                  )
   ;  }

      if (!found && ON_FAIL == EXIT_ON_FAIL)
         mcxErr
         (  "mclxGetVector PBD"
         ,  "did not find vector <%ld> in <%ld,%ld> matrix"
         ,  (long) vid
         ,  (long) N_COLS(mx)
         ,  (long) N_ROWS(mx)
         )
      ,  mcxExit(1)

   ;  return found
;  }


mclx* mclxMakeMap
(  mclVector*  dom_cols
,  mclVector*  new_dom_cols
)
   {  mclx* mx
   ;  int i

   ;  if (dom_cols->n_ivps != new_dom_cols->n_ivps)
      return NULL

   ;  mx = mclxAllocZero(dom_cols, new_dom_cols)

   ;  for (i=0;i<N_COLS(mx);i++)
      mclvInsertIdx(mx->cols+i, new_dom_cols->ivps[i].idx, 1.0)

   ;  return mx
;  }


mclMatrix* mclxTranspose
(  const mclMatrix*  src
)
   {  mclMatrix*   dst  =  mclxAllocZero
                           (  mclvCopy(NULL, src->dom_rows)
                           ,  mclvCopy(NULL, src->dom_cols)
                           )
   ;  const mclVector*  src_vec  =  src->cols
   ;  int               vec_ind  =  N_COLS(src)
   ;  mclVector*        dst_vec
   ;

      /*
       * Pre-calculate sizes of destination columns
       * fixme; if canonical domains do away with mclxGetVector.
      */
      while (--vec_ind >= 0)
      {  int   src_n_ivps  =  src_vec->n_ivps
      ;  mclIvp*  src_ivp  =  src_vec->ivps
      ;  dst_vec           =  dst->cols

      ;  while (--src_n_ivps >= 0)
         {  dst_vec = mclxGetVector(dst, src_ivp->idx, EXIT_ON_FAIL, dst_vec)
         ;  dst_vec->n_ivps++
         ;  src_ivp++
         ;  dst_vec++   /* with luck we get immediate hit */
      ;  }
         src_vec++
   ;  }

      /*
       * Allocate
      */
      dst_vec     =  dst->cols
   ;  vec_ind     =  N_COLS(dst)
   ;  while (--vec_ind >= 0)
      {  if (!mclvResize(dst_vec, dst_vec->n_ivps))
         {  mclxFree(&dst)
         ;  return 0
      ;  }
         dst_vec->n_ivps = 0    /* dirty: start over for write */
      ;  dst_vec++
   ;  }

      /*
       * Write
       *
      */
      src_vec     =  src->cols
   ;  while (src_vec < src->cols+N_COLS(src))
      {  int   src_n_ivps  =  src_vec->n_ivps
      ;  mclIvp* src_ivp   =  src_vec->ivps
      ;  dst_vec           =  dst->cols

      ;  while (--src_n_ivps >= 0)
         {  dst_vec = mclxGetVector(dst, src_ivp->idx, EXIT_ON_FAIL, dst_vec)
         ;  dst_vec->ivps[dst_vec->n_ivps].idx = src_vec->vid
         ;  dst_vec->ivps[dst_vec->n_ivps].val = src_ivp->val
         ;  dst_vec->n_ivps++
         ;  dst_vec++
         ;  src_ivp++
      ;  }
         src_vec++
   ;  }

      return dst
;  }


mclVector* mclxColNums
(  const mclMatrix*  m
,  double           (*f)(const mclVector * vec)
,  mcxenum           mode  
)
   {  mclVector*  nums = mclvClone(m->dom_cols)
   ;  int vec_ind =  0
   
   ;  if (nums)
      {  while (vec_ind < N_COLS(m))
         {  nums->ivps[vec_ind].val = f(m->cols + vec_ind)
         ;  vec_ind++
      ;  }
      }
      if (mode == MCL_VECTOR_SPARSE)
      mclvUnary(nums, fltxCopy, NULL)

   ;  return nums
;  }


mclVector* mclxDiagValues
(  const mclMatrix*  m
,  mcxenum     mode  
)
   {  return mclxColNums(m, mclvSelf, mode)
;  }


mclVector* mclxColSums
(  const mclMatrix*  m
,  mcxenum     mode  
)
   {  return mclxColNums(m, mclvSum, mode)
;  }


mclVector* mclxColSizes
(  const mclMatrix*     m
,  mcxenum        mode
)
   {  return mclxColNums(m, mclvSize, mode)
;  }


double mclxMass
(  const mclMatrix*     m
)
   {  int   c
   ;  double  mass  =  0
   ;  for (c=0;c<N_COLS(m);c++)
      mass += mclvSum(m->cols+c)
   ;  return mass
;  }


long mclxNrofEntries
(  const mclMatrix*     m
)
   {  int  c
   ;  long nr =  0
   ;  for (c=0;c<N_COLS(m);c++)
      nr += (m->cols+c)->n_ivps
   ;  return nr
;  }


void  mclxColumnsRealign
(  mclMatrix* m
,  int (*cmp)(const void* vec1, const void* vec2)
)
   {  int i
   ;  qsort(m->cols, N_COLS(m), sizeof(mclVector), cmp)
   ;  for (i=0;i<m->dom_cols->n_ivps;i++)
      m->cols[i].vid = m->dom_cols->ivps[i].idx
;  }


size_t mclxNEntries
(  const mclMatrix*     mx
)
   {  int i
   ;  size_t n = 0
   ;  for (i=0;i<N_COLS(mx);i++)
      n += mx->cols[i].n_ivps
   ;  return n
;  }


double mclxMaxValue
(  const mclMatrix*        mx
) 
   {  double max_val  =  0.0
   ;  mclxUnary((mclMatrix*)mx, fltxPropagateMax, &max_val)
   ;  return max_val
;  }


mclMatrix* mclxIdentity
(  mclVector* vec
)  
   {  return mclxConstDiag(vec, 1.0)
;  }


void mclxMakeCharacteristic
(  mclMatrix*              mx
)  
   {  double one  =  1.0
   ;  mclxUnary(mx, fltxConst, &one)
;  }


mclMatrix* mclxMax
(  const mclMatrix*        m1
,  const mclMatrix*        m2
)  
   {  return mclxBinary(m1, m2, fltMax)
;  }


mclMatrix* mclxMinus
(  const mclMatrix*        m1
,  const mclMatrix*        m2
)  
   {  return mclxBinary(m1, m2, fltSubtract)
;  }


mclMatrix* mclxAdd
(  const mclMatrix*        m1
,  const mclMatrix*        m2
)  
   {  return mclxBinary(m1, m2, fltAdd)
;  }


void mclxAddTranspose
(  mclx* mx
,  double diagweight
)
   {  long c
   ;  mclx* mxt = mclxTranspose(mx)
   ;  mclv* mvec = NULL

   ;  mclxChangeDomains
      (  mx
      ,  mcldMerge(mx->dom_cols, mxt->dom_cols, NULL)
      ,  mcldMerge(mx->dom_rows, mxt->dom_rows, NULL)
      )

   ;  for (c=0;c<N_COLS(mxt);c++)
      {  long vid = mxt->dom_cols->ivps[c].idx
      ;  mvec = mclxGetVector(mx, vid, RETURN_ON_FAIL, mvec)
      ;  if (!mvec)
         {  mcxErr("mclxAddTranspose panic", "no vector %ld in matrix", vid)
         ;  continue
      ;  }
         mclvAdd(mvec, mxt->cols+c, mvec)
      ;  mclvRelease(mxt->cols+c)
   ;  }

      if (diagweight != 1.0)
      mclxScaleDiag(mx, diagweight)
   ;  mclxFree(&mxt)
;  }



mclMatrix* mclxHadamard
(  const mclMatrix*        m1
,  const mclMatrix*        m2
)
   {  return mclxBinary(m1, m2, fltMultiply)
;  }



#if 0
void mclxCenter
(  mclMatrix*        mx
,  double            w_self
,  double            o_ctrmax
)
   {  long cct

   ;  for (cct=0;cct<N_COLS(mx);cct++)
      {  mclVector*  vec      =  mx->cols+cct
      ;  mclIvp*     match    =  NULL
      ;  int         offset   =  -1
      ;  double      maxval   =  mclvMaxValue(vec)

      ;  if (w_self > 0 && vec->n_ivps)
         {  mclvIdxVal(vec, vec->vid, &offset)
         ;  if (offset >= 0)
            {  match = (vec->ivps+offset)
            ;  match->val  =  0.0
         ;  }
            else                    /* create extra room in vector */
            {  mclvResize (vec, (vec->n_ivps)+1)
            ;  match       =  vec->ivps+(vec->n_ivps-1)
            ;  match->val  =  0.0
            ;  match->idx  =  vec->vid
            ;  mclvSort (vec, mclpIdxCmp)
                             /* fixme ^^^ this could be done by shifting */

            ;  mclvIdxVal(vec, vec->vid, &offset)

            ;  if (offset < 0)
                  mcxErr("mclxCenter", "error: insertion failed ?!?")
               ,  mcxExit(1)

            ;  match    =  (vec->ivps+offset)
         ;  }

            {  double sum =  mclvSum(vec)

            ;  if (sum > 0.0)
               {  double  selfinit
                  =     o_ctrmax * (mclvPowSum(vec, 2) / sum)
                     +  (1 - o_ctrmax) * maxval
               ;  match->val = w_self * selfinit
               ;  if (!match->val)
                  match->val =  1
            ;  }
               else
               match->val =  1
         ;  }
         }
         else if (!vec->n_ivps)
            mcxErr
            ("loop assignment", "adding loop to void column <%ld>", (long) cct)
         ,  mclvInsertIdx(vec, vec->vid, 1.0)
   ;  }
   }
#endif



void mclxAdjustLoops
(  mclx*    mx
,  double  (*op)(mclv* vec, long r, void* data)
,  void*    data
)
   {  long i
   ;  for (i=0;i<N_COLS(mx);i++)
      {  mclv*    vec   =  mx->cols+i
      ;  mclp*    ivp   =  mclvGetIvp(vec, vec->vid, NULL)
      ;  double   val   =  op(vec, vec->vid, data)

      ;  if (ivp && !val)
            ivp->val = 0.0
         ,  mclvUnary(vec, fltxCopy, NULL)
      ;  else if (ivp && val)
         ivp->val = val
      ;  else if (!ivp && val)
         mclvInsertIdx(vec, vec->vid, val)
   ;  }
   }


mclv* mclxUnionv
(  const mclx* mx
,  const mclv* dom
,  mclv* dst
)
   {  const mclVector* dvec = NULL
   ;  long x
   ;  if (!dst)
      dst = mclvInit(dst)
   ;  else
      mclvResize(dst, 0)

   ;  if (!dom)
      dom = mx->dom_cols

   ;  for (x=0;x<dom->n_ivps;x++)
      {  long idx = dom->ivps[x].idx

      ;  if (idx == -1)             /* DING DING DING ugly meltdown fixme */
         dvec = mx->dom_cols
      ;  else if (idx == -2)
         dvec = mx->dom_rows
      ;  else
         dvec = mclxGetVector(mx, idx, RETURN_ON_FAIL, dvec)

      ;  if (dvec)
         mcldMerge(dst, dvec, dst)

      ;  if (idx < 0)
         dvec = NULL
   ;  }
      return dst
;  }


void mclxWeed
(  mclx* mx
,  mcxbits bits
)
   {  mclv* colselect
      =     bits & (MCLX_WEED_COLS | MCLX_WEED_GRAPH)
         ?  mclxColNums(mx, mclvSize, MCL_VECTOR_SPARSE)
         :  NULL

   ;  mclv* rowselect                     /* fixme, cheaper way? */
      =     bits & (MCLX_WEED_ROWS | MCLX_WEED_GRAPH)
         ?  mclxUnionv(mx, NULL, NULL)
         :  NULL

   ;  if (bits & MCLX_WEED_GRAPH)
      {  mcldMerge(colselect, rowselect, colselect)
      ;  mclvCopy(rowselect, colselect)
   ;  }
      mclxChangeDomains(mx, colselect, rowselect)
;  }


long mclxUnaryList
(  mclx*    mx
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)
   {  int n_cols =  N_COLS(mx)
   ;  mclVector* vec = mx->cols
   ;  long n_entries_kept = 0

   ;  while (--n_cols >= 0)
         n_entries_kept += mclvUnaryList(vec, ar)
      ,  vec++
   ;  return n_entries_kept
;  }



/* remove those nodes in domain that cannot reach any
 * node in projection via graph
*/

void wave_prune_sideways
(  const mclx* graph
,  mclv* domain
,  const mclv* projection
)
   {  long i = 0
   ;  mclv* nb = NULL
   ;  for (i=0;i<domain->n_ivps;i++)
      {  nb = mclxGetVector(graph, domain->ivps[i].idx, RETURN_ON_FAIL, nb)
      ;  if (!nb || !mcldCountSet(nb,  projection, MCLD_CT_MEET))
         domain->ivps[i].val = 0.0
   ;  }
      mclvUnary(domain, fltxCopy, NULL)
;  }


mclv* wave_get_new
(  const mclv* wavez
,  const mclv* seenz
,  const mclx* graph
)
   {  mclv* nb = NULL
   ;  mclv* newpart = NULL
   ;  mclv* newz = mclvInit(NULL)
   ;  long i

   ;  for (i=0;i<wavez->n_ivps;i++)
      {  long zz = wavez->ivps[i].idx
      ;  nb = mclxGetVector(graph, zz, RETURN_ON_FAIL, nb)
      ;  if (!nb)
         {  mcxErr("mclxShortestPaths", "panic <%ld> not found", zz)
         ;  continue
      ;  }
         newpart = mcldMinus(nb, seenz, newpart)
      ;  newz = mcldMerge(newz, newpart, newz)
   ;  }
      mclvFree(&newpart)
   ;  return newz
;  }


mclx* waves_get_nodes
(  int            n_waves
,  mclv*          node_set       /* used as matrix row domain */
,  const mclv*    meet_in_the_middle
,  const mcxLink* firstx
,  const mcxLink* lasty
)
   {  const mcxLink* curr = firstx
   ;  mclx* track = NULL
   ;  int n = 0

   ;  track =
      mclxAllocZero
      (  mclvCanonical(NULL, n_waves, 1.0)
      ,  node_set
      )
      /* onwards: set the matrix columns */

   ;  while (curr && curr->val && n < n_waves)
      {  mclvCopy(track->cols+n++, curr->val)
      ;  curr = curr->next
   ;  }

      if (n < n_waves)
      mclvCopy(track->cols+n++, meet_in_the_middle)
   ;  curr = lasty

   ;  while (curr && curr->val && n < n_waves)
      {  mclvCopy(track->cols+n++, curr->val)
      ;  curr = curr->prev
   ;  }

      if (n != n_waves)
      mcxErr
      ("mclxShortestSimplePaths", "unexpected mismatch %d/%d", n, n_waves)

   ;  return track
;  }



mclv* mclxSSPxy
(  const mclx* graph
,  long x
,  long y
,  mclx** trackpp
)
   {  mcxLink* src
   ;  mcxLink* rootx, *rooty, *currx, *curry
   ;  mclv* wavex, *wavey, *seenx, *seeny, *meet_in_the_middle
   ;  int n_waves_x = 1, n_waves_y = 1
   ;  mclx* track = NULL
   ;  mclv* punters = NULL    /* nodes participating in SSPs */
   
   ;  if (x == y)       /* caller should cater */
      return NULL
                        /* fixme should pbb set ERANGE */
   ;  if (!mclxGetVector(graph, x, RETURN_ON_FAIL, NULL))
      return NULL
                        /* fixme should pbb set ERANGE */
   ;  if (!mclxGetVector(graph, y, RETURN_ON_FAIL, NULL))
      return NULL

   ;  src = mcxLinkNew(20, NULL, 0)

   ;  wavex =  mclvInsertIdx(NULL, x, 1.0)
   ;  wavey =  mclvInsertIdx(NULL, y, 1.0)

   ;  seenx =  mclvClone(wavex)
   ;  seeny =  mclvClone(wavey)

   ;  currx = rootx =  mcxLinkSpawn(src, wavex)
   ;  curry = rooty =  mcxLinkSpawn(src, wavey)

   ;  while (wavex->n_ivps + wavey->n_ivps)     /* redundant condition */
      {  mclv* wavex_new = wave_get_new(wavex, seenx, graph), *wavey_new
      ;  currx       =  mcxLinkAfter(currx, wavex_new)
      ;  wavex       =  wavex_new
      ;  if (!wavex_new->n_ivps || mcldCountSet(wavex_new, wavey, MCLD_CT_MEET))
         break
      ;  n_waves_x++
      ;  mcldMerge(seenx, wavex_new, seenx)

      ;  wavey_new   =  wave_get_new(wavey, seeny, graph)
      ;  curry       =  mcxLinkAfter(curry, wavey_new)
      ;  wavey       =  wavey_new
      ;  if (!wavey_new->n_ivps || mcldCountSet(wavey_new, wavex, MCLD_CT_MEET))
         break
      ;  n_waves_y++
      ;  mcldMerge(seeny, wavex_new, seeny)
   ;  }

      if (wavex->n_ivps && wavey->n_ivps)
      {  mcxLink* topx = currx
      ;  mcxLink* topy = curry
      ;  mclv* projection
            =  meet_in_the_middle
            =  mcldMeet(topx->val, topy->val, NULL)

      ;  punters = mclvClone(meet_in_the_middle)

      ;  topx->val = NULL
      ;  topy->val = NULL

      ;  while ((currx = currx->prev))
         {  wave_prune_sideways(graph, currx->val, projection)
               /* ^reduces currx->val  */
         ;  projection = currx->val
         ;  mcldMerge(punters, projection, punters)
      ;  }
         projection = meet_in_the_middle
      ;  while ((curry = curry->prev))
         {  wave_prune_sideways(graph, curry->val, projection)
               /* ^reduces curry->val  */
         ;  projection = curry->val
         ;  mcldMerge(punters, projection, punters)
      ;  }

         if (trackpp)
         {  track
            =  waves_get_nodes
               (  n_waves_x + n_waves_y
               ,  punters
               ,  meet_in_the_middle
               ,  rootx, topy->prev
               )
         ;  *trackpp = track
      ;  }
         mclvFree(&meet_in_the_middle)
   ;  }

      mcxLinkFree(&src, mclvFree_v)
   ;  mclvFree(&seenx)
   ;  mclvFree(&seeny)

   ;  return punters
;  }


mclv* mclxSSPd
(  const mclx* graph
,  const mclv* domain
)
   {  mclv* punters = mclvInit(NULL)
   ;  long i
   ;  for (i=0;i<domain->n_ivps;i++)
      {  long j
      ;  long x = domain->ivps[i].idx
      ;  for (j=i+1;j<domain->n_ivps;j++)
         {  long y = domain->ivps[j].idx
         ;  mclv* nodes = mclxSSPxy(graph, x, y, NULL)
         ;  if (nodes)
            mcldMerge(punters, nodes, punters)
         ;  mclvFree(&nodes)
      ;  }
   ;  }
      return punters
;  }


