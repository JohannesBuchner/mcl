/*   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdio.h>
#include <math.h>

#include "clm.h"
#include "dpsd.h"
#include "interpret.h"

#include "impala/matrix.h"
#include "impala/compose.h"
#include "impala/scan.h"
#include "taurus/la.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/alloc.h"
#include "util/types.h"
#include "util/equate.h"
#include "util/err.h"



#ifdef DEBUG
#define DEBUG_DEFINED 1
#else
#define DEBUG 0
#endif


/* fixme: implement */
mclMatrix* mclcTop
(  mclVector*  dom
)  ;
mclMatrix* mclcBottom
(  mclVector*  dom
)  ;


mclMatrix* mclcProject
(  const mclMatrix*  cl
,  const mclVector*  dom
)
   {  mclMatrix* pr =   mclxSub
                        (cl, mclvCopy(NULL, cl->dom_cols), mclvCopy(NULL, dom))
   ;  int overlap=0, missing=0, empty=0
   ;  int total = mclcEnstrict(pr, &overlap, &missing, &empty, ENSTRICT_TRULY)

   ;  if (total != empty)
      mcxErr
      (  "mclcProject"
      ,  "input clustering on <%ld> elements is not a partition [o%dm%de%d]"
      ,  (long) N_ROWS(cl)
      ,  (int) overlap
      ,  (int) missing
      ,  (int) empty
      )

   ;  return pr
;  }


double mclcLogVariance
(  const mclMatrix*  cl
)
   {  double v = 0.0
   ;  int i

   ;  if (!N_ROWS(cl))
      return 0.0        /* fixme; some other value ? */

   ;  for (i=0;i<N_COLS(cl);i++)
      {  double n = cl->cols[i].n_ivps
      ;  if (n)
         v += n * log(n)
   ;  }
      return -v
;  }


void mclcVIDistance
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  const mclMatrix*  abmeet
,  double*     abdist
,  double*     badist
)
   {  int i, j
   ;  double varab   =  0.0
   ;  double vara    =  0.0
   ;  double varb    =  0.0
   ;  double n_elems =  N_ROWS(cla)

   ;  *abdist = 0.0
   ;  *badist = 0.0

   ;  if (!n_elems)
      return

   /* fixme?: test for enstriction ? */
   /* fixme-warning: using reals as ints in abmeet
    * this warning belongs in the caller really,
    * or where abmeet is produced
   */

   ;  if (!mcldEquate(cla->dom_rows, clb->dom_rows, MCLD_EQ_EQUAL))
      {  mcxErr
         (  "mclcVIDistance PBD"
         ,  "domains sized (%ld,%ld) differ"
         ,  (long) N_ROWS(cla)
         ,  (long) N_ROWS(clb)
         )
      ;  return
   ;  }

      for (i=0;i<N_COLS(abmeet);i++)
      {  mclVector   *vecmeets   =  abmeet->cols+i
      ;  mclVector   *bvec       =  NULL
      ;  double n_lft            =  cla->cols[i].n_ivps
      ;  double n_rgt
      ;  double n_meet

      ;  for (j=0;j<vecmeets->n_ivps;j++)
         {  n_meet   =  vecmeets->ivps[j].val
         ;  bvec     =  mclxGetVector
                        (  clb
                        ,  vecmeets->ivps[j].idx
                        ,  EXIT_ON_FAIL
                        ,  bvec
                        )
         ;  n_rgt =  bvec->n_ivps
         ;  if (n_rgt && n_lft)
            varab += n_meet * log (n_meet / (n_lft * n_rgt))
      ;  }
      }

   ;  vara  =  mclcLogVariance(cla)
   ;  varb  =  mclcLogVariance(clb)
   ;  *badist = (vara - varab) / n_elems
   ;  *abdist = (varb - varab) / n_elems
   ;  if (*badist <= 0.0)  /* for -epsilon case */
      *badist  =  0.0
   ;  if (*abdist <= 0.0)  /* for -epsilon case */
      *abdist  =  0.0
;  }


void mclcSJDistance
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  const mclMatrix*  abmeet
,  const mclMatrix*  bameet
,  int*        abdist
,  int*        badist
)
   {  int i, j

   ;  *abdist = 0
   ;  *badist = 0       /* fixme; don't use these for intermediate results */

   /* fixme?: test for enstriction ? */
   /* fixme-warning: using reals as ints. */

   ;  if (!mcldEquate(cla->dom_rows, clb->dom_rows, MCLD_EQ_EQUAL))
      {  mcxErr
         (  "mclcSJDistance PBD"
         ,  "domains sized (%ld,%ld) differ"
         ,  (long) N_ROWS(cla)
         ,  (long) N_ROWS(clb)
         )
      ;  return
   ;  }

      for (i=0;i<N_COLS(abmeet);i++)
      {  int         max            =  0
      ;  mclVector   *vecmeets      =  abmeet->cols+i

      ;  for (j=0;j<vecmeets->n_ivps;j++)
          if ((int) (vecmeets->ivps+j)->val > max)
           max = (int) (vecmeets->ivps+j)->val
      ;  *abdist += (cla->cols+i)->n_ivps - max
   ;  }

      for (i=0;i<N_COLS(bameet);i++)
      {  int         max         =  0
      ;  mclVector   *vecmeets   =  bameet->cols+i

      ;  for (j=0;j<vecmeets->n_ivps;j++)
          if ((int) (vecmeets->ivps+j)->val > max)
           max = (int) (vecmeets->ivps+j)->val
      ;  *badist += (clb->cols+i)->n_ivps - max
   ;  }
   }


mclMatrix*  mclcContingency
(  const mclMatrix*  cla
,  const mclMatrix*  clb
)
   {  mclMatrix  *clbt  =  mclxTranspose(clb)
   ;  mclMatrix  *ct   =  mclxCompose(clbt, cla, 0)
   ;  mclxFree(&clbt)
   ;  return ct
;  }


mclMatrix*  mclcMeet
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  mcxstatus   ON_FAIL
)
   {  int i, a, o, m, e, n_clmeet, i_clmeet
   ;  mclMatrix   *abmeet, *clmeet
   ;  const char* mepanic = "mclcMeet panic"

   ;  if
      (  mclcEnstrict((mclx*) cla, &o, &m, &e, ENSTRICT_REPORT_ONLY)
      || mclcEnstrict((mclx*) clb, &o, &m, &e, ENSTRICT_REPORT_ONLY)
      )
      {  if (ON_FAIL == RETURN_ON_FAIL)
         return NULL
      ;  else
            mcxErr(mepanic, "some clustering is not a partition")
         ,  mcxExit(1)
   ;  }

      if (!mcldEquate(cla->dom_rows, clb->dom_rows, MCLD_EQ_EQUAL))
      {  mcxErr
         (  mepanic
         ,  "domains sized (%ld, %ld) differ"
         ,  (long) N_ROWS(cla)
         ,  (long) N_ROWS(clb)
         )
      ;  return NULL
   ;  }

      abmeet      =     mclcContingency(cla, clb)
   ;  if (!abmeet)
      return NULL

   ;  i_clmeet    =     0
   ;  n_clmeet    =     mclxNrofEntries(abmeet)
   ;  clmeet      =     mclxAllocZero
                        (  mclvCanonical(NULL, n_clmeet, 1.0)
                        ,  mclvCopy(NULL, cla->dom_rows)
                        )

   ;  for (a=0;a<N_COLS(abmeet);a++)
      {  mclVector* vec    =  abmeet->cols+a
      ;  mclVector* bvec   =  NULL

      ;  for (i=0;i<vec->n_ivps;i++)
         {  long  b  =  (vec->ivps+i)->idx
         ;  bvec = mclxGetVector(clb, b, RETURN_ON_FAIL, bvec)

         ;  if (!bvec || i_clmeet == n_clmeet)
            {  mcxErr(mepanic, "internal math does not add up")
            ;  continue
           /*  little bit frightning */
         ;  }

            mcldMeet
            (  cla->cols+a
            ,  bvec
            ,  clmeet->cols+i_clmeet
            )
         ;  i_clmeet++
      ;  }
      }

      if (i_clmeet != n_clmeet)
      mcxErr(mepanic, "internal math does not substract")

   ;  return clmeet
;  }


void mclcJQDistance
(  const mclMatrix*  cla
,  const mclMatrix*  clb
,  const mclMatrix*  abmeet
,  double*     abdist
,  double*     badist
)
   {  int i, j
   ;  double sosqa  = 0.0
   ;  double sosqb  = 0.0
   ;  double sosqm  = 0.0

   ;  *abdist = 0
   ;  *badist = 0       /* fixme; don't use these for intermediate results */

   ;  if (!mcldEquate(cla->dom_rows, clb->dom_rows, MCLD_EQ_EQUAL))
      {  mcxErr
         (  "mclcJQDistance PBD"
         ,  "domains sized (%ld,%ld) differ"
         ,  (long) N_ROWS(cla)
         ,  (long) N_ROWS(clb)
         )
      ;  return
   ;  }

      for (i=0;i<N_COLS(abmeet);i++)
      {  mclVector  *vecmeets  =  abmeet->cols+i
      ;  for (j=0;j<vecmeets->n_ivps;j++)
         sosqm +=  pow((vecmeets->ivps+j)->val, 2)
   ;  }

      for (i=0;i<N_COLS(cla);i++)
      sosqa +=  pow(cla->cols[i].n_ivps, 2)

   ;  for (i=0;i<N_COLS(clb);i++)
      sosqb +=  pow(clb->cols[i].n_ivps, 2)

   ;  *abdist = sosqa - sosqm
   ;  *badist = sosqb - sosqm
;  }


/*
 *    Nrof clusters
 *    center size  (as a percentage)
 *    largest size (as a percentage)
 *    Nrof singletons
 *    dg coefficient
*/

mcxstatus clmGranularity
(  const mclMatrix* cl
,  double vals[8]
)
   {  mclvScore vscore
   ;  mclv* szs = mclxColSizes(cl, MCL_VECTOR_COMPLETE)
   ;  int i = szs->n_ivps
   ;  double dgi = 0.0      /* david-goliath index */
   ;  double n_sgl = -1.0
   ;  double david = 0.0

   ;  mclvScan(szs, &vscore)
   ;  mclvSort(szs, mclpValRevCmp)

   ;  while (--i >= 0)
      {  if (david < vscore.max_i)
         david += szs->ivps[i].val
      ;  if (n_sgl < 0.0 && szs->ivps[i].val > 1)
         n_sgl = szs->n_ivps - i - 1

      ;  if (david >= vscore.max_i && n_sgl >= 0.0)
         break
   ;  }

      if (n_sgl < 0.0)
      n_sgl = szs->n_ivps

   ;  dgi = szs->n_ivps - i

   ;  vals[0]  =  N_ROWS(cl)
   ;  vals[1]  =  N_COLS(cl)
   ;  vals[2]  =  vscore.max_i
   ;  vals[3]  =  vscore.sum_i ? vscore.ssq_i / vscore.sum_i : -1.0
   ;  vals[4]  =  N_COLS(cl) ?  N_ROWS(cl) / (double) N_COLS(cl) : -1.0
   ;  vals[5]  =  vscore.min_i
   ;  vals[6]  =  dgi
   ;  vals[7]  =  n_sgl

   ;  mclvFree(&szs)
   ;  return STATUS_OK
;  }


void clmGranularityPrint
(  FILE* fp
,  const char* info
,  double vals[8]
,  int do_header
)
   {  fprintf
      (  fp
      ,  
         "%s%s%s"
         "nodes=%.0f clusters=%.0f max=%.0f"
         " ctr=%.0f avg=%.0f min=%.0f DGI=%.0f sgl=%.0f\n"
         "%s"

      ,  do_header ? "(clm-granularity\n" : ""
      ,  info ? info : ""
      ,  info ? "\n" : ""

      ,  vals[0]
      ,  vals[1]
      ,  vals[2]
      ,  vals[3]
      ,  vals[4]
      ,  vals[5]
      ,  vals[6]
      ,  vals[7]

      ,  do_header ? ")\n" : ""
      )
;  }


void clmPerformancePrint
(  FILE* fp
,  const char* info
,  double vals[5]
,  int do_header
)
   {  fprintf
      (  fp
      ,  
         "%s%s%s"
         "efficiency=%.5f "
         "massfrac=%.5f "
         "areafrac=%.5f "
         "mxlinkavg=%.5f "
         "cllinkavg=%.5f\n"
         "%s"

      ,  do_header ? "(clm-performance\n" : ""
      ,  info ? info : ""
      ,  info ? "\n" : ""

      ,  vals[0]
      ,  vals[1]
      ,  vals[2]
      ,  vals[3]      /*  clmass / cledges covered */
      ,  vals[4]      /*  mxmass / mxnrofentries */

      ,  do_header ? ")\n" : ""
      )  ;
;  }


mcxstatus clmPerformance
(  const mclMatrix* mx
,  const mclMatrix* cl
,  double vals[5]
)
   {  double mxArea = N_COLS(mx) * (N_COLS(mx) -1)
   ;  mclxScore xscore

   ;  double massFraction, areaFraction, mxLinkWeight, clLinkWeight
   ;  double nrEdges, mxMass

   ;  double clArea   =  0.0
   ;  double clMass   =  0.0
   ;  double clEdgesCovered =  0.0

   ;  int c

   ;  mclxScan(mx, &xscore)
   ;  mxMass         =  xscore.sum_i
   ;  nrEdges        =  xscore.n_elem_i
   ;  mxLinkWeight   =  nrEdges ? mxMass / nrEdges : -1.0

   ;  for (c=0;c<N_COLS(cl);c++)
      {  mclVector *cvec   =  cl->cols+c
      ;  mclxSubScan
         (mx, cvec, cvec, &xscore)
      ;  clEdgesCovered   +=  xscore.n_elem_i
      ;  clMass           +=  xscore.sum_i
      ;  clArea           +=  cvec->n_ivps * (cvec->n_ivps -1)
   ;  }
      clLinkWeight  =  clEdgesCovered ? (clMass / clEdgesCovered) : -1

   ;  if (!mxArea)
      mxArea = -1.0
   ;  if (!clArea)
      clArea = -1.0

   ;  massFraction         =  mxMass ? clMass / mxMass : -1.0
   ;  areaFraction         =  mxArea ? clArea / mxArea : -1.0

   ;  vals[0] = mclxCoverage(mx, cl, NULL)   /* overlap: ok (node average) */
   ;  vals[1] = massFraction
   ;  vals[2] = areaFraction            /* overlap: can grow larger than 1 */
   ;  vals[3] = mxLinkWeight
   ;  vals[4] = clLinkWeight
        /* massFraction, overlap: counting edges in overlap doubly (or more) */
   ;  return STATUS_OK
;  }


mclMatrix*  mclcSeparate
(  const mclMatrix*  cl
)
   {  /* change mclcMeet to accept overlapping clusterings */
      return NULL
;  }



/* fixme
 *    -  why not a simple while loop ?
 *    -  why not 
*/

void mclx_grow_component
(  const mclx* mx
,  long  cidx     /*  cidx is already in cur; we don't know about its neighbours. */
,  mclv* cur
)
   {  mclv* vec = mclxGetVector(mx, cidx, RETURN_ON_FAIL, NULL)
   ;  mclv* proj = mclvResize(NULL, 0)

   ;  if (!vec)
      return

                              /* fixme; needs new mclvGetIvpAlien */
   ;  if (vec->n_ivps && mcldCountSet(cur, vec, MCLD_CT_RDIFF))
      {  mcldMinus(vec, cur, proj)
      ;  if (proj->n_ivps)
         {  int j
;if (DEBUG) fprintf
(stderr
,"<%ld add=%ld to=%ld>"
, cidx, (long) proj->n_ivps, (long) cur->n_ivps
)
         ;  mcldMerge(cur, proj, cur)
         ;  for (j=0;j<proj->n_ivps;j++)
            mclx_grow_component(mx, proj->ivps[j].idx, cur)
;if (DEBUG) fprintf(stderr, "</%ld level=%ld>", cidx, (long)cur->n_ivps)
      ;  }
      }
      else {
;if (DEBUG) fprintf(stderr, "[%ld]", (long) cidx);
      }
      mclvFree(&proj)
;  }


mclx* mclcComponents
(  const mclx* mx
,  const mclx* dom
)
   {  int i, c, n_cls = 0
   ;  mcxbool project = dom ? TRUE : FALSE
   ;  mclx* coco                                /* connected components */
   ;  mclv* cur

   ;  if (!mx || !mclxIsGraph(mx))
      return NULL

   ;  if (!project)
      {  dom = mclxAllocZero
               (mclvInsertIdx(NULL, 0, 1.0), mclvCopy(NULL, mx->dom_rows))
      ;  mclvCopy(dom->cols+0, mx->dom_rows)
   ;  }

      coco  =  mclxAllocZero
               (mclvCanonical(NULL, N_COLS(mx), 1.0), mclvCopy(NULL, mx->dom_rows))
   ;  cur   =  mclvInit(NULL)


   ;  for (c=0;c<N_COLS(dom);c++)
      {  mclv* clvec = dom->cols+c
      ;  const mclx* sub
                     =     project
                        ?  mclxSub(mx, mclvCopy(NULL, clvec), mclvCopy(NULL, clvec))
                        :  mx
      ;  mclv* done_these =project ? sub->dom_cols : dom->dom_rows 
      ;  mclp* civp = NULL
      ;  mclvMakeConstant(done_these, 0.5)
                                 /* fixme should not touch sub->dom_cols */

      ;  for (i=0;i<clvec->n_ivps;i++)
         {  long cidx = clvec->ivps[i].idx
         ;  civp = mclvGetIvp(done_these, cidx, civp)

         ;  if (!civp || civp->val > 1.0)
            continue

         ;  mclvInsertIdx(cur, cidx, 1.0)
         ;  mclx_grow_component(sub, cidx, cur)
         ;  mclvMakeConstant(cur, 1.0)
         ;  if (1)                  /* recentlychanged 06-026 */
            mclvUpdateMeet(done_these, cur, fltAdd)
         ;  else
            mclvAdd(done_these, cur, done_these)
         ;  mclvRenew(coco->cols+n_cls, cur->ivps, cur->n_ivps)
         ;  mclvResize(cur, 0)
;if (DEBUG) fprintf(stderr, "\n##\n##\n new cluster %ld\n\n", (long) n_cls)
         ;  n_cls++
      ;  }

         if (project)
         mclxFree((mclx**) &sub)

      ;  if (n_cls == N_COLS(coco) && c+1 < N_COLS(dom))
         {  mcxErr("mclcComponents", "ran out of space, fix me")
         ;  break
      ;  }
      }

      if (!project)
      mclxFree((mclMatrix**) &dom)   /* we did allocate it ourselves */

   ;  mclvResize(coco->dom_cols, n_cls)
   ;  coco->cols = mcxRealloc(coco->cols, n_cls * sizeof(mclv), RETURN_ON_FAIL)

   ;  mclxColumnsRealign(coco, mclvSizeRevCmp)

   ;  mclvFree(&cur)
   ;  return coco
;  }


   /*
    *    Remove overlap
    *    Add missing entries
    *    Remove empty clusters
    */

int  mclcEnstrict
(  mclMatrix*  cl
,  int         *overlap
,  int         *missing
,  int         *empty
,  mcxbits     flags
)
   {  int      c
   ;  mcxIL    *ct   =  ilInstantiate(NULL, N_ROWS(cl), NULL)
   ;  const char* me =  "mclcEnstrict"
   ;  mcxbool keep_olap =  flags & ENSTRICT_KEEP_OVERLAP

   ;  *overlap       =  0
   ;  *missing       =  0
   ;  *empty         =  0

   ;  for (c=0;c<N_COLS(cl);c++)
      {  mclIvp*  ivp      =  (cl->cols+c)->ivps
      ;  mclIvp*  ivpmax   =  ivp + (cl->cols+c)->n_ivps
      ;  int      olap     =  0
      ;  int      l        =  -1

      ;  while(ivp< ivpmax)
         {  if ((l = mclvGetIvpOffset(cl->dom_rows, ivp->idx, l)) < 0)
               mcxErr(me, "index <%ld> not in domain", (long) ivp->idx)
            ,  mcxExit(1)
                                         /* already seen (overlap) */
         ;  if (*(ct->L+l) > 0)
            {  olap++
            ;  if (!keep_olap)
               ivp->val    =  -1.0
         ;  }
            else
            *(ct->L+l) = 1

         ;  ivp++
         ;  l++
      ;  }

         *overlap  += olap

      ;  if (olap && !keep_olap)
         mclvSelectGqBar(cl->cols+c, 0.0)
   ;  }

      *missing = ilCountLtBar(ct, 1)
                /* ouch; this il code is still over the place */ 

   ;  if (*missing && !(flags & ENSTRICT_LEAVE_MISSING))
      {  int   z
      ;  int   y        =  0
      ;  long  newvid   =  mclvHighestIdx(cl->dom_cols) + 1
      ;  int   n_cols_new   =  N_COLS(cl) + 1
      ;  int   clus_n   =  N_COLS(cl)
      ;  mclVector* clus=  NULL

      ;  cl->cols       =  (mclVector*) mcxRealloc
                           (  cl->cols
                           ,  n_cols_new * sizeof(mclVector)
                           ,  EXIT_ON_FAIL
                           )
                          /*  room for extra column (representing cluster) */

     /*  the stuff above and below touches the members of cl,
      *  so it's pretty critical it be done right so that cl remains in
      *  consistent state.  It could also be done (equally efficiently) by
      *  creating a new matrix for the new cluster alone and using mclxMerge.
      *  However, the vector-filling for loop below would still remain, and one
      *  would have to
      *
      *  -  create new column domain consisting of newvid ivp.
      *  -  copy row domain
      *  -  create new matrix clsingle using mclxAllocZero
      *  -  fill the single column with the missing entries
      *  -  Use mclxMerge(cl, clsingle, fltAdd).
      *  -  free clsingle
     */

      ;  clus = cl->cols + n_cols_new -1
      ;  mclvInit(clus)                            /* make consistent state */
      ;  clus->vid = newvid                        /* give vector identity  */
      ;  mclvResize(clus, *missing)                /* size the thing        */
      ;  mclvInsertIdx(cl->dom_cols, newvid, 1.0)  /* update column domain  */

     /*  NOW N_COLS(cl) has changed (by mclvInsertIdx) */

      ;  for(z=0;z<ct->n;z++)
         {  if (*(ct->L+z) == 0)
            {  ((cl->cols+clus_n)->ivps+y)->idx =  cl->dom_rows->ivps[z].idx
            ;  ((cl->cols+clus_n)->ivps+y)->val =  1.0
            ;  y++
            ;  mcxErr
               ("enstrict", "missing entry %ld", (long) cl->dom_rows->ivps[z].idx)
         ;  }
         }
      }

      {  int   q  =  0

      ;  for (c=0;c<N_COLS(cl);c++)
         {  if ((cl->cols+c)->n_ivps > 0)
            {  if (c>q  && !(flags & ENSTRICT_KEEP_EMPTY))
               {  (cl->cols+q)->n_ivps    =  (cl->cols+c)->n_ivps
               ;  (cl->cols+q)->ivps      =  (cl->cols+c)->ivps
            ;  }
               q++
         ;  }
         }

      ;  if (q && !(flags & ENSTRICT_KEEP_EMPTY))
         {  *empty  =  N_COLS(cl) - q
         ;  cl->cols =  (mclVector*) mcxRealloc
                           (  cl->cols
                           ,  q*sizeof(mclVector)
                           ,  EXIT_ON_FAIL
                           )
         ;  mclvResize(cl->dom_cols, q)
                 /*  careful; this automatically effects N_COLS(cl) */
      ;  }
      }

      ilFree(&ct)
   ;  return (*overlap + *missing + *empty)
;  }


#ifdef DEBUG_DEFINED
#undef DEBUG_DEFINED
#else
#undef DEBUG
#endif

