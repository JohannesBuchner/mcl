/*   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <time.h>
#include <signal.h>

#include "proc.h"
#include "clm.h"
#include "dpsd.h"
#include "expand.h"
#include "inflate.h"
#include "procinit.h"

#include "impala/io.h"
#include "impala/matrix.h"

#include "util/ting.h"
#include "util/err.h"
#include "util/io.h"
#include "util/types.h"
#include "util/alloc.h"
#include "util/minmax.h"

#define ITERATION_INITIAL  1
#define ITERATION_MAIN     2


static volatile sig_atomic_t abort_loop = 0;

static int mclVerbosityStart    =  1;        /* mq static */


void  mclDumpVector
(  mclProcParam*  mpp
,  const char*    affix
,  const char*    pfix
,  int            n
,  mcxbool        print_value
)  ;


int doIteration
(  mclMatrix**          mxin
,  mclMatrix**          mxout
,  mclProcParam*        mpp
,  int                  type
)  ;


void mclSigCatch
(  int sig
)
   {  abort_loop = 1
   ;  signal(sig, mclSigCatch)
;  }


mclProcParam* mclProcParamNew
(  void
)  
   {  mclProcParam* mpp    =  (mclProcParam*)
                              mcxAlloc(sizeof(mclProcParam), EXIT_ON_FAIL)
   ;  int i

   ;  mpp->mxp             =  mclExpandParamNew()
   ;  mpp->ipp             =  mclInterpretParamNew()

   ;  mpp->n_ithreads      =  0
   ;  mpp->fname_expanded  =  NULL

   ;  for (i=0;i<5;i++)
      mpp->marks[i]        =  100
   ;  mpp->massLog         =  mcxTingEmpty(NULL, 200)

   ;  mpp->devel           =  0

   ;  mpp->dumping         =  0
   ;  mpp->dump_modulo     =  1
   ;  mpp->dump_offset     =  0
   ;  mpp->dump_bound      =  5
   ;  mpp->dump_stem       =  mcxTingNew("")
   ;  mpp->dump_list       =  mclvInit(NULL)
   ;  mpp->dump_tab        =  NULL

   ;  mpp->chaosLimit      =  0.0001
   ;  mpp->lap             =  0.0
   ;  mpp->n_ite           =  0

   ;  mpp->vec_attr        =  NULL

   ;  mpp->mainInflation   =  2
   ;  mpp->mainLoopLength  =  10000
   ;  mpp->initInflation   =  2
   ;  mpp->initLoopLength  =  0

   ;  mpp->inflate_expanded=  0.0

   ;  mpp->printDigits     =  3
   ;  mpp->printMatrix     =  0

   ;  mpp->dimension       =  0
   ;  return mpp
;  }


void mclProcParamFree
(  mclProcParam** ppp
)
   {  mclProcParam* mpp = *ppp
   ;  mclExpandParamFree(&(mpp->mxp))
   ;  mclInterpretParamFree(&(mpp->ipp))
   ;  mcxTingFree(&(mpp->massLog))
   ;  mcxTingFree(&(mpp->dump_stem))
   ;  mclvFree(&(mpp->dump_list))
   ;  mcxFree(mpp)
   ;  *ppp = NULL
;  }


mclMatrix*  mclProcess
(  mclMatrix** mx0
,  mclProcParam* mpp
)
   {  mclMatrix*        mxEven      =  *mx0
   ;  mclMatrix*        mxOdd       =  NULL
   ;  mclMatrix*        mxCluster   =  NULL
   ;  int               n_cols      =  N_COLS(mxEven)
   ;  int               digits      =  mpp->printDigits
   ;  mclExpandParam    *mxp        =  mpp->mxp
   ;  int               i           =  0
   ;  FILE*             fv          =  stderr
   ;  clock_t           t1          =  clock()

                                       /* mq check memleak for param and stats
                                        * structs and members
                                       */
   ;  if (!mxp->stats)
      mclExpandParamDim(mxp, mxEven)

   ;  mclExpandInitLog(mpp->massLog, mxp)

   ;  if (mpp->printMatrix)
      mclFlowPrettyPrint
      (  mxEven
      ,  stdout
      ,  digits
      ,  "1 After centering (if) and normalization"
      )

   ;  if (MCPVB(mpp, MCPVB_ITE))
      mclDumpMatrix(mxEven, mpp, "ite", "", 0, TRUE)

   ;  if (XPNVB(mxp, XPNVB_VPROGRESS))
      {  
         if (!XPNVB(mxp, XPNVB_PRUNING))
         fprintf(fv, " ite ")

      ;  for (i=0;i<n_cols/mxp->vectorProgression;i++)
         fputc('-', fv)

      ;  fprintf(fv, XPNVB(mxp, XPNVB_PRUNING) ? "\n" : "  chaos  time\n")
   ;  }
      else if (XPNVB(mxp, XPNVB_MPROGRESS))
      fprintf(fv, " ite  chaos  time\n")

   ;  if (mpp->inflate_expanded)
      {  if (mpp->n_ithreads)
         mclxInflateBoss(mxEven, mpp->mainInflation, mpp)
      ;  else
         mclxInflate(mxEven, mpp->inflate_expanded)
   ;  }

      for (i=0;i<mpp->initLoopLength;i++)
      {  doIteration 
         (  &mxEven
         ,  &mxOdd
         ,  mpp
         ,  ITERATION_INITIAL
         )
      ;  mclxFree(&mxEven)
      ;  mpp->n_ite++
      ;  mxEven  =  mxOdd
   ;  }

      if (  mpp->initLoopLength
         && (  XPNVB(mxp, XPNVB_PRUNING)
            || XPNVB(mxp, XPNVB_VPROGRESS)
            )
         )
      fprintf
      (  fv
      ,  "====== Changing from initial to main inflation now ======\n"
      )

   ;  for (i=0;i<mpp->mainLoopLength;i++)
      {  int convergence
         =  doIteration
            (  &mxEven
            ,  &mxOdd
            ,  mpp
            ,  ITERATION_MAIN
            )

      ;  mclxFree(&mxEven)
      ;  mpp->n_ite++
      ;  mxEven  =  mxOdd

      ;  if (abort_loop || convergence)
         {  if (XPNVB(mxp, XPNVB_PRUNING))
            fprintf(fv, "\n")
         ;  break
      ;  }
      }

      mpp->lap = ((double) (clock() - t1)) / CLOCKS_PER_SEC

   ;  *mx0 = mxEven

   ;  {  mclx* dag = mclDag(mxEven, mpp->ipp)
      ;  mxCluster = mclInterpret(dag)
      ;  mclxFree(&dag)
   ;  }

      return mxCluster
;  }


int doIteration
(  mclMatrix**          mxin
,  mclMatrix**          mxout
,  mclProcParam*        mpp
,  int                  type
)
   {  int               digits         =  mpp->printDigits
   ;  FILE*             fv             =  stderr
   ;  mclExpandParam*   mxp            =  mpp->mxp  
   ;  mclExpandStats*   stats          =  mxp->stats
   ;  int               bInitial       =  (type == ITERATION_INITIAL)
   ;  const char        *when          =  bInitial ? "initial" : "main"
   ;  int               n_ite          =  mpp->n_ite
   ;  char              msg[80]
   ;  double            inflation      =  bInitial
                                          ?  mpp->initInflation
                                          :  mpp->mainInflation
 
            
   ;  if (XPNVB(mxp, XPNVB_VPROGRESS) && !XPNVB(mxp, XPNVB_PRUNING))
      fprintf(fv, "%3d  ", (int) n_ite+1)

   ;  *mxout =  mclExpand(*mxin, mxp)

   ;  if (n_ite >= 0 && n_ite < 5)
      mpp->marks[n_ite] = (int)(100.001*mxp->stats->mass_final_low[mxp->nj])

   ;  if (n_ite >= 0 && n_ite < mxp->nl)
      mclExpandAppendLog(mpp->massLog, mxp->stats, n_ite)

   ;  if (MCPVB(mpp, MCPVB_CHR))
      {  mclMatrix* chr = mxp->stats->mx_chr
      ;  int k

      ;  for (k=0;k<N_COLS(chr);k++)
         ((chr->cols+k)->ivps+0)->val
         =  mclvIdxVal((*mxout)->cols+k, k, NULL)
   ;  }

      if (mpp->printMatrix)
      {  snprintf
         (  msg, sizeof msg, "%d%s%s%s"
         ,  (int) 2*n_ite+1, " After expansion (", when, ")"
         )
      ;  if (XPNVB(mxp,XPNVB_VPROGRESS))
         fprintf(stdout, "\n")
      ;  mclFlowPrettyPrint(*mxout, stdout, digits, msg)
   ;  }

      if (XPNVB(mxp,XPNVB_VPROGRESS))
      {  if (XPNVB(mxp,XPNVB_PRUNING))
         fprintf
         (  fv
         ,  "\nchaos <%.2f> time <%.2f>\n"
         ,  (double) mxp->stats->chaosMax
         ,  (double) mxp->stats->lap
         )
      ;  else
         fprintf
         (fv, " %6.2f %5.2f\n", (double) stats->chaosMax, (double) stats->lap)
   ;  }
      else if (!XPNVB(mxp,XPNVB_PRUNING) && XPNVB(mxp,XPNVB_MPROGRESS))
      fprintf
      (  fv
      ,  "%3d  %6.2f %5.2f\n"
      ,  (int) n_ite+1
      ,  (double) stats->chaosMax
      ,  (double) stats->lap
      )

   ;  if (mpp->n_ite == 0 && mpp->fname_expanded)
      {  mcxIO* xftmp = mcxIOnew(mpp->fname_expanded->str, "w")
      ;  mclxWrite(*mxout, xftmp, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   ;  }

      if (XPNVB(mxp,XPNVB_PRUNING))
      {  if (mclVerbosityStart && XPNVB(mxp,XPNVB_EXPLAIN))
         {  mclExpandStatsHeader(fv, stats, mxp)
         ;  mclVerbosityStart = 0
      ;  }
         mclExpandStatsPrint(stats, fv)
   ;  }

      if (XPNVB(mxp, XPNVB_CLUSTERS) || MCPVB(mpp, (MCPVB_CLUSTERS | MCPVB_DAG)))
      {  int o, m, e
      ;  mclMatrix* dag  = mclDag(*mxout, mpp->ipp)
      ;  mclMatrix* clus = mclInterpret(dag)
      ;  int dagdepth = mclDagTest(dag)
      ;  mclcEnstrict(clus, &o, &m, &e, ENSTRICT_KEEP_OVERLAP)
      ;  fprintf
         (  stderr
         ,  "clusters <%d> overlap <%d> dagdepth <%d> m/e <%d/%d>\n"
         ,  (int) N_COLS(clus)
         ,  o
         ,  dagdepth
         ,  m
         ,  e
         )
      ;  if (MCPVB(mpp, MCPVB_CLUSTERS))
         mclDumpMatrix(clus, mpp, "cls", "", n_ite+1, FALSE)
      ;  if (MCPVB(mpp, MCPVB_DAG))
         mclDumpMatrix(dag, mpp, "dag", "", n_ite+1, TRUE)
      ;  mclxFree(&dag)
      ;  mclxFree(&clus)
   ;  }

      if (mpp->n_ithreads)
      mclxInflateBoss(*mxout, inflation, mpp)
   ;  else
      mclxInflate(*mxout, inflation)

   ;  if (mpp->printMatrix)
      {  snprintf
         (  msg,  sizeof msg, "%d%s%s%s"
         ,  (int) 2*n_ite+2, " After inflation (", when, ")"
         )
      ;  if (XPNVB(mxp, XPNVB_VPROGRESS))
         fprintf(stdout, "\n")
      ;  mclFlowPrettyPrint(*mxout, stdout, digits, msg)
   ;  }

   ;  if (MCPVB(mpp, MCPVB_ITE))
      mclDumpMatrix(*mxout, mpp, "ite", "", n_ite+1, TRUE)

   ;  if (MCPVB(mpp, MCPVB_SUB))
      {  mclx* sub = mclxExtSub(*mxout, mpp->dump_list, mpp->dump_list)
      ;  mclDumpMatrix(sub, mpp, "sub", "", n_ite+1, TRUE)
      ;  mclxFree(&sub)
   ;  }

      if (MCPVB(mpp, MCPVB_CHR))
      {  mclMatrix* chr = mxp->stats->mx_chr
      ;  int k

      ;  for (k=0;k<N_COLS(chr);k++)
         ((chr->cols+k)->ivps+1)->val
         =  mclvIdxVal((*mxout)->cols+k, k, NULL)

      ;  mclDumpMatrix(chr, mpp, "chr", "", n_ite+1, TRUE)
   ;  }

      if (stats->chaosMax < mpp->chaosLimit)
      return 1
   ;  else
      return 0
;  }


void mclDumpMatrix
(  mclMatrix*     mx
,  mclProcParam*  mpp
,  const char*    affix
,  const char*    postfix
,  int            n
,  mcxbool        print_value
)
   {  mcxIO*   xfdump
   ;  mcxTing*  fname
   ;  mcxbool   lines   =  MCPVB(mpp, MCPVB_LINES)
   ;  mcxbool   cat     =  MCPVB(mpp, MCPVB_CAT)

   ;  mcxbits   dump_modes =     !strcmp(affix, "result")
                              ?  MCLX_DUMP_RLINES
                              :  MCLX_DUMP_PAIRS | MCLX_DUMP_VALUES

   ;  if (!strcmp(affix, "result"))

   ;  else if (  (  mpp->dump_offset
            && (n<mpp->dump_offset)
            )
         || (  mpp->dump_bound
            && (n >= mpp->dump_bound)
            )
         || (  ((n-mpp->dump_offset) % mpp->dump_modulo) != 0)
         )
         return

   ;  if (cat)
      fname = mcxTingNew(mpp->dump_stem->str)
   ;  else
      fname
      =  mcxTingPrint
         (NULL, "%s-%d.%s%s", affix, (int) n, mpp->dump_stem->str, postfix)

   ;  xfdump = mcxIOnew(fname->str, cat ? "a" : "w")

   ;  if (mcxIOopen(xfdump, RETURN_ON_FAIL) != STATUS_OK)
      {  mcxErr
         ("mclDumpMatrix", "cannot open stream <%s>, ignoring", xfdump->fn->str)
      ;  return
   ;  }
      else if (lines)
      {  mclxIOdumper dumper
      ;  mclxIOdumpSet(&dumper, dump_modes, NULL, NULL, NULL)
      ;  dumper.threshold = 0.001
      ;  if (cat)
         fprintf(xfdump->fp, "(mcldump %s %d\n", affix, (int) n)
      ;  mclxIOdump
         (mx, xfdump, &dumper, mpp->dump_tab, mpp->dump_tab, RETURN_ON_FAIL)
      ;  if (cat)
         fprintf(xfdump->fp, ")\n")
   ;  }
      else
      {  mcxenum printMode = print_value?MCLXIO_VALUE_GETENV:MCLXIO_VALUE_NONE
      ;  mclxWrite(mx, xfdump, printMode, RETURN_ON_FAIL)
   ;  }

      mcxIOfree(&xfdump)
   ;  mcxTingFree(&fname)
;  }


void  mclDumpVector
(  mclProcParam*  mpp
,  const char*    affix
,  const char*    postfix
,  int            n
,  mcxbool        print_value
)
   {  mcxIO*   xf
   ;  mcxTing*  fname

   ;  if (  (  mpp->dump_offset
            && (n<mpp->dump_offset)
            )
         || (  mpp->dump_bound
            && (n >= mpp->dump_bound)
            )
         )
         return

   ;  fname = mcxTingNew(mpp->dump_stem->str)
   ;  mcxTingAppend(fname, affix)

   ;  mcxTingPrintAfter(fname, "%d", (int) n)
   ;  mcxTingAppend(fname, postfix)

   ;  xf =  mcxIOnew(fname->str, "w")
   ;  if (mcxIOopen(xf, RETURN_ON_FAIL) == STATUS_FAIL)
      {  mcxTingFree(&fname)
      ;  mcxIOfree(&xf)
      ;  return
   ;  }

      mclvaWrite(mpp->vec_attr, xf->fp, print_value ? 8 : MCLXIO_VALUE_NONE)
   ;  mcxIOfree(&xf)
   ;  mcxTingFree(&fname)
;  }


void mclProcPrintInfo
(  FILE* fp
,  mclProcParam* mpp  
)
   {  mclShowSettings(fp, mpp, FALSE)
;  }
