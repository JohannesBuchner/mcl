/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <time.h>
#include <signal.h>
#include <string.h>

#include "proc.h"
#include "dpsd.h"
#include "expand.h"
#include "inflate.h"
#include "procinit.h"

#include "clew/clm.h"
#include "clew/cat.h"

#include "impala/io.h"
#include "impala/matrix.h"

#include "util/ting.h"
#include "util/err.h"
#include "util/io.h"
#include "util/types.h"
#include "util/alloc.h"
#include "util/minmax.h"
#include "util/compile.h"

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
(  const mclx*    mxstart
,  mclx**         mxin
,  mclx**         mxout
,  mclProcParam*  mpp
,  int            type
)  ;


void mclSigCatch
(  int sig
)
   {  if (sig == SIGALRM)
      abort_loop = 1
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

   ;  mpp->printDigits     =  3
   ;  mpp->printMatrix     =  0
   ;  mpp->expansionVariant=  0

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
(  mclMatrix** mxstart
,  mclProcParam* mpp
,  mcxbool constmx            /* if true do not free *mxstart */
,  mclx**  cachexp            /* if !NULL cache expanded */
,  mclx**  limit
)
   {  mclMatrix*        mxEven      =  *mxstart
   ;  mclMatrix*        mxOdd       =  NULL
   ;  mclMatrix*        mxCluster   =  NULL
   ;  int               n_cols      =  N_COLS(mxEven)
   ;  int               digits      =  mpp->printDigits
   ;  mclExpandParam    *mxp        =  mpp->mxp
   ;  int               i           =  0
   ;  clock_t           t1          =  clock()
   ;  const char* me                =  "mclProcess"
   ;  FILE*             fplog       =  mcxLogGetFILE()
   ;  mcxbool           log_gauge   =  mcxLogGet(MCX_LOG_GAUGE) 
   ;  mcxbool           log_stats   =  XPNVB(mpp->mxp, XPNVB_CLUSTERS)

   ;  if (cachexp)
      *cachexp =  NULL
   ;  if (limit)
      *limit   =  NULL
                                       /* mq check memleak for param and stats
                                        * structs and members
                                       */

   ;  if (!mxp->stats)                 /* size dependent init stuff */
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

   ;  if (log_gauge || log_stats)
      {  fprintf(fplog, " ite ")
      ;  if (log_gauge)
         {  for (i=0;i<n_cols/mxp->vectorProgression;i++)
            fputc('-', fplog)
         ;  fputs("  chaos  time hom(avg,lo,hi)", fplog)
      ;  }
         if (log_stats)
         fputs("   E/V  dd    cls   olap avg", fplog)
      ;  fputc('\n', fplog)
   ;  }

      for (i=0;i<mpp->initLoopLength;i++)
      {  doIteration 
         (  mxstart[0]
         ,  &mxEven
         ,  &mxOdd
         ,  mpp
         ,  ITERATION_INITIAL
         )

      ;  if
         (  (i == 0 && !constmx && !mpp->expansionVariant)
         || (i == 1 && !cachexp)
         ||  i > 1
         )
         mclxFree(&mxEven)
      ;  else if (i == 1 && cachexp)
         *cachexp = mxEven

      ;  mpp->n_ite++
      ;  mxEven  =  mxOdd
   ;  }

      if (mpp->initLoopLength)
      mcxLog
      (  MCX_LOG_MODULE
      ,  me
      ,  "====== Changing from initial to main inflation now ======"
      )

   ;  for (i=0;i<mpp->mainLoopLength;i++)
      {  int convergence
         =  doIteration
            (  mxstart[0]
            ,  &mxEven
            ,  &mxOdd
            ,  mpp
            ,  ITERATION_MAIN
            )

      ;  if
         (  mpp->initLoopLength
         || (i == 0 && !constmx && !mpp->expansionVariant)
         || (i == 1 && !cachexp)
         ||  i > 1
         )
         mclxFree(&mxEven)
      ;  else if (i == 1 && cachexp)
         *cachexp = mxEven

      ;  mpp->n_ite++
      ;  mxEven  =  mxOdd

      ;  if (abort_loop || convergence)
         {  if (XPNVB(mxp, XPNVB_PRUNING))
            fprintf(fplog, "\n")
         ;  break
      ;  }
      }

      if (cachexp && ! *cachexp)
      *cachexp = mxOdd

   ;  mpp->lap = ((double) (clock() - t1)) / CLOCKS_PER_SEC

   ;  *limit = mxEven

   ;  {  mclx* dag = mclDag(mxEven, mpp->ipp)
      ;  mxCluster = mclInterpret(dag)
      ;  mclxFree(&dag)
   ;  }

      return mxCluster
;  }


void mclInflate
(  mclx*    mx
,  double   power
,  mclv*    homgVec
)
   {  mcxbool   local  =   getenv("MCL_AUTO_LOCAL") ? TRUE : FALSE
   ;  mcxbool   smooth =   getenv("MCL_AUTO_SMOOTH") ? TRUE : FALSE  
   ;  mcxbool   test   =   getenv("MCL_AUTO_TEST") ? TRUE : FALSE  
   ;  mcxbool   dump   =   getenv("MCL_AUTO_DUMP") ? TRUE : FALSE

   ;  double    infl   =   power
   ;  mclv*     vec_infl = NULL
   ;  double hom_max = 0.0, hom_min = 10.0
   ;  dim k, i

   ;  if (local || smooth || test)
      {  hom_max = mclvMaxValue(homgVec)
      ;  hom_min = mclvMinValue(homgVec)
      ;  vec_infl = mclvCanonical(NULL, N_COLS(mx), power)
      ;  for (i=0;i<N_COLS(mx);i++)
         {  double avg = 0.0
         ;  double infllocal = power
         ;  mclv* nblist = mx->cols+i
         ;  mclp* ivp = NULL
         ;  dim j

         ;  if (smooth)
            for (j=0;j<nblist->n_ivps;j++)
            {  long idx = nblist->ivps[j].idx
            ;  if ((ivp = mclvGetIvp(homgVec, idx, ivp)))
               avg += nblist->ivps[j].val * ivp->val
         ;  }
            else
            {  if ((ivp = mclvGetIvp(homgVec, mx->cols[i].vid, ivp)))
               avg = ivp->val
         ;  }

if(dump)fprintf(stdout, "%d\t%.3f\n", (int) i, avg);

                  /* If homogeneity is low, the reasoning is
                   * that we can decrease inflation as apparently the local
                   * topology of the graph already encourages skew.
                   * Empirically, this seems to result in clusters that may be more
                   * elongated.
                   *
                   * Roughly speaking, we may expect high homogeneity to correlate
                   * with high clustering coefficient.
                  */

         ;  MCX_RESTRICT(avg, 0.333, 3)

         ;  infllocal = pow(infllocal, avg)

         ;  if (infllocal <= 1.1)
            infllocal = 1.1
         ;  if (infllocal >= 10.0)
            infllocal = 10.0

         ;  vec_infl->ivps[i].val  = infllocal
      ;  }
      }

      if (dump)
      fputc('\n', stdout)

   ;  for (k=0;k<N_COLS(mx);k++)
      mclvInflate(mx->cols+k, local || smooth ? vec_infl->ivps[k].val : infl)

   ;  mclvFree(&vec_infl)
;  }


int doIteration
(  const mclx*          mxstart
,  mclx**               mxin
,  mclx**               mxout
,  mclProcParam*        mpp
,  int                  type
)
   {  int               digits         =  mpp->printDigits
   ;  mclExpandParam*   mxp            =  mpp->mxp  
   ;  mclExpandStats*   stats          =  mxp->stats
   ;  int               bInitial       =  (type == ITERATION_INITIAL)
   ;  const char        *when          =  bInitial ? "initial" : "main"
   ;  dim               n_ite          =  mpp->n_ite
   ;  char              msg[80]
   ;  FILE*             fplog          =  mcxLogGetFILE()
   ;  double            inflation      =  bInitial
                                          ?  mpp->initInflation
                                          :  mpp->mainInflation
   ;  mcxbool           log_gauge      =  mcxLogGet(MCX_LOG_GAUGE) 
   ;  mcxbool           log_stats      =  XPNVB(mxp, XPNVB_CLUSTERS)
   ;  double            homgAvg
   ;  mclv*             homgVec

   ;  mxp->inflation = inflation

   ;  if (log_gauge || log_stats)
      fprintf(fplog, "%3d  ", (int) n_ite+1)

   ;  *mxout  =   mclExpand(*mxin, mpp->expansionVariant ? mxstart : *mxin,  mxp)
   ;  homgAvg =   mxp->stats->homgAvg

   ;  homgVec = mxp->stats->homgVec
   ;  mxp->stats->homgVec = NULL       /* fixme ugly ownership */

   ;  if (n_ite < 5)    /* fixme/document why just one? */
      mpp->marks[n_ite] = (int)(100.001*mxp->stats->mass_final_low[mxp->nj])

   ;  if (n_ite < mxp->nl)
      mclExpandAppendLog(mpp->massLog, mxp->stats, n_ite)

   ;  if (MCPVB(mpp, MCPVB_CHR))
      {  mclMatrix* chr = mxp->stats->mx_chr
      ;  dim k

      ;  for (k=0;k<N_COLS(chr);k++)
         ((chr->cols+k)->ivps+0)->val = mclvIdxVal((*mxout)->cols+k, k, NULL)
   ;  }

      if (log_gauge)
      fprintf
      (  fplog
      ,  " %6.2f %5.2f %.2f/%.2f/%.2f"
      ,  (double) stats->chaosMax
      ,  (double) stats->lap
      ,  (double) stats->homgAvg
      ,  (double) stats->homgMin
      ,  (double) stats->homgMax
      )

   ;  if (log_stats || MCPVB(mpp, (MCPVB_CLUSTERS | MCPVB_DAG)))
      {  dim o, m, e
      ;  mclMatrix* dag  = mclDag(*mxout, mpp->ipp)
      ;  mclMatrix* clus = mclInterpret(dag)
      ;  dim clm_stats[N_CLM_STATS]
      ;  int dagdepth = mclDagTest(dag)

      ;  clmStats(clus, clm_stats)

#if 0
;mcxIO *xfstdout = mcxIOnew("-", "w")
#endif
      ;  clmEnstrict(clus, &o, &m, &e, ENSTRICT_KEEP_OVERLAP)
      ;  if (log_stats)
         fprintf
         (  fplog
         ,  "%6.0f %2d %7lu %6lu %3.1f"
         ,     N_COLS(*mxout)
            ? (double) mclxNrofEntries(*mxout) / N_COLS(*mxout)
            :  0.0
         ,  dagdepth
         ,  (ulong) clm_stats[CLM_STAT_CLUSTERS]
         ,  (ulong) clm_stats[CLM_STAT_NODES_OVERLAP]
         ,  (double)
               (  clm_stats[CLM_STAT_NODES_OVERLAP]
               ?  clm_stats[CLM_STAT_SUM_OVERLAP] * 1.0 / clm_stats[CLM_STAT_NODES_OVERLAP]
               :  0.0
               )
         )
      ;  if (m+e)
         fprintf(fplog, " [!m=%lu e=%lu]", (ulong) m, (ulong) e)
#if 0
;if (o)
mclxWrite(*mxout, xfstdout, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
#endif
      ;  if (MCPVB(mpp, MCPVB_CLUSTERS))
         mclDumpMatrix(clus, mpp, "cls", "", n_ite+1, FALSE)
      ;  if (MCPVB(mpp, MCPVB_DAG))
         mclDumpMatrix(dag, mpp, "dag", "", n_ite+1, TRUE)
      ;  mclxFree(&dag)
      ;  mclxFree(&clus)
   ;  }

      if (log_gauge || log_stats)
      fputc('\n', fplog)

   ;  if (mpp->printMatrix)
      {  snprintf
         (  msg, sizeof msg, "%d%s%s%s"
         ,  (int) (2*n_ite+1), " After expansion (", when, ")"
         )
      ;  if (log_gauge)
         fprintf(stdout, "\n")
      ;  mclFlowPrettyPrint(*mxout, stdout, digits, msg)
   ;  }

      if (mpp->n_ite == 0 && mpp->fname_expanded)
      {  mcxIO* xftmp = mcxIOnew(mpp->fname_expanded->str, "w")
      ;  mclxWrite(*mxout, xftmp, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
      ;  mcxIOfree(&xftmp)
   ;  }

      if (XPNVB(mxp,XPNVB_PRUNING))
      {  if (mclVerbosityStart && XPNVB(mxp,XPNVB_EXPLAIN))
         {  mclExpandStatsHeader(fplog, stats, mxp)
         ;  mclVerbosityStart = 0
      ;  }
         mclExpandStatsPrint(stats, fplog)
   ;  }

      mclInflate(*mxout, inflation, homgVec)

   ;  mclvFree(&homgVec)

   ;  if (mpp->printMatrix)
      {  snprintf
         (  msg,  sizeof msg, "%d%s%s%s"
         ,  (int) (2*n_ite+2), " After inflation (", when, ")"
         )
      ;  if (log_gauge)
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
      ;  dim k

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
                              ?  (MCLX_DUMP_LINES | MCLX_DUMP_NOLEAD)
                              :  (MCLX_DUMP_PAIRS | MCLX_DUMP_VALUES)

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
      ;  dumper.threshold = 0.00001
      ;  if (cat)
         fprintf(xfdump->fp, "(mcldump %s %d\n", affix, (int) n)
      ;  mclxIOdump
         (  mx
         ,  xfdump
         ,  &dumper
         ,  mpp->dump_tab
         ,  mpp->dump_tab
         ,  MCLXIO_VALUE_GETENV
         ,  RETURN_ON_FAIL
         )
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
