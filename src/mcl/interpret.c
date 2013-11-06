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

/* maxdepth used for testing once. not so interesting */

int isAttractive
(  const mclMatrix*  Mt
,  int*              colProps
,  int               col
,  int               depth
,  int*              maxdepth
)  ;


mclMatrix* mclTryDag
(  const mclMatrix*  A
,  const mclInterpretParam* ipp
)
   {  long col

   ;  double w_center = ipp ? ipp->w_center : 0.0
   ;  double w_selfval= ipp ? ipp->w_selfval: 0.999
   ;  double w_maxval = ipp ? ipp->w_maxval : 0.001

   ;  mclMatrix* M = mclxAllocZero
                     (  mclvCopy(NULL, A->dom_cols)
                     ,  mclvCopy(NULL, A->dom_rows)
                     )
   ;  for (col=0;col<N_COLS(A);col++)           /* thorough clean-up */
      {  mclVector*  vec      =  A->cols+col
      ;  mclVector*  dst      =  M->cols+col
      ;  double      center   =  mclvPowSum(vec, 2)
      ;  double      selfval  =  mclvIdxVal(vec, vec->vid, NULL)
      ;  double      maxval   =  mclvMaxValue(vec)

      ;  int         n_bar
      ;  double      bar

      ;  if (ipp && ipp->w_partialsum)
         {  mclvTop(vec, ipp->w_partialsum, NULL, &bar, FALSE)
         ;  bar *=  0.99
         ;  n_bar = 0
        /*  disregard count returned by mclvTop in order to ensure fairness */
      ;  }
         else
         {  bar   =  (  (w_center * center) 
                     +  (w_selfval * selfval) 
                     +  (w_maxval * maxval) 
                     )
                     *  0.99
         ;  n_bar =  mclvCountGiven(vec, mclpGivenValGt, &bar)
      ;  }

         mclvCopyGiven(dst, vec, mclpGivenValGt, &bar, n_bar)
   ;  }
      return M
;  }



mclMatrix* mclInterpret
(  const mclMatrix*     A
,  const mclInterpretParam* ipp
,  int*  depthptr
,  mclMatrix** dagptr
)
   {  mclx* clus2elem   =  NULL
   ;  mclx* M, *Mt

   ;  int n_cols =  N_COLS(A)
   ;  int dagdepth = 0
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

   ;  M  = mclTryDag(A, ipp)
   ;  Mt = mclxTranspose(M)
   ;
      {  int maxdepth = 1
      ;  for (col=0;col<n_cols;col++)
         {  mclVector*  vec    =  M->cols+col
         ;  int offset

           /*
            *  vertex col has a loop
           */
         ;  if
            (  mclvIdxVal(vec, vec->vid, &offset)
            ,  offset >= 0
            )
            colProps[col] = colProps[col] | is_attractive

           /*
            *  vertex col has no parents (and thus not a loop)
           */
         ;  if
            (  (Mt->cols+col)->n_ivps == 0
            )
            colProps[col] = colProps[col] | is_unattractive
         ;
         }

        /*
         *  compute closure of Bool "has attractive parent"
        */
      ;  for (col=0;col<n_cols;col++)
         isAttractive (Mt, colProps, col, 0, &maxdepth)
   ;  }

      startoffset = 0
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

         ;  mclpInstantiate(leafnodes->ivps+0,M->cols[startoffset].vid,1.0)

         ;  while (leafnodes->n_ivps)
            {  mclVector  *new_leafnodes = mclvInit(NULL)
            ;  mclIvp *leafivp, *leafl, *leafr

            ;  if (depth > dagdepth)
               dagdepth = depth

            ;  depth++

            ;  leafl = leafnodes->ivps+0
            ;  leafr = leafl + leafnodes->n_ivps

            ;  for (leafivp=leafl; leafivp < leafr; leafivp++)
               {  int leafoffset       =   mclxGetVectorOffset
                                          (  M
                                          ,  leafivp->idx
                                          ,  EXIT_ON_FAIL
                                          ,  -1
                                          )
                                          /* fixme: return & check */
               ;  colProps[leafoffset] =   colProps[leafoffset] | is_classified

               ;  if (colProps[leafoffset] & is_attractive)
                  {  mcldMerge          /* look forward */
                     (  new_leafnodes
                     ,  M->cols+leafoffset
                     ,  new_leafnodes
                     )
                  ;  mcldMerge          /* look backward too */
                     (  new_leafnodes
                     ,  Mt->cols+leafoffset
                     ,  new_leafnodes
                     )
               ;  }
                  else if (!(colProps[leafoffset] & is_attractive))
                  mcldMerge          /* look backward only */
                  (  new_leafnodes
                  ,  Mt->cols+leafoffset
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

      if (depthptr)
      *depthptr = dagdepth
   ;

     /*  NOTE this should work for empty matrix and zero clusters as well */

      {  clus2elem =    mclxAllocZero
                        (  mclvCanonical(NULL, n_cluster, 1.0)
                        ,  mclvCopy(NULL, A->dom_cols)
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
   ;  mclxFree(&Mt)

   ;  if (dagptr)
      *dagptr = M
   ;  else
      mclxFree(&M)

   ;  return clus2elem
;  }


int isAttractive
(  const mclMatrix*  Mt
,  int* colProps
,  int colidx
,  int depth
,  int* maxdepth
)
   {  int   i
   ;  mclVector* parents = Mt->cols+colidx

   ;  if (depth > *maxdepth)
      *maxdepth = depth

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
            ,  maxdepth
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
   ;  ipp->w_center     =  0.0
   ;  ipp->w_selfval    =  0.999
   ;  ipp->w_maxval     =  0.001
   ;  ipp->w_partialsum =  0.0

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


