/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include "interpret.h"

#include <math.h>
#include <float.h>
#include <limits.h>

#include "clm.h"

#include "util/types.h"
#include "util/alloc.h"
#include "util/io.h"
#include "util/err.h"

#include "impala/io.h"
#include "impala/scan.h"

#define  IS_ATTRACTIVE_RECURBOUND     20


#define  is_attractive      0x01
#define  is_unattractive    0x02
#define  is_classified      0x04

/*
 *   Routine int isAttractive(,,,) is used by mclInterpret. Fills array
 *   colProps after a partial categorization. Vertices (i.e. columns of the
 *   matrix M) come in two flavours: i)  is_attractive ii) and is_unattractive.
 *   In the partial initialization, a vertex/column c is attractive if it sees
 *   itself (there is a loop from c to c), it is unattractive if there is no
 *   incoming loop at all. isAttractive computes the closure of the relation
 *   "has attractive parent", where a first order attractive parent is an
 *   incoming node which has itself a loop. Initially unattractive nodes can
 *   thus become attractive.
 *
 *   Motivation: mclInterpret uses each connected set of attractive nodes as
 *   defining (the core of) a separate cluster. All nodes which reach such a
 *   set are included in the cluster as well. This can cause overlap. This
 *   definition and implementation ensures that the resulting clustering is
 *   permutation invariant, i.e. does not depend on the order in which the
 *   columns of M (nodes of the underlying graph) are listed. This is not
 *   entirely true, since there is a bound on the recursion. Graphs meeting
 *   this recursion bound are likely to be very funny.
 *
 *   colProps is indexed by the vector count, not by the vector vid.
 *   The mclInterpret code is now quite hard to read; it needs documentation.
 *   What it does is quite reasonable however.
 *
*/


int isAttractive
(  const mclMatrix*  Mt
,  int*              colProps
,  int               col
,  int               depth
)  ;


/* Keep only highest weight edges for each tail node,
 * using some weighting factors.
*/

mclMatrix* mclDag
(  const mclMatrix*  A
,  const mclInterpretParam* ipp
)
   {  long col

   ;  double w_selfval= ipp ? ipp->w_selfval: 0.999
   ;  double w_maxval = ipp ? ipp->w_maxval : 0.001
   ;  double delta    = ipp ? ipp->delta    : 0.01 

   ;  mclMatrix* M = mclxAllocZero
                     (  mclvCopy(NULL, A->dom_cols)
                     ,  mclvCopy(NULL, A->dom_rows)
                     )
   ;  for (col=0;col<N_COLS(A);col++)           /* thorough clean-up */
      {  mclVector*  vec      =  A->cols+col
      ;  mclVector*  dst      =  M->cols+col
      ;  double      selfval  =  mclvIdxVal(vec, vec->vid, NULL)
      ;  double      maxval   =  mclvMaxValue(vec)
      ;  double      bar      =  selfval < maxval
                                 ?  (  (w_selfval * selfval) 
                                    +  (w_maxval * maxval) 
                                    )
                                 :     delta
                                    ?  selfval / (1 + delta)
                                    :  selfval
      ;  int n_bar =  mclvCountGiven(vec, mclpGivenValGt, &bar)
      ;  mclvCopyGiven(dst, vec, mclpGivenValGt, &bar, n_bar)
   ;  }
      return M
;  }



/* we have equivalence classes of attractors, indexed by classid.
   classes are mapped back to neighbour lists, namely
   the merge of all the neighbours in a given class.

fixme: attractor-in-chain is not write:
spurious attr 505 in chain starting at 505 messages.
*/

int calc_depth
(  mclx* m_transient
,  mclx* m_attr_to_classid
,  mclx* m_classid_to_jointnb
)
   {  mclv *wave_attr=  mclvInit(NULL)
   ;  mclv* wave     =  mclvInit(NULL)
   ;  mclv *classid  =  mclvInit(NULL)
   ;  mclv *classnb  =  mclvInit(NULL)
   ;  mclv *new_wave =  mclvInit(NULL)
   ;  mclv *seen_tr  =  mclvInit(NULL)
   ;  mclv *seen_at  =  mclvInit(NULL)
   ;  const mclv* classid_single_attr = NULL /* don't free this one */
   ;  int maxdepth   =  0
   ;  int i

   ;  for (i=0;i<N_COLS(m_transient);i++)
      {  long cid = m_transient->cols[i].vid
      ;  mcxbool  attr_in_chain = FALSE
      ;  long     attr_example  = -1
      ;  int      depth = 0

      ;  if
         (  (  classid_single_attr
            =  mclxGetVector(m_attr_to_classid, cid, RETURN_ON_FAIL, NULL)
            )
         )
         {  mclxUnionv(m_classid_to_jointnb, classid_single_attr, wave)
         ;  if (wave->n_ivps)
               attr_in_chain =   TRUE
            ,  attr_example  =   cid
      ;  }
         else
         mclvCopy(wave, m_transient->cols+i)

      ;  mclvResize(seen_tr, 0)
      ;  mclvResize(seen_at, 0)

                                       /* fixme keep track of seen attractors */
      ;  while (wave->n_ivps)
         {  if (mclvGetIvp(wave, cid, NULL))
            {  fprintf(stderr, "loop from %ld depth %d\n", cid, depth)
            ;  break
         ;  }

                           /* 1. find attractors in wave */
                           /* 2. find classids of those attractors */
                           /* 3. find joint transient neighbours */
                           /* 4. remove attractors from wave */
                           /* 5. create next wave by merging transient neighbour lists */
                           /* 6. merge attractor induced neighbours */
                           /* 7. discards those we already saw */
                           /* 8. update the seen list for next time around */

         ;  mcldMeet(wave, m_attr_to_classid->dom_cols, wave_attr)
         ;  mclxUnionv(m_attr_to_classid, wave_attr, classid)
         ;  mclxUnionv(m_classid_to_jointnb, classid, classnb)
         ;  if (wave_attr->n_ivps)
            mcldMinus(wave, wave_attr, wave)
         ;  if (!attr_in_chain && classnb->n_ivps && wave_attr->n_ivps)
               attr_in_chain =   TRUE
            ,  attr_example  =   wave_attr->ivps[0].idx
         ;  mclxUnionv(m_transient, wave, new_wave)
         ;  mcldMerge(new_wave, classnb, new_wave)
         ;  mcldMinus(new_wave, seen_tr, new_wave)
         ;  mcldMerge(seen_tr, new_wave, seen_tr)

         ;  {  mclv* tmp = wave
            ;  wave = new_wave
            ;  new_wave = tmp
         ;  }
            depth++
      ;  }
         if (depth > maxdepth)
         maxdepth = depth
      ;  if (0 && attr_in_chain)
         fprintf(stderr, "attr %ld in chain starting at %ld\n", attr_example, cid)
   ;  }

      mclvFree(&wave)
   ;  mclvFree(&wave_attr)
   ;  mclvFree(&classid)
   ;  mclvFree(&classnb)
   ;  mclvFree(&new_wave)
   ;  mclvFree(&seen_tr)
   ;  mclvFree(&seen_at)
   ;  return maxdepth
;  }



  /* 
   *  calc_depth takes care of situations
   *  where attractors go further down the transient matrix. It is improbable,
   *  but one possible scenario is this:
   *
   *    (mclheader
   *    mcltype matrix
   *    dimensions 7x7
   *    )
   *    (mclmatrix
   *    begin
   *    0 1 4 $
   *    1 2 3 $
   *    2 2 3 5 $
   *    3 3 $
   *    4 5 $
   *    5 6 $
   *    6 6 $
   *    )
   *
   *  note though that this matrix is not dpsd (asymmetric 0/1 pattern to start with)
  */


int mclDagTest
(  const mclMatrix* dag
)
   {  mclx* m_attr_classes, *m_attr_to_classid, *m_classid_to_jointnb
   ;  mclx* m_attr      =  mclxAllocClone(dag)
   ;  mclx* m_transient =  mclxCopy(dag)
   ;  int maxdepth      =  0
   ;  int i

                       /*
                        *  m_attr
                        *     is the symmetrified matrix on all nodes that do
                        *     have a self-value. Links to non-attractors are removed.
                        *  m_transient
                        *     is the matrix where all attractor vectors are truncated.
                        *  m_attr_classes
                        *     lists the connected components of m_attr
                        *  m_attr_to_classid
                        *     maps attractors to their class ID.
                        *  m_classid_to_jointnb
                        *     maps the class ID to joint neighbours
                       */

#ifdef DEBUG
   ;  mcxIO *u_attr_to_classid      =  mcxIOnew("u_attr_to_classid", "w")
   ;  mcxIO *u_classid_to_jointnb   =  mcxIOnew("u_classid_to_jointnb", "w")
   ;  mcxIO *u_transient            =  mcxIOnew("u_transient", "w")
#endif

   ;  for (i=0;i<N_COLS(m_transient);i++)
      {  mclv* col = m_transient->cols+i
      ;  if (mclvGetIvp(col, col->vid, NULL))   /* deemed attractor */
            mclvCopy(m_attr->cols+i, col)
         ,  mclvResize(col, 0)                  /* remove from transient */
   ;  }

                                                /* coldom on attrs only */
      {  mclxWeed(m_attr, MCLX_WEED_COLS)
      ;  mclxAddTranspose(m_attr, 1.0)          /* symmetrify */
      ;  mclxMakeCharacteristic(m_attr)
   ;  }

                                                /* deemed attr systems */
   ;  m_attr_classes       =  mclcComponents(m_attr, NULL)
   ;  m_attr_to_classid    =  mclxTranspose(m_attr_classes)
   ;  m_classid_to_jointnb =  mclxAllocZero
                              (  mclvClone(m_attr_classes->dom_cols)
                              ,  mclvClone(dag->dom_cols)
                              )

   ;  for (i=0;i<N_COLS(m_attr_classes);i++)
      {  mclv* class =  m_attr_classes->cols+i
      ;  mclv* joint =  m_classid_to_jointnb->cols+i
      ;  mclv* nb    =  NULL
      ;  int j
      ;  for (j=0;j<class->n_ivps;j++)
         {  long id  = class->ivps[j].idx
         ;  nb = mclxGetVector(dag, id, EXIT_ON_FAIL, nb)
         ;  mcldMerge(joint, nb, joint)
      ;  }
         mcldMinus(joint, m_attr_classes->cols+i, joint)
        /*  if the above statement does something, we have a node
         *  (in joint) reached by one of the attractors that reaches back
         *  into the attractor set -- i.e. it is a loop.
         *  We might opt to not do this and inspect and solve
         *  the problem in the general loop-checking code.
        */
   ;  }

#ifdef DEBUG
      mclxWrite(m_attr_to_classid, u_attr_to_classid, 6, EXIT_ON_FAIL)
   ;  mclxWrite(m_transient, u_transient, 6, EXIT_ON_FAIL)
   ;  mclxWrite(m_classid_to_jointnb, u_classid_to_jointnb, 6, EXIT_ON_FAIL)
#endif

   ;  maxdepth = calc_depth(m_transient, m_attr_to_classid, m_classid_to_jointnb)

   ;  mclxFree(&m_transient)
   ;  mclxFree(&m_attr_to_classid)
   ;  mclxFree(&m_classid_to_jointnb)
   ;  mclxFree(&m_attr)
   ;  mclxFree(&m_attr_classes)

#ifdef DEBUG
   ;  mcxIOfree(&u_classid_to_jointnb)
   ;  mcxIOfree(&u_attr_to_classid)
   ;  mcxIOfree(&u_transient)
#endif

   ;  return maxdepth
;  }


mclMatrix* mclInterpret
(  const mclMatrix* dag
)
   {  mclx* clus2elem   =  NULL
   ;  mclx* dagtp      =  mclxTranspose(dag)

   ;  int n_cols =  N_COLS(dag)
   ;  int col, i, startoffset, n_cluster

   ;  int*           colProps = NULL
   ;  mclVector**    clusterNodes = NULL

   ;  if
      (  n_cols &&
      !  (colProps = mcxAlloc(sizeof(int)*n_cols, RETURN_ON_FAIL))
      )
         mcxMemDenied(stderr, "mclInterpret", "int", n_cols)
      ,  mcxExit(1)

   ;  if
      (
      !  (  clusterNodes
         =  mcxAlloc(sizeof(mclVector*)*(n_cols+1), RETURN_ON_FAIL)
         )
      )
         mcxMemDenied(stderr, "mclInterpret", "mclVector", n_cols+1)
      ,  mcxExit(1)

   ;  for (i=0;i<n_cols;i++)
      *(colProps+i) = 0

   ;  for (col=0;col<n_cols;col++)
      {  mclVector*  vec    =  dag->cols+col
      ;  int offset
                       /* vertex col has a loop */
      ;  if (mclvIdxVal(vec, vec->vid, &offset), offset >= 0)
         colProps[col] = colProps[col] | is_attractive

                       /* vertex col has no parents (and thus not a loop) */
      ;  if ((dagtp->cols+col)->n_ivps == 0)
         colProps[col] = colProps[col] | is_unattractive
   ;  }

                       /* compute closure of Bool "has attractive parent" */
   ;  for (col=0;col<n_cols;col++)
      isAttractive (dagtp, colProps, col, 0)

   ;  startoffset = 0
   ;  n_cluster = 0
   ;

      while (startoffset < n_cols)
      {
         {  while (startoffset < n_cols)
            {  if
               (  (colProps[startoffset] & is_attractive)
               &&!(colProps[startoffset] & is_classified)
               )
               break
            ;  else
               startoffset++
         ;  }

            if (startoffset == n_cols)
            continue
      ;  }
                                            /* new members of cluster, resp */
                                            /* current members of cluster   */
         {  mclVector  *leafnodes = mclvResize(NULL, 1)
         ;  mclVector  *treenodes = mclvResize(NULL, 0)
         ;  int depth = 0

         ;  mclpInstantiate(leafnodes->ivps+0,dag->cols[startoffset].vid,1.0)

         ;  while (leafnodes->n_ivps)
            {  mclVector  *new_leafnodes = mclvInit(NULL)
            ;  mclIvp *leafivp, *leafl, *leafr

            ;  depth++

            ;  leafl = leafnodes->ivps+0
            ;  leafr = leafl + leafnodes->n_ivps

            ;  for (leafivp=leafl; leafivp < leafr; leafivp++)
               {  int leafoffset       =   mclxGetVectorOffset
                                          (  dag
                                          ,  leafivp->idx
                                          ,  EXIT_ON_FAIL
                                          ,  -1
                                          )
                                          /* fixme: return & check */
               ;  colProps[leafoffset] =   colProps[leafoffset] | is_classified

               ;  if (colProps[leafoffset] & is_attractive)
                  {  mcldMerge          /* look forward */
                     (  new_leafnodes
                     ,  dag->cols+leafoffset
                     ,  new_leafnodes
                     )
                  ;  mcldMerge          /* look backward too */
                     (  new_leafnodes
                     ,  dagtp->cols+leafoffset
                     ,  new_leafnodes
                     )
               ;  }
                  else if (!(colProps[leafoffset] & is_attractive))
                  mcldMerge          /* look backward only */
                  (  new_leafnodes
                  ,  dagtp->cols+leafoffset
                  ,  new_leafnodes
                  )
            ;  }

               treenodes= mcldMerge(treenodes, leafnodes, treenodes)
            ;  leafnodes= mcldMinus(new_leafnodes, treenodes, leafnodes)
            ;  mclvFree(&new_leafnodes)
         ;  }

            *(clusterNodes+n_cluster) = treenodes
         ;  mclvFree(&leafnodes)
         ;  n_cluster++
      ;  }
      }

     /*  NOTE this should work for empty matrix and zero clusters as well */

      {  clus2elem =    mclxAllocZero
                        (  mclvCanonical(NULL, n_cluster, 1.0)
                        ,  mclvCopy(NULL, dag->dom_cols)
                        )
      ;  for (i=0;i<n_cluster;i++)
         {  mclvRenew
            (  clus2elem->cols+i
            ,  (*(clusterNodes+i))->ivps 
            ,  (*(clusterNodes+i))->n_ivps
            )
         ;  mclvFree(clusterNodes+i)
      ;  }
         mclxMakeCharacteristic(clus2elem)
      ;
      }

      free(colProps)
   ;  free(clusterNodes)
   ;  mclxFree(&dagtp)

   ;  return clus2elem
;  }


int isAttractive
(  const mclMatrix*  Mt
,  int* colProps
,  int colidx
,  int depth
)
   {  int   i
   ;  mclVector* parents = Mt->cols+colidx

   ;  if (colProps[colidx] & is_attractive)     /* already categorized */
      return 1

   ;  if (colProps[colidx] & is_unattractive)   /* already categorized */
      return 0

   ;  if (depth > IS_ATTRACTIVE_RECURBOUND)     /* prevent cycles      */
      {  colProps[colidx] = colProps[colidx] |  is_unattractive
      ;  return 0
   ;  }

   ;  for (i=0;i<parents->n_ivps;i++)
      {  int j = mclxGetVectorOffset(Mt, parents->ivps[i].idx, EXIT_ON_FAIL,-1)
                                          /* fixme: return & check */
      ;  if
         (  isAttractive
            (  Mt
            ,  colProps
            ,  j
            ,  depth +1
            )
         )
         {  colProps[colidx] = colProps[colidx] |  is_attractive
         ;  return 1
      ;  }                             /* inherits attractivity from parent */
      }

      colProps[colidx] = colProps[colidx] |  is_unattractive  /* no good */
   ;  return 0
;  }



void  clusterMeasure
(  const mclMatrix*  clus
,  FILE*          fp
)
   {  int         clsize   =  N_COLS(clus)
   ;  int         clrange  =  N_ROWS(clus)
   ;  int         i
   ;  double      ctr      =  0.0

   ;  for (i=0;i<N_COLS(clus);i++)
      ctr += pow((clus->cols+i)->n_ivps, 2.0)

   ;  if (clsize && clrange)
      ctr /= clrange

   ;  fprintf
      (  fp
      ,  " %-d"
      ,  (int) clsize
      )
;  }


#if 0
void mcxDiagnosticsAttractor
(  const char*          ffn_attr
,  const mclMatrix*     clus2elem
,  const mcxDumpParam   dumpParam
)
   {  int         n_nodes     =  clus2elem->n_range
   ;  int         n_written   =  dumpParam->n_written
   ;  int         i           =  0
   ;  mclMatrix*  mtx_Ascore  =  mclxAllocZero(n_written, n_nodes)
   ;  mcxIO*      xfOut       =  mcxIOnew(ffn_atr, "w")

   ;  if (mcxIOopen(xfOut, RETURN_ON_FAIL) == STATUS_FAIL)
      {  mclxFree(&mtx_Ascore)
      ;  mcxIOfree(&xfOut)
      ;  return
   ;  }

   ;  for(i=0; i<n_written; i++)
      {  mclMatrix*  iterand     =  *(dumpParam->iterands+i)
      ;  mclVector*  vec_Ascore  =  NULL

      ;  if (iterands->n_cols != n_nodes || iterand->n_range != n_nodes)
         {  fprintf(stderr, "mcxDiagnosticsAttractor: dimension error\n")
         ;  mcxExit(1)
      ;  }

         vec_Ascore  =  mcxAttractivityScale(iterand)
      ;  mclvRenew((mtx_Ascore->cols+i), vec_Ascore->ivps, vec_Ascore->n_ivps)
      ;  mclvFree(&vec_Ascore)
   ;  }

      mclxbWrite(mtx_Ascore, xfOut, RETURN_ON_FAIL)
   ;  mclxFree(mtx_Ascore)
;  }


void mcxDiagnosticsPeriphery
(  const char*     ffn_peri
,  const mclMatrix*  clustering
,  const mcxDumpParam   dumpParam
)  {
;  }
#endif


mclVector*  mcxAttractivityScale
(  const mclMatrix*           M
)
   {  int n_cols = N_COLS(M)
   ;  int col

   ;  mclVector* vec_values = mclvResize(NULL, n_cols)

   ;  for (col=0;col<n_cols;col++)
      {  mclVector*  vec   =  M->cols+col
      ;  double   selfval  =  mclvIdxVal(vec, col, NULL)
      ;  double   maxval   =  mclvMaxValue(vec)
      ;  if (maxval <= 0.0)
         {  mcxErr
            (  "mcxAttractivityScale"
            ,  "encountered nonpositive maximum value"
            )
         ;  maxval = 1.0  
      ;  }
         (vec_values->ivps+col)->idx = col
      ;  (vec_values->ivps+col)->val = selfval / maxval
   ;  }
      return vec_values
;  }


void mclInterpretParamFree
(  mclInterpretParam **ipp
)
   {  if (*ipp)
      mcxFree(*ipp)
   ;  *ipp = NULL
;  }



mclInterpretParam* mclInterpretParamNew
(  void
)
   {  mclInterpretParam* ipp =  mcxAlloc
                                (  sizeof(mclInterpretParam)
                                ,  EXIT_ON_FAIL
                                )
   ;  ipp->w_selfval    =  0.999
   ;  ipp->w_maxval     =  0.001
   ;  ipp->delta        =  0.01

   ;  return ipp
;  }


double mclxCoverage
(  const mclMatrix*     mx
,  const mclMatrix*     clus2elem
,  double               *maxcoverage
)
   {  int               c
   ;  double            coverage       =  0.0
   ;  double            thismaxcoverage=  0.0
   ;  unsigned long     clus2elem_size =  0       

   ;  if (maxcoverage)
      *maxcoverage = 0.0

   ;  if (!mcldEquate(clus2elem->dom_rows, mx->dom_cols, MCLD_EQ_EQUAL))
      {  mcxErr
         (  "mclxCoverage"
         ,  "node domains (sizes %ld, %ld) do not fit"
         ,  (long) N_ROWS(clus2elem)
         ,  (long) N_COLS(mx)
         )
      ;  return 0
   ;  }

  /*  fixme-urgent; need to have strict partitioning here (why again?) */

      for (c=0;c<N_COLS(clus2elem);c++)
      {  mclVector* clus = clus2elem->cols+c
      ;  mclxScore xscore
      ;  mclxSubScan(mx, clus, clus, &xscore)

      ;  thismaxcoverage += xscore.covmax * clus->n_ivps
      ;  coverage += xscore.cov * clus->n_ivps
      ;  clus2elem_size += clus->n_ivps
   ;  }

      if (clus2elem_size)
      {  if (maxcoverage)
         *maxcoverage = thismaxcoverage / clus2elem_size
      ;  return coverage/clus2elem_size
   ;  }
      return 0.0
;  }


