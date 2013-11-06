/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "compose.h"
#include "matrix.h"
#include "io.h"

#include "util/compile.h"
#include "util/alloc.h"
#include "util/types.h"
#include "util/err.h"
#include "util/io.h"


/* helper function */
/* should distinguish error condition */

static mcxbool id_map
(  mclx  *map
)
   {  dim d
   ;  for (d=0;d<N_COLS(map);d++)
      {  if (map->cols[d].n_ivps != 1)
         return FALSE
      ;  if (map->cols[d].ivps[0].idx != map->dom_cols->ivps[d].idx)
         return FALSE
   ;  }
      return TRUE
;  }


mclv* mclxMapVectorPermute
(  mclv  *dom
,  mclx  *map
,  mclpAR** ar_dompp
)
   {  mclpAR* ar_dom       =  NULL
   ;  mclv* new_dom_cols   =  NULL
   ;  mcxstatus status     =  STATUS_FAIL
   ;  dim d
   ;  *ar_dompp            =  NULL

   ;  while (1)
      {  long ofs = -1
      ;  ar_dom = mclpARensure(NULL, dom->n_ivps)

      ;  for (d=0;d<dom->n_ivps;d++)
         {  if
            (  (  0
               >  (ofs = mclvGetIvpOffset(map->dom_cols, dom->ivps[d].idx, ofs))
               )
            || map->cols[ofs].n_ivps < 1
            )
            break
         ;  ar_dom->ivps[d].idx = map->cols[ofs].ivps[0].idx
         ;  ar_dom->n_ivps++
      ;  }
         if (d != dom->n_ivps)
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
      ;  mcxErr
         (  "mclxMapDomain"
         ,  "error occurred with %lux%lu map matrix"
         ,  (ulong) N_COLS(map)
         ,  (ulong) N_ROWS(map)
         )
   ;  }
      return new_dom_cols
;  }


mcxstatus mclxMapCols
(  mclx  *mx
,  mclx  *map
)
   {  mclv* new_dom_cols = NULL
   ;  mclpAR     *ar_dom = NULL
   ;  dim d

   ;  if (map && id_map(map))
      return STATUS_OK

   ;  if (map)
      {  if (!mcldEquate(mx->dom_cols, map->dom_cols, MCLD_EQT_SUB))
         {  mcxErr("mclxMapCols", "matrix domain not included in map domain")
         ;  return STATUS_FAIL
      ;  }
         if (!(new_dom_cols = mclxMapVectorPermute(mx->dom_cols, map, &ar_dom)))
         return STATUS_FAIL
   ;  }
      else
      new_dom_cols = mclvCanonical(NULL, N_COLS(mx), 1.0)

   ;  for (d=0; d<N_COLS(mx); d++)
      mx->cols[d].vid = ar_dom ? ar_dom->ivps[d].idx : d

   ;  if (ar_dom)
      qsort(mx->cols, N_COLS(mx), sizeof(mclv), mclvVidCmp)

   ;  mclvFree(&(mx->dom_cols))
   ;  mx->dom_cols = new_dom_cols
   ;  mclpARfree(&ar_dom)

   ;  return STATUS_OK
;  }


mcxstatus  mclxMapRows
(  mclx  *mx
,  mclx  *map
)
   {  mclv* new_dom_rows
   ;  mclv* vec = mx->cols
   ;  mclpAR* ar_dom = NULL
   ;  mcxbool canonical = mclxRowCanonical(mx)

   ;  if (map && id_map(map))
      return STATUS_OK

   ;  if (map)
      {  if (!mcldEquate(mx->dom_rows, map->dom_cols, MCLD_EQT_SUB))
         {  mcxErr("mclxMapRows", "matrix domain not included in map domain")
         ;  return STATUS_FAIL
      ;  }
         if (!(new_dom_rows = mclxMapVectorPermute(mx->dom_rows, map, &ar_dom)))
         return STATUS_FAIL
   ;  }
      else
      new_dom_rows = mclvCanonical(NULL, N_ROWS(mx), 1.0)

   ;  while (vec < mx->cols + N_COLS(mx))
      {  mclIvp* rowivp    =  vec->ivps
      ;  mclIvp* rowivpmax =  rowivp + vec->n_ivps
      ;  ofs offset = -1
      
      ;  while (rowivp < rowivpmax)
         {  offset  =   canonical
                     ?  rowivp->idx
                     :  mclvGetIvpOffset(mx->dom_rows, rowivp->idx, offset)
         ;  if (offset < 0)
               mcxErr
               (  "mclxMapRows PANIC"
               ,  "index <%lu> not in domain for <%lux%lu> matrix"
               ,  (ulong) rowivp->idx
               ,  (ulong) N_COLS(mx)
               ,  (ulong) N_ROWS(mx)
               )
            ,  mcxExit(1)
         ;  else
            rowivp->idx = ar_dom ? ar_dom->ivps[offset].idx : offset
         ;  rowivp++
      ;  }
         if (ar_dom)
         mclvSort(vec, mclpIdxCmp)
      ;  vec++
   ;  }
      
      mclvFree(&(mx->dom_rows))
   ;  mclpARfree(&ar_dom)
   ;  mx->dom_rows = new_dom_rows
   ;  return STATUS_OK
;  }


mcxbool mclxMapTest
(  mclx* map
)
   {  dim n_edges    =  mclxNrofEntries(map)
   ;  mclv* rowids   =     n_edges == N_COLS(map) && N_COLS(map) == N_ROWS(map)
                        ?  mclgUnionv(map, NULL, NULL, SCRATCH_READY, NULL)
                        :  NULL
   ;  mcxbool ok     =     rowids && rowids->n_ivps == N_COLS(map)
                        ?  TRUE
                        :  FALSE
   ;  if (rowids)
      mclvFree(&rowids)

   ;  return ok
;  }


void mclxInflate
(  mclx*   mx
,  double       power
)
   {  mclv*     vecPtr          =     mx->cols
   ;  mclv*     vecPtrMax       =     vecPtr + N_COLS(mx)

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



mclx* mclxAllocZero
(  mclv * dom_cols
,  mclv * dom_rows
)
   {  dim d, n_cols
   ;  mclx *dst
   ;  const char* me = "mclxAllocZero"

   ;  if (!dom_cols || !dom_rows)
      {  mcxErr(me, "got NULL arguments (allocation error?)")
      ;  return NULL
   ;  }

      n_cols  = dom_cols->n_ivps
   
   ;  if (!(dst = mcxAlloc(sizeof(mclx), ENQUIRE_ON_FAIL)))
      return NULL

   ;  if
      (! (dst->cols = mcxAlloc (n_cols * sizeof(mclv), ENQUIRE_ON_FAIL))
      && n_cols
      )
      {  mcxMemDenied(stderr, me, "mclv", n_cols)
      ;  return NULL
   ;  }

      dst->dom_cols  =  dom_cols
   ;  dst->dom_rows  =  dom_rows

   ;  for (d=0; d<n_cols; d++)
      {  mclv* col   =  dst->cols+d
      ;  col->vid    =  dom_cols->ivps[d].idx
      ;  col->ivps   =  NULL
      ;  col->val    =  0.0
      ;  col->n_ivps =  0
   ;  }

      return dst
;  }


mclx* mclxCartesian
(  mclv*     dom_cols
,  mclv*     dom_rows
,  double         val
)
   {  dim d
   ;  mclx*  rect  =  mclxAllocZero(dom_cols, dom_rows)

   ;  for(d=0;d<N_COLS(rect);d++)
      {  mclvCopy(rect->cols+d, dom_rows)
      ;  mclvMakeConstant(rect->cols+d, val)
   ;  }
      return rect
;  }


/* If row_segment == NULL columns will be empty.
 * If row_segment subsumes src->dom_rows columns are copied.
*/

static mcxstatus meet_the_joneses
(  mclx* dst
,  const mclx* src
,  const mclv* col_segment    /* pick these columns from src */
,  const mclv* row_segment    /* and these rows              */
)
   {  const mclv* col_select =  col_segment ? col_segment : src->dom_cols
   ;  mclp* domivp      =  col_select->ivps
   ;  mclp* domivpmax   =  domivp + col_select->n_ivps
   ;  mclv* dstvec      =  NULL, *srcvec = NULL

   ;  mcxbool copyrow   =  row_segment && MCLD_SUPER(row_segment, src->dom_rows)

   ;  while (domivp<domivpmax)
      {  dstvec  =  mclxGetVector(dst, domivp->idx, RETURN_ON_FAIL, dstvec)
      ;  srcvec  =  mclxGetVector(src, domivp->idx, RETURN_ON_FAIL, srcvec)

      ;  if (!dstvec)
         {  mcxErr("mclxSelect panic", "corruption in submatrix")
         ;  return STATUS_FAIL
      ;  }

         if (srcvec)
         {  if (copyrow)
            mclvCopy(dstvec, srcvec)
         ;  else if (row_segment)
            mcldMeet(srcvec, row_segment, dstvec)
         ;  srcvec++
      ;  }
         domivp++
      ;  dstvec++
   ;  }
      return STATUS_OK
;  }


mclx*  mclxExtSub
(  const mclx*  mx
,  const mclv*  col_select
,  const mclv*  row_select
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



mclx*  mclxSub
(  const mclx*  mx
,  const mclv*  col_select
,  const mclv*  row_select
)
   {  mclv *new_dom_cols, *new_dom_rows
   ;  mclx*  sub = NULL

   ;  new_dom_cols = col_select ? mclvClone(col_select) : mclvInit(NULL)
   ;  new_dom_rows = row_select ? mclvClone(row_select) : mclvInit(NULL)

   ;  if (!(sub = mclxAllocZero(new_dom_cols, new_dom_rows)))
      return NULL

   ;  if (meet_the_joneses(sub, mx, col_select, row_select))
      mclxFree(&sub)

   ;  return sub
;  }


dim mclxSelectLower
(  mclx*  mx
)
   {  dim d, n_entries = 0
   ;  for (d=0;d<N_COLS(mx);d++)
      n_entries
      +=    mclvSelectIdcs
            (mx->cols+d, NULL, &(mx->cols[d].vid), MCLX_EQT_LT, mx->cols+d)
   ;  return n_entries
;  }


dim mclxSelectUpper
(  mclx*  mx
)
   {  dim d, n_entries = 0
   ;  for (d=0;d<N_COLS(mx);d++)
      n_entries
      +=    mclvSelectIdcs
            (mx->cols+d, &(mx->cols[d].vid), NULL, MCLX_EQT_GT, mx->cols+d)
   ;  return n_entries
;  }


double mclxSelectValues
(  mclx*  mx
,  double*     lft
,  double*     rgt
,  mcxbits     equate
)
   {  dim d
   ;  double sum = 0.0
   ;  for (d=0;d<N_COLS(mx);d++)
      sum += mclvSelectValues(mx->cols+d, lft, rgt, equate, mx->cols+d)
   ;  return sum
;  }


mclx* mclxConstDiag
(  mclv* vec
,  double c
)
   {  mclx*  m = mclxDiag(vec)
   ;  mclxUnary(m, fltxConst, &c)
   ;  return m
;  }


void mclxScaleDiag
(  mclx* mx
,  double fac
)
   {  dim d
   ;  for(d=0;d<N_COLS(mx);d++)
      {  mclv* vec = mx->cols+d
      ;  mclp* self = mclvGetIvp(vec, vec->vid, NULL)
      ;  if (self)
         self->val *= fac
   ;  }
   }


mclx* mclxDiag
(  mclv* vec
)
   {  mclx* mx = mclxAllocZero(vec, mclvCopy(NULL, vec))
   ;  dim d

   ;  if (!mx)
      return NULL

   ;  for(d=0;d<N_COLS(mx);d++)
      mclvInsertIdx(mx->cols+d, vec->ivps[d].idx, vec->ivps[d].val)
      /* fixme; this might fail ... */
   ;  return mx
;  }


mclx* mclxCopy
(  const mclx*     src
)
                               /* pbb sufficiently efficient */
   {  return mclxSub(src, src->dom_cols, src->dom_rows)
;  }


void mclxFree
(  mclx**             mxpp
)
   {  mclx* mx = *mxpp
   ;  if (mx)
      {  mclv*  vec      =  mx->cols
      ;  dim         n_cols   =  N_COLS(mx)

      ;  while (n_cols-- > 0)    /* careful with unsignedness */
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
(  mclx* mx
)  
   {  mclv* vecPtr    =  mx->cols
   ;  mclv* vecPtrMax =  vecPtr + N_COLS(mx)

   ;  while (vecPtr < vecPtrMax)
         mclvNormalize(vecPtr)
      ,  vecPtr++
;  }



mclv* mclxColSelect
(  const mclx*  m
,  double          (*f_cb)(const mclv*, void*)
,  void*             arg_cb
)
   {  mclv*  sel = mclvClone(m->dom_cols)
   ;  dim i =  0
   
   ;  while (i < N_COLS(m))
      {  sel->ivps[i].val = f_cb(m->cols + i, arg_cb)
      ;  i++
   ;  }

      mclvUnary(sel, fltxCopy, NULL)
   ;  return sel
;  }


struct sparse_sel
{  dim sel_gq
;  dim sel_lq
;
}  ;


double sparse_sel_cb
(  const mclv* vec
,  void* data
)
   {  struct sparse_sel* sel = data
   ;  return
      (  (sel->sel_gq && vec->n_ivps < sel->sel_gq)
      || (sel->sel_lq && vec->n_ivps > sel->sel_lq)
      )
   ?  0.0 : 1
;  }


mclv* mclgMakeSparse
(  mclx* m
,  dim        sel_gq
,  dim        sel_lq
)
   {  struct sparse_sel values
   ;  mclv* sel   = NULL
   ;  mclp* p = NULL
   ;  dim d

   ;  values.sel_gq = sel_gq
   ;  values.sel_lq = sel_lq

   ;  sel = mclxColSelect(m, sparse_sel_cb, &values)

   ;  for (d=0;d<N_COLS(m);d++)
      {  if (!(p = mclvGetIvp(sel, m->cols[d].vid, p)))
         mclvResize(m->cols+d, 0)
      ;  else
         mcldMeet(m->cols+d, sel, m->cols+d)
   ;  }
      return sel
;  }


void mclxUnary
(  mclx*  src
,  double (*op)(pval, void*)
,  void* arg
)
   {  dim         n_cols =  N_COLS(src)
   ;  mclv*  vec    =  src->cols

   ;  while (n_cols-- > 0)    /* careful with unsignedness */
         mclvUnary(vec, op, arg)
      ,  vec++
;  }


void mclxAccomodate
(  mclx* mx
,  const mclv* dom_cols
,  const mclv* dom_rows
)
   {  if (dom_cols && !mcldEquate(mx->dom_cols, dom_cols, MCLD_EQT_SUPER))
      mclxChangeCDomain(mx, mcldMerge(mx->dom_cols, dom_cols, NULL))
   ;  if (dom_rows && !mcldEquate(mx->dom_rows, dom_rows, MCLD_EQT_SUPER))
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
   {  dim d

   ;  if (mcldEquate(mx->dom_rows, domain, MCLD_EQT_LDIFF))
      {  for (d=0;d<N_COLS(mx);d++)
         mcldMeet(mx->cols+d, domain, mx->cols+d)
   ;  }
      mclvFree(&(mx->dom_rows))
   ;  mx->dom_rows = domain
;  }


void mclxChangeCDomain
(  mclx* mx
,  mclv* domain
)
   {  dim d
   ;  mclv* new_cols
   ;  mclv* cvec = mx->cols

   ;  if (mcldEquate(mx->dom_cols, domain, MCLD_EQT_EQUAL))
      {  mclvFree(&domain)
      ;  return
   ;  }

      new_cols =  mcxAlloc
                  (  domain->n_ivps * sizeof(mclv)
                  ,  ENQUIRE_ON_FAIL
                  )
   ;  if (!new_cols && domain->n_ivps)
         mcxMemDenied
         (stderr, "mclxChangeCDomain", "mclv", domain->n_ivps)
      ,  mcxExit(1)

   ;  for (d=0;d<domain->n_ivps;d++)
      {  mclv* newcol=  new_cols+d
      ;  long vid    =  domain->ivps[d].idx
      ;  cvec        =  mclxGetVector(mx, vid, RETURN_ON_FAIL, cvec)

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

   ;  for (d=0;d<N_COLS(mx);d++)
      mclvRelease(mx->cols+d)
   ;  mcxFree(mx->cols)

   ;  mx->cols = new_cols

   ;  mclvFree(&(mx->dom_cols))
   ;  mx->dom_cols = domain
;  }


mclx*  mclxBlocksC
(  const mclx*     mx
,  const mclx*     domain
)
   {  dim d
   ;  mclx* blocksc  =  mclxAllocClone(mx)

   ;  for (d=0;d<N_COLS(domain);d++)
      {  mclv* dom  =   domain->cols+d
      ;  ofs offset =  -1
      ;  dim e
      ;  for (e=0;e<dom->n_ivps;e++)
         {  long idx =  dom->ivps[e].idx
         ;  mclv* universe
         ;  offset = mclvGetIvpOffset(mx->dom_cols, idx, offset)
         ;  if (offset < 0)
            continue
         ;  universe =     blocksc->cols[offset].n_ivps
                        ?  blocksc->cols+offset
                        :  mx->cols+offset
         ;  mcldMinus(universe, dom, blocksc->cols+offset)
      ;  }
      }
   ;  return blocksc
;  }



/* implementation note:
 *    This may suggest mclvTernary(v1, v2, v3, f, g)
 *    Write f(v1[x], v2[x]) if g(v2[x], v3[x]).
 *    That would get rid of the temporary vector in the inner loop.
 *
 *    How useful would mclvTernary be otherwise?
*/

mclx*  mclxBlocks
(  const mclx*     mx       /* fixme; check domain equality ? */
,  const mclx*     domain
)
   {  dim d
   ;  mclv* meet     =  mclvInit(NULL)
   ;  mclx* blocks   =  mclxAllocClone(mx)

   ;  for (d=0;d<N_COLS(domain);d++)
      {  mclv* dom  =   domain->cols+d
      ;  ofs offset =  -1
      ;  dim e
      ;  for (e=0;e<dom->n_ivps;e++)
         {  long idx =  dom->ivps[e].idx
         ;  offset = mclvGetIvpOffset(mx->dom_cols, idx, offset)
         ;  if (offset < 0)
            continue
         ;  mcldMeet(mx->cols+offset, dom, meet)
         ;  mclvBinary(blocks->cols+offset, meet, blocks->cols+offset, fltLoR)
      ;  }
      }
      mclvFree(&meet)
   ;  return blocks
;  }


mclx*  mclxBlocks2
(  const mclx*     mx
,  const mclx*     domain
)
   {  dim d
   ;  mclx* blocks  =   mclxAllocClone(mx)

   ;  for (d=0;d<N_COLS(domain);d++)
      {  mclv* dom = domain->cols+d
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

void mclxMerge
(  mclx* m1
,  const mclx* m2
,  double  (*op)(pval, pval)
)
   {  mclv *m1vec = m1->cols
   ;  dim d, rdif = 0

   ;  if (mcldCountParts(m1->dom_rows, m2->dom_rows, NULL, NULL, &rdif))
      {  mcxErr
         (  "mclxMerge PBD"
         ,  "left domain (ct %ld) does not subsume right domain (ct %ld)"
         ,  (long) N_COLS(m2)
         ,  (long) N_COLS(m1)
         )
      ;  return
   ;  }

      for (d=0;d<N_COLS(m2);d++)
      {  mclv *m2vec = m2->cols+d

      ;  if (!(m1vec = mclxGetVector(m1, m2vec->vid, RETURN_ON_FAIL, m1vec)))
         continue

      ;  if (!mclvBinary(m1vec, m2vec, m1vec, op))
         break    /* fixme; should err, pbb not free */
   ;  }
   }


void mclxAddto
(  mclx* m1
,  const mclx* m2
)
   {  mclv *m1vec = m1->cols
   ;  dim d, rdif = 0

   ;  if (mcldCountParts(m1->dom_rows, m2->dom_rows, NULL, NULL, &rdif))
      {  mcxErr
         (  "mclxAddto PBD"
         ,  "left domain (ct %ld) does not subsume right domain (ct %ld)"
         ,  (long) N_COLS(m2)
         ,  (long) N_COLS(m1)
         )
      ;  return
   ;  }

      for (d=0;d<N_COLS(m2);d++)
      {  mclv *m2vec = m2->cols+d

      ;  if (!m2vec->n_ivps)
         continue

      ;  if (!(m1vec = mclxGetVector(m1, m2vec->vid, RETURN_ON_FAIL, m1vec)))
         continue

      ;  if (mcldCountParts(m1vec, m2vec, NULL, NULL, &rdif))
         mclvAdd(m1vec, m2vec, m1vec)
      ;  else
         mclvUpdateMeet(m1vec, m2vec, fltAdd)
   ;  }
   }


mclx* mclxBinary
(  const mclx* m1
,  const mclx* m2
,  double  (*op)(pval, pval)
)
   {  mclv *dom_rows     =  mcldMerge
                                 (  m1->dom_rows
                                 ,  m2->dom_rows
                                 ,  NULL
                                 )
   ;  mclv *dom_cols     =  mcldMerge
                                 (  m1->dom_cols
                                 ,  m2->dom_cols
                                 ,  NULL
                                 )
   ;  mclx*  m3          =  mclxAllocZero(dom_cols, dom_rows)
   ;  mclv  *dstvec      =  m3->cols 
   ;  mclv  *m1vec       =  m1->cols
   ;  mclv  *m2vec       =  m2->cols
   ;  mclv  empvec

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


ofs mclxGetVectorOffset
(  const mclx* mx
,  long  vid
,  mcxOnFail ON_FAIL
,  ofs  offset
)
   {  mclv* vec =  mclxGetVector
                        (  mx
                        ,  vid
                        ,  ON_FAIL
                        ,  offset > 0 ? mx->cols+offset : NULL
                        )
   ;  return vec ? vec - mx->cols : -1
;  }


mclv* mclxGetNextVector
(  const mclx* mx
,  long   vid
,  mcxOnFail ON_FAIL
,  const mclv* offset
)
   {  const mclv* max =  mx->cols + N_COLS(mx)

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
            ,  "did not find vector <%ld> in <%lu,%lu> matrix"
            ,  (long) vid
            ,  (ulong) N_COLS(mx)
            ,  (ulong) N_ROWS(mx)
            )
         ,  mcxExit(1)
   ;  }
      else
      return (mclv*) offset
   ;  return NULL
;  }


mclv* mclxGetVector
(  const mclx* mx
,  long   vid
,  mcxOnFail ON_FAIL
,  const mclv* offset
)
   {  dim n_cols  =  N_COLS(mx)
   ;  mclv* found =  NULL

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
      found = (mclv*) offset      /* const riddance */
   ;  else
      {  mclv keyvec
      ;  mclvInit(&keyvec)
      ;  keyvec.vid = vid

      ;  if (!offset)
         offset = mx->cols

      ;  n_cols -= (offset - mx->cols)
      ;  found =  bsearch
                  (  &keyvec
                  ,  offset
                  ,  n_cols
                  ,  sizeof(mclv)
                  ,  mclvVidCmp
                  )
   ;  }

      if (!found && ON_FAIL == EXIT_ON_FAIL)
         mcxErr
         (  "mclxGetVector PBD"
         ,  "did not find vector <%ld> in <%lu,%lu> matrix"
         ,  (long) vid
         ,  (ulong) N_COLS(mx)
         ,  (ulong) N_ROWS(mx)
         )
      ,  mcxExit(1)

   ;  return found
;  }


mclx* mclxMakeMap
(  mclv*  dom_cols
,  mclv*  new_dom_cols
)
   {  mclx* mx
   ;  dim d

   ;  if (dom_cols->n_ivps != new_dom_cols->n_ivps)
      return NULL

   ;  mx = mclxAllocZero(dom_cols, new_dom_cols)

   ;  for (d=0;d<N_COLS(mx);d++)
      mclvInsertIdx(mx->cols+d, new_dom_cols->ivps[d].idx, 1.0)

   ;  return mx
;  }


mclx* mclxTranspose
(  const mclx*  m
)
   {  mclx*   tp  =  mclxAllocZero
                     (  mclvCopy(NULL, m->dom_rows)
                     ,  mclvCopy(NULL, m->dom_cols)
                     )
   ;  const mclv*  mvec =  m->cols
   ;  mclv* tvec
   ;  dim i = N_COLS(m)
   ;

      /*
       * Pre-calculate sizes of destination columns
       * fixme; if canonical domains do away with mclxGetVector.
      */
      while (i-- > 0)         /* careful with unsignedness */
      {  dim   n_ivps  =  mvec->n_ivps
      ;  mclIvp*  ivp  =  mvec->ivps
      ;  tvec           =  tp->cols

      ;  while (n_ivps-- > 0)   /* careful with unsignedness */
         {  tvec = mclxGetVector(tp, ivp->idx, EXIT_ON_FAIL, tvec)
         ;  tvec->n_ivps++
         ;  ivp++
         ;  tvec++               /* with luck we get immediate hit */
      ;  }
         mvec++
   ;  }

      /*
       * Allocate
      */
      tvec  =  tp->cols
   ;  i     =  N_COLS(tp)
   ;  while (i-- > 0)           /* careful with unsignedness */
      {  if (!mclvResize(tvec, tvec->n_ivps))
         {  mclxFree(&tp)
         ;  return 0
      ;  }
         tvec->n_ivps = 0        /* dirty: start over for write */
      ;  tvec++
   ;  }

      /*
       * Write
       *
      */
      mvec     =  m->cols
   ;  while (mvec < m->cols+N_COLS(m))
      {  dim   n_ivps  =  mvec->n_ivps
      ;  mclIvp* ivp   =  mvec->ivps
      ;  tvec           =  tp->cols

      ;  while (n_ivps-- > 0)   /* careful with unsignedness */
         {  tvec = mclxGetVector(tp, ivp->idx, EXIT_ON_FAIL, tvec)
         ;  tvec->ivps[tvec->n_ivps].idx = mvec->vid
         ;  tvec->ivps[tvec->n_ivps].val = ivp->val
         ;  tvec->n_ivps++
         ;  tvec++
         ;  ivp++
      ;  }
         mvec++
   ;  }

      return tp
;  }


mclv* mclxColNums
(  const mclx*  m
,  double           (*f_cb)(const mclv * vec)
,  mcxenum           mode  
)
   {  mclv*  nums = mclvClone(m->dom_cols)
   ;  dim i =  0
   
   ;  if (nums)
      {  while (i < N_COLS(m))
         {  nums->ivps[i].val = f_cb(m->cols + i)
         ;  i++
      ;  }
      }
      if (mode == MCL_VECTOR_SPARSE)
      mclvUnary(nums, fltxCopy, NULL)

   ;  return nums
;  }


mclv* mclxDiagValues
(  const mclx*  m
,  mcxenum     mode  
)
   {  return mclxColNums(m, mclvSelf, mode)
;  }


mclv* mclxColSums
(  const mclx*  m
,  mcxenum     mode  
)
   {  return mclxColNums(m, mclvSum, mode)
;  }


mclv* mclxColSizes
(  const mclx*     m
,  mcxenum        mode
)
   {  return mclxColNums(m, mclvSize, mode)
;  }


double mclxMass
(  const mclx*     m
)
   {  dim d
   ;  double  mass  =  0
   ;  for (d=0;d<N_COLS(m);d++)
      mass += mclvSum(m->cols+d)
   ;  return mass
;  }


dim mclxNrofEntries
(  const mclx*     m
)
   {  dim d
   ;  dim nr =  0
   ;  for (d=0;d<N_COLS(m);d++)
      nr += (m->cols+d)->n_ivps
   ;  return nr
;  }


void  mclxColumnsRealign
(  mclx* m
,  int (*cmp)(const void* vec1, const void* vec2)
)
   {  dim d
   ;  qsort(m->cols, N_COLS(m), sizeof(mclv), cmp)
   ;  for (d=0;d<m->dom_cols->n_ivps;d++)
      m->cols[d].vid = m->dom_cols->ivps[d].idx
;  }


dim mclxNEntries
(  const mclx*     mx
)
   {  dim d
   ;  dim n = 0
   ;  for (d=0;d<N_COLS(mx);d++)
      n += mx->cols[d].n_ivps
   ;  return n
;  }


double mclxMaxValue
(  const mclx*        mx
) 
   {  double max_val  =  0.0
   ;  mclxUnary((mclx*)mx, fltxPropagateMax, &max_val)
   ;  return max_val
;  }


mclx* mclxIdentity
(  mclv* vec
)  
   {  return mclxConstDiag(vec, 1.0)
;  }


void mclxMakeCharacteristic
(  mclx*              mx
)  
   {  double one  =  1.0
   ;  mclxUnary(mx, fltxConst, &one)
;  }


mclx* mclxMax
(  const mclx*        m1
,  const mclx*        m2
)  
   {  return mclxBinary(m1, m2, fltMax)
;  }


mclx* mclxMinus
(  const mclx*        m1
,  const mclx*        m2
)  
   {  return mclxBinary(m1, m2, fltSubtract)
;  }


mclx* mclxAdd
(  const mclx*        m1
,  const mclx*        m2
)  
   {  return mclxBinary(m1, m2, fltAdd)
;  }


/* TODO: what would be an efficient mechanism for optionally
 *    squeezing in a diagonal element around the mclvAdd call?  Perhaps the
 *    overalloc-one-ivp thing. But it adds significant complexity and ideally
 *    one would want to hide it in the allocator. Perhaps write a special
 *    purpose allocator for ivps.
*/

void mclxAddTranspose
(  mclx* mx
,  double diagweight
)
   {  dim d
   ;  mclx* mxt = mclxTranspose(mx)
   ;  mclv* mvec = NULL

   ;  mclxChangeDomains
      (  mx
      ,  mcldMerge(mx->dom_cols, mxt->dom_cols, NULL)
      ,  mcldMerge(mx->dom_rows, mxt->dom_rows, NULL)
      )

   ;  for (d=0;d<N_COLS(mxt);d++)
      {  long vid = mxt->dom_cols->ivps[d].idx
      ;  mvec = mclxGetVector(mx, vid, RETURN_ON_FAIL, mvec)
      ;  if (!mvec)
         {  mcxErr("mclxAddTranspose panic", "no vector %ld in matrix", vid)
         ;  continue
      ;  }
         mclvAdd(mvec, mxt->cols+d, mvec)
      ;  mclvRelease(mxt->cols+d)
   ;  }

      if (diagweight != 1.0)
      mclxScaleDiag(mx, diagweight)
   ;  mclxFree(&mxt)
;  }



mclx* mclxHadamard
(  const mclx*        m1
,  const mclx*        m2
)
   {  return mclxBinary(m1, m2, fltMultiply)
;  }


double mclxLoopCBifEmpty
(  mclv  *vec
,  long r cpl__unused
,  void*data cpl__unused
)
   {  return vec->n_ivps ? 0.0 : 1.0
;  }


double mclxLoopCBremove
(  mclv  *vec
,  long r cpl__unused
,  void*data cpl__unused
)
   {  return 0.0
;  }


double mclxLoopCBmax
(  mclv  *vec
,  long r cpl__unused
,  void*data cpl__unused
)
   {  double max = mclvMaxValue(vec)
   ;  if (vec->n_ivps && max)
      return max
   ;  return 1.0
;  }


dim mclxAdjustLoops
(  mclx*    mx
,  double  (*op)(mclv* vec, long r, void* data)
,  void*    data
)
   {  dim d, n_void = 0
   ;  for (d=0;d<N_COLS(mx);d++)
      {  mclv*    vec   =  mx->cols+d
      ;  mclp*    ivp   =  mclvGetIvp(vec, vec->vid, NULL)
      ;  double   val

      ;  if (ivp)
         ivp->val = 0.0

      ;  val = op(vec, vec->vid, data)

      ;  if (!vec->n_ivps)
         n_void++

      ;  if (ivp && !val)
            ivp->val = 0.0
         ,  mclvUnary(vec, fltxCopy, NULL)
      ;  else if (ivp && val)
         ivp->val = val
      ;  else if (!ivp && val)
         mclvInsertIdx(vec, vec->vid, val)
   ;  }
      return n_void
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
         ?  mclgUnionv(mx, NULL, NULL, SCRATCH_DIRTY, NULL)
         :  NULL

   ;  if (bits & MCLX_WEED_GRAPH)
      {  mcldMerge(colselect, rowselect, colselect)
      ;  mclvCopy(rowselect, colselect)
   ;  }
      mclxChangeDomains(mx, colselect, rowselect)
;  }


dim mclxUnaryList
(  mclx*    mx
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)
   {  dim n_cols  =  N_COLS(mx)
   ;  mclv* vec   =  mx->cols
   ;  dim n_entries_kept = 0

   ;  while (n_cols-- > 0)    /* careful with unsignedness */
         n_entries_kept += mclvUnaryList(vec, ar)
      ,  vec++
   ;  return n_entries_kept
;  }


mclv* mclgUnionv
(  mclx* mx
,  const mclv* coldom
,  const mclv* restrict
,  mcxenum scratch_STATUS
,  mclv* dst
)
   {  const mclv* dvec = NULL
   ;  mclv *row_scratch = NULL
   ;  mcxbool canonical = mclxRowCanonical(mx)
   ;  mclpAR* par = mclpARensure(NULL, 256)
   ;  dim d

   ;  if (!dst)
      dst = mclvInit(dst)
   ;  else
      mclvResize(dst, 0)

   ;  if (!coldom)
      coldom = mx->dom_cols

   ;  if (scratch_STATUS == SCRATCH_BUSY)
      row_scratch = mclvClone(mx->dom_rows)
   ;  else
      row_scratch = mx->dom_rows
      
   ;  if (scratch_STATUS != SCRATCH_READY && scratch_STATUS != SCRATCH_UPDATE)
      mclvMakeCharacteristic(row_scratch)

   ;  for (d=0;d<coldom->n_ivps;d++)
      {  long idx = coldom->ivps[d].idx

      ;  if ((dvec = mclxGetVector(mx, idx, RETURN_ON_FAIL, dvec)))
         {  long o_scratch = -1, o_restrict = -1
         ;  dim t
         ;  for (t=0; t<dvec->n_ivps; t++)
            {  long idx = dvec->ivps[t].idx
            ;  if
               (  0
               >  (  o_scratch
                  =     canonical
                     ?  idx
                     :  mclvGetIvpOffset(row_scratch, idx, o_scratch)
                  )
               )
               continue               /* panic, really */
            ;  if
               (  restrict
               && 0 > (o_restrict = mclvGetIvpOffset(restrict, idx, o_restrict))
               )
               continue                /* not found in restriction domain */

            ;  if (row_scratch->ivps[o_scratch].val < MCLG_UNIONV_SENTINEL)
                  row_scratch->ivps[o_scratch].val = MCLG_UNIONV_SENTINEL + 0.5
               ,  mclpARextend(par, idx, 1.0)
         ;  }
         }
      }

   ;  mclvFromPAR
      (  dst
      ,  par
      ,  0
      ,  mclpMergeLeft
      ,  NULL
      )
   ;  mclpARfree(&par)

   ;  if (scratch_STATUS == SCRATCH_READY)
      {  long o = -1
      ;  for(d=0;d<dst->n_ivps;d++)
         {  o =   canonical
               ?  dst->ivps[d].idx
               :  mclvGetIvpOffset(mx->dom_rows, dst->ivps[d].idx, o)
         ;  row_scratch->ivps[o].val = 1.0
      ;  }
      }

      if (scratch_STATUS == SCRATCH_BUSY)
      mclvFree(&row_scratch)

   ;  return dst
;  }


