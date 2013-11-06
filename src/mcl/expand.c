/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <math.h>
#include <limits.h>
#include <float.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "expand.h"

#include "util/alloc.h"
#include "util/types.h"
#include "util/heap.h"
#include "util/minmax.h"
#include "util/err.h"

#include "impala/compose.h"
#include "impala/ivp.h"
#include "impala/vector.h"
#include "impala/iface.h"

#include "taurus/ilist.h"


static const char* tags    =  "0123456789@??";

int mclDefaultWindowSizes[] =
{     1,       2,       5,
     10,      20,      50,
    100,     200,     500,
   1000,    2000,    5000,
  10000,   20000,   50000,
} ;
/*
 100000,  200000,  500000,
1000000, 2000000, 5000000,
*/

dim mcl_n_windows   =      sizeof mclDefaultWindowSizes
                        /  sizeof mclDefaultWindowSizes[0];

static int   levels[]
=
   {  80000
   ,  50000
   ,  30000
   ,  20000
   ,  12500

   ,  8000

   ,  5000
   ,  3000
   ,  2000
   ,  1250

   ,  800
   ,  500
   ,  300
   ,  200
   ,  125

   ,  -1
   }  ;


int n_levels   =  sizeof(levels)/sizeof(int) ;


static void levelAccount
(  mcxIL* il
,  int   size
)  ;


double mclExpandVector
(  const mclMatrix*  mx
,  const mclVector*  srvec       /* src                         */
,  mclVector*        dstvec      /* dst                         */
,  mclpAR*           ivpbuf      /* backup storage for recovery */
,  mclxComposeHelper*ch
,  long              col
,  mclExpandParam*   mxp
,  mclExpandStats*   stats
)  ;


                  /* homg is measured as                                      */
                  /*                                                          */
                  /*  mclvPowSum(src, 2.0) * mclvPowSum(dst, inflation) ** 2  */
                  /*  ------------------------------------------------        */
                  /*             mclvPowSum(dst, 2.0 * inflation)             */
                  /*                                                          */

static double get_homg
(  const mclv* src
,  const mclv* dst
,  double inflation
)
   {  if (getenv("MCL_AUTO_COSINE"))
      {  double ip = mclvIn(src, dst)
      ;  double nom = sqrt(mclvPowSum(src, 2.0) * mclvPowSum(dst, 2.0))
      ;  double invcosine = ip ? nom / ip : 1.0
      ;  return invcosine
   ;  }
      else
      {  double homg_num   =  mclvPowSum(src, 2.0) * pow(mclvPowSum(dst, inflation), 2.0)
      ;  double homg_nom   =  mclvPowSum(dst, 2.0 * inflation)
;if(0)fprintf(stdout, "-> %ld\t%.4f\t%.4f\n", (long) src->vid, homg_nom, homg_num)
      ;  return   homg_nom
               ?  homg_num / homg_nom
               :  1.0
   ;  }
   }


typedef struct
{  long              id
;  long              start
;  long              end
;  long              n_mod
;  mclExpandParam*   mxp
;  mclExpandStats*   stats
;  const mclx*       mxsrc
;  const mclx*       mxright
;  mclx*             mxdst
;  mclv*             chaosVec
;  mclv*             homgVec
;
}  mclExpandVectorLine_arg ;


static void vecMeasure
(  mclVector*        vec
,  double            *maxval
,  double            *center
)  ;


mclExpandParam* mclExpandParamNew
(  void
)  
   {  mclExpandParam *mxp  =  (mclExpandParam*) mcxAlloc
                              (  sizeof(mclExpandParam)
                              ,  EXIT_ON_FAIL
                              )

   ;  mxp->stats           =  NULL

   ;  mxp->n_ethreads      =  0

   ;  mxp->modePruning     =  MCL_PRUNING_RIGID
   ;  mxp->modeExpand      =  MCL_EXPAND_SPARSE

   ;  mxp->precision       =  0.000666
   ;  mxp->pct             =  0.95

   ;  mxp->num_prune       =  1444u /* should be overridden by scheme or user */
   ;  mxp->num_select      =  1444u /* should be overridden by scheme or user */
   ;  mxp->num_recover     =  1444u /* should be overridden by scheme or user */
   ;  mxp->scheme          =  6
   ;  mxp->my_scheme       =  -1

   ;  mxp->cutCof          =  4.0
   ;  mxp->cutExp          =  2.0

   ;  mxp->vectorProgression     =  20

   ;  mxp->warnFactor      =  50
   ;  mxp->warnPct         =  0.3

   ;  mxp->nx              =  3
   ;  mxp->ny              =  6
   ;  mxp->nj              =  6
   ;  mxp->nw              =  0
   ;  mxp->nl              =  10
   ;  mxp->windowSizes     =  NULL

   ;  mxp->verbosity       =  0
   ;  mxp->dimension       =  -1

   ;  mxp->inflation       =  -1.0

   ;  return mxp
;  }


void mclExpandParamFree
(  mclExpandParam** epp
)
   {  mclExpandParam *mxp = *epp
   ;  if (mxp->stats)
      mclExpandStatsFree(&(mxp->stats))
   ;  if (mxp->windowSizes)
      ilFree(&(mxp->windowSizes))
   ;  mcxFree(*epp)
   ;  *epp =  NULL
;  }


void* mclExpandVectorLine
(  void* arg
)
   {  mclExpandVectorLine_arg *a =  arg
   ;  const mclx*    mxsrc    =  a->mxsrc
   ;  const mclx*    mxright  =  a->mxright
   ;  mclx*          mxdst    =  a->mxdst
   ;  mclv*          chaosVec =  a->chaosVec
   ;  mclv*          homgVec  =  a->homgVec
   ;  long           colidx   =  a->start
   ;  mclExpandParam*  mxp    =  a->mxp
   ;  mclExpandStats* stats   =  a->stats
   ;  double homg_factor =  getenv("HOMG_USE_INFLATION") ? mxp->inflation : 2.0
   ;  clock_t        t1       =  clock(), t2

   ;  mclpAR* ivpbuf =  mclpARensure(NULL, N_ROWS(mxsrc))  
   ;  mclxComposeHelper*ch =  mclxComposePrepare(mxsrc, NULL)

   ;  for (colidx = a->start; colidx < a->end; colidx+=a->n_mod)
      {  double colInhomogeneity
         =  mclExpandVector
            (  mxsrc
            ,  mxright->cols + colidx
            ,  mxdst->cols + colidx
            ,  ivpbuf      /* backup storage for recovery */
            ,  ch
            ,  colidx
            ,  mxp
            ,  stats
            )

      ;  (homgVec->ivps+colidx)->val
         =  get_homg
            (  mxsrc->cols+colidx
            ,  mxdst->cols+colidx
            ,  homg_factor
            )
      ;  (chaosVec->ivps+colidx)->val = colInhomogeneity

      ;  if (!a->id)
         {  t2 = clock()
         ;  stats->lap += ((double) (t2 - t1)) / CLOCKS_PER_SEC
         ;  t1 = t2
      ;  }
      }

      mclpARfree(&ivpbuf)
   ;  mclxComposeRelease(&ch)

   ;  free(a)
   ;  return NULL
;  }


double mclExpandVector
(  const mclMatrix*  mx
,  const mclVector*  srcvec
,  mclVector*        dstvec
,  mclpAR*           ivpbuf
,  mclxComposeHelper*ch
,  long              col
,  mclExpandParam*   mxp
,  mclExpandStats*   stats
)
   {  dim            rg_n_expand    =  0
   ;  dim            rg_n_prune     =  0

   ;  mcxbool        rg_b_recover   =  FALSE
   ;  mcxbool        rg_b_select    =  FALSE

   ;  float          rg_mass_prune  =  0.0   /* used for heap, must be float */
   ;  float          rg_mass_final  =  1.0   /* same */

   ;  double         rg_sbar        =  -1.0     /*    select bar           */
   ;  double         rg_rbar        =  -1.0     /*    recovery bar         */

   ;  double         cut            =  0.0
   ;  mcxbool        mesg           =  FALSE
                     
   ;  double         maxval         =  0.0
   ;  double         center         =  0.0
   ;  double         cut_adapt      =  0.0
   ;  double         colInhomogeneity =  0.0

   ;  mcxbool        progress       =  mcxLogGet(MCX_LOG_GAUGE)

   ;  mclxVectorCompose
      (  mx
      ,  srcvec
      ,  dstvec
      ,  ch
      )

   ;  rg_n_expand          =  dstvec->n_ivps ? dstvec->n_ivps : 1

   ;  if (mxp->num_recover)
      {  memcpy(ivpbuf->ivps, dstvec->ivps, dstvec->n_ivps * sizeof(mclIvp))
      ;  ivpbuf->n_ivps = dstvec->n_ivps
   ;  }

      if (mxp->modePruning == MCL_PRUNING_RIGID)
      {  vecMeasure(dstvec, &maxval, &center)
      ;  if (mxp->precision)
         {  cut            =  mxp->precision
         ;  rg_mass_prune  =  mclvSelectGqBar (dstvec, cut)
      ;  }
         else
         rg_mass_prune  =  mclvSum(dstvec)

      ;  rg_n_prune        =  dstvec->n_ivps
      ;  rg_mass_final     =  rg_mass_prune
   ;  }

      else if (mxp->modePruning == MCL_PRUNING_ADAPT)
      {  vecMeasure(dstvec, &maxval, &center)
      ;  cut_adapt         =  (1/mxp->cutCof)
                              *  center
                              *  pow(center/maxval,mxp->cutExp)
      ;  cut               =  MCX_MAX(cut_adapt, mxp->precision)
      ;  rg_mass_prune     =  mclvSelectGqBar(dstvec, cut)
      ;  rg_n_prune        =  dstvec->n_ivps
      ;  rg_mass_final     =  rg_mass_prune
   ;  }
      
      else
      {  /* E.g. DENSE mode. */
         /* fixme; above two branches should be exhaustive [check] */
   ;  }

   ;  if
      (  mxp->warnFactor
      && (     mxp->warnFactor * MCX_MAX(dstvec->n_ivps,mxp->num_select)
            <  rg_n_expand
         && rg_mass_prune < mxp->warnPct
         )
      )
      {  mesg = TRUE
      ;  fprintf
         (  stderr,
         "\n"
         "___> Vector with idx [%ld], maxval [%.6f] and [%ld] entries\n"
         " ->  initially reduced to [%ld] entries with combined mass [%.6f].\n"
         " ->  Consider increasing the -P value and %s the -S value.\n"
         ,  (long) col
         ,  (double) maxval
         ,  (long) rg_n_expand
         ,  (long) dstvec->n_ivps
         ,  (double) rg_mass_prune
         ,  mxp->num_select ? "increasing" : "using"
         )
   ;  }

      if (!mxp->num_recover && !dstvec->n_ivps)
      {  mclvResize(dstvec, 1)
      ;  (dstvec->ivps+0)->idx = col
      ;  (dstvec->ivps+0)->val = 1.0
      ;  rg_mass_prune  =  1.0
      ;  rg_n_prune     =  1
      ;  if (mxp->warnFactor)
         fprintf(stderr, " ->  Emergency measure: added loop to node\n")
   ;  }

      if
      (  mxp->num_recover
      && (  dstvec->n_ivps <  mxp->num_recover)
      && (  rg_mass_prune  <  mxp->pct)
      )
      {  dim recnum     =  mxp->num_recover
      ;  rg_b_recover   =  TRUE
      ;  mclvRenew(dstvec, ivpbuf->ivps, ivpbuf->n_ivps)

      ;  if (dstvec->n_ivps > recnum)        /* use cut previously      */
         rg_rbar                             /* computed.               */
         =  mclvKBar                         /* we should check         */
         (  dstvec                           /* whether it is any use   */
         ,  recnum - rg_n_prune              /* (but we don't)          */
         ,  cut
         ,  KBAR_SELECT_LARGE
         )
      ;  else
         rg_rbar = 0.0

      ;  rg_mass_final     =  mclvSelectGqBar(dstvec, rg_rbar)
   ;  }

      else if (mxp->num_select && dstvec->n_ivps > mxp->num_select)
      {  double mass_select
      ;  int n_select
      ;  rg_b_select       =  TRUE

      ;  if (mxp->num_recover)         /* recovers to post prune vector */
         {  memcpy(ivpbuf->ivps, dstvec->ivps, dstvec->n_ivps * sizeof(mclIvp))
         ;  ivpbuf->n_ivps = dstvec->n_ivps
      ;  }

         if (dstvec->n_ivps >= 2*mxp->num_select)
         rg_sbar
         =  mclvKBar
            (  dstvec
            ,  mxp->num_select
            ,  FLT_MAX
            ,  KBAR_SELECT_LARGE
            )
      ;  else
         rg_sbar
         =  mclvKBar
            (  dstvec
            ,  dstvec->n_ivps - mxp->num_select + 1
            ,  -FLT_MAX          /* values < cut are already removed */
            ,  KBAR_SELECT_SMALL
            )

      ;  mass_select       =  mclvSelectGqBar(dstvec, rg_sbar)
      ;  rg_mass_final     =  mass_select
      ;  n_select          =  dstvec->n_ivps

      ;  if
         (  mxp->num_recover
         && (  dstvec->n_ivps  <  mxp->num_recover)
         && (  mass_select    <  mxp->pct)
         )
         {  dim recnum     =  mxp->num_recover
         ;  rg_b_recover   =  TRUE
         ;  mclvRenew(dstvec, ivpbuf->ivps, ivpbuf->n_ivps)

         ;  if (dstvec->n_ivps > recnum)        /* use cut previously   */
            rg_rbar                             /* computed.            */
            =  mclvKBar                         /* we should check      */
            (  dstvec                           /* whether it is any use*/
            ,  recnum - n_select                /* (but we don't)       */
            ,  rg_sbar
            ,  KBAR_SELECT_LARGE
            )
         ;  else
            rg_rbar = 0.0

         ;  rg_mass_final  =  mclvSelectGqBar(dstvec, rg_rbar)
      ;  }
      }

      if (mesg)
      fprintf
      (  stderr
      ,  " ->  (before rescaling) Finished with [%ld] entries and [%f] mass.\n"
      ,  (long) dstvec->n_ivps
      ,  (double) rg_mass_final
      )

      /* fixme at this stage I could have 0.0 mass and 0 entries
       * The code seems to work in that extreme boundary case (sometimes
       * achievable e.g. by '-I 30', but some of the adding loop block above
       * could be used over here. Before doing anything, a careful look
       * at the selection and recovery logic is needed though.
      */

   ;  if (rg_mass_final)
      mclvScale(dstvec, rg_mass_final)

   ;  colInhomogeneity  =  (maxval-center) * dstvec->n_ivps
   ;


     /*
      *  expansion threads only have read & write access to stats
      *  in the block below and nowhere else.
     */

     /*
      *  always fill the stuff, we need it for the jury marks
     */
      {  dim z
      ;  mclMatrix *chr = stats->mx_chr

      ;  if (mxp->n_ethreads)
         pthread_mutex_lock(&(stats->mutex))

      ;  levelAccount(stats->il_levels_expand, rg_n_expand)
      ;  levelAccount(stats->il_levels_prune, rg_n_prune)

      ;  ((chr->cols+col)->ivps+0)->val  =   -1.0  /* self is expensive */
      ;  ((chr->cols+col)->ivps+1)->val  =   -1.0  /* self is expensive */
      ;  ((chr->cols+col)->ivps+2)->val  =   center
      ;  ((chr->cols+col)->ivps+3)->val  =   maxval
      ;  ((chr->cols+col)->ivps+4)->val  =   rg_mass_final

      ;  for (z=0;z<stats->n_windows;z++)
         {  mcxHeapInsert((stats->windowPrune)+z, &rg_mass_prune)
         ;  mcxHeapInsert((stats->windowFinal)+z, &rg_mass_final)
      ;  }

         stats->mass_prune_all += rg_mass_prune
      ;  stats->mass_final_all += rg_mass_final

      ;  if (rg_b_select)
         stats->n_selects++

      ;  if (rg_b_recover)
         stats->n_recoveries++

      ;  if (rg_mass_final < mxp->pct)
         stats->n_below_pct++

      ;  if (progress)
         {  stats->n_cols++
         ;  if (stats->n_cols % mxp->vectorProgression == 0)
            fwrite(".", sizeof(char), 1, stderr)
         ,  fflush(stderr)
      ;  }

         if (mxp->n_ethreads)
         pthread_mutex_unlock(&(stats->mutex))
   ;  }

      return colInhomogeneity
;  }


mclMatrix* mclExpand
(  const mclMatrix*        mx
,  const mclMatrix*        mxright
,  mclExpandParam*         mxp
)
   {  mclMatrix*        sq
   ;  mclVector*        chaosVec, * homgVec
   ;  long              col      =  0
   ;  mclExpandStats*   stats    =  mxp->stats
   ;  clock_t           t1       =  clock(), t2
   ;  long              n_cols   =  N_COLS(mx)
   ;  double      homg_factor = getenv("HOMG_USE_INFLATION") ? mxp->inflation : 2.0

   ;  if (mxp->dimension < 0 || !stats || !mxp->windowSizes)
         mcxErr("mclExpand", "pbd: not correctly initialized")
         /* mclExpandParamDim probably not called */
      ,  mcxExit(1)

   ;  if (!mcldEquate(mx->dom_cols, mx->dom_rows, MCLD_EQT_EQUAL))
         mcxErr("mclExpand", "pbd: matrix not square")
      ,  mcxExit(1)

   ;  sq       =  mclxAllocZero
                  (  mclvCopy(NULL, mx->dom_rows)
                  ,  mclvCopy(NULL, mx->dom_cols)
                  )
   ;  chaosVec =  mclvCanonical(NULL, n_cols, 1.0)
   ;  homgVec  =  mclvCanonical(NULL, n_cols, 1.0)

   ;  mclExpandStatsReset(stats)

   ;  if (mxp->n_ethreads)
      {  pthread_t *threads_expand  
         =  mcxAlloc(mxp->n_ethreads*sizeof(pthread_t), EXIT_ON_FAIL)
      ;  pthread_attr_t  pthread_custom_attr
      ;  int i

      ;  pthread_mutex_init(&(stats->mutex), NULL)
      ;  pthread_attr_init(&pthread_custom_attr)

      ;  for (i=0;i<mxp->n_ethreads;i++)
         {  mclExpandVectorLine_arg *a = malloc(sizeof(mclExpandVectorLine_arg))
         ;  a->id          =  i

         ;  a->start       =  i
         ;  a->end         =  N_COLS(mx)
         ;  a->n_mod       =  mxp->n_ethreads

         ;  a->mxdst         =  sq
         ;  a->mxp         =  mxp
         ;  a->stats       =  stats
         ;  a->chaosVec    =  chaosVec
         ;  a->homgVec     =  homgVec

         ;  a->mxsrc       =  mx
         ;  a->mxright     =  mxright

                           /* TODO: non-overflow accounting of lap times */
         ;  pthread_create
            (  threads_expand+i
            ,  &pthread_custom_attr
            ,  mclExpandVectorLine
            ,  a
            )
      ;  }

         for (i = 0; i < mxp->n_ethreads; i++)
         {  pthread_join(threads_expand[i], NULL)
      ;  }
         mcxFree(threads_expand)
   ;  }      /* glob a is destroyed by mclExpandVectorLine */

      else
      {  mclpAR* ivpbuf = mclpARensure(NULL, N_ROWS(mx))  
      ;  mclxComposeHelper *ch = mclxComposePrepare(mx, NULL)

      ;  for (col=0;col<n_cols;col++)
         {  double colInhomogeneity
            =  mclExpandVector
               (  mx
               ,  mxright->cols+col
               ,  sq->cols+col
               ,  ivpbuf
               ,  ch
               ,  col
               ,  mxp
               ,  stats
               )
         ;  (chaosVec->ivps+col)->val = colInhomogeneity
         ;  (homgVec->ivps+col)->val
            =  get_homg
               (  mx->cols+col
               ,  sq->cols+col
               ,  homg_factor
               )

         ;  if (!((col+1) % 10))
            {  t2 = clock()
            ;  stats->lap += ((double) (t2 - t1)) / CLOCKS_PER_SEC
            ;  t1 = t2
         ;  }
         }

         mclpARfree(&ivpbuf)
      ;  mclxComposeRelease(&ch)
   ;  }

      if (chaosVec->n_ivps)
      {  stats->chaosMax =  mclvMaxValue(chaosVec)
      ;  stats->chaosAvg =  mclvSum(chaosVec) / chaosVec->n_ivps
      ;  stats->homgAvg  =  mclvSum(homgVec) / homgVec->n_ivps
      ;  stats->homgMax  =  mclvMaxValue(homgVec)
      ;  stats->homgMin  =  mclvMinValue(homgVec)
   ;  }

      if (getenv("MCL_SPEW"))    /* lisp homg: lame TODO fixfixmefixme : remove */
      {  dim i
      ;  for (i=0;i<N_COLS(mx);i++)
         {  mclv* nblist = mx->cols+i
         ;  long nb = -1
         ;  double homg = 0.0
         ;  dim j
         ;  for (j=0;j<nblist->n_ivps;j++)
            {  long ofs = mclvGetIvpOffset(homgVec, nblist->ivps[j].idx, nb)
            ;  if (ofs >=0)
               homg += nblist->ivps[j].val * homgVec->ivps[ofs].val
         ;  }
            fprintf(stdout, "%4ld -> avg %.2f, val %.2f\n", nblist->vid, homg, homgVec->ivps[i].val)
      ;  }
         fputs("===\n", stdout)
   ;  }

     /*
      *  All but the level accounting part of the registry.
      *  Transferral of information from registry to stats.
     */

      {  dim x, z

      ;  for (z=0;z<stats->n_windows;z++)
         {  mcxHeap* floatHeap = (stats->windowPrune)+z

         ;  if (floatHeap->n_inserted)
            {  float* flp = (float *) floatHeap->base  

            ;  for (x=0;x<floatHeap->n_inserted;x++)
               stats->mass_prune_low[z] += *(flp+x)

            ;  stats->mass_prune_low[z] /=  floatHeap->n_inserted
         ;  }

            floatHeap = (stats->windowFinal)+z

         ;  if (floatHeap->n_inserted)
            {  float* flp = (float *) floatHeap->base  

            ;  for (x=0;x<floatHeap->n_inserted;x++)
               stats->mass_final_low[z] += *(flp+x)

            ;  stats->mass_final_low[z] /=  floatHeap->n_inserted
         ;  }
         }

         if (n_cols)
            stats->mass_final_all  /=  n_cols
         ,  stats->mass_prune_all  /=  n_cols
   ;  }


     /*
      *  The level accounting part of the registry.
      *  Transferral of information from registry to stats.
     */

      {  int   x
      ;  mcxIL* ilx           =  stats->il_levels_expand
      ;  mcxIL* ilp           =  stats->il_levels_prune

      ;  int   n_levels_p     =  ilp->n
      ;  int   n_levels_x     =  ilx->n

      ;  char *xbuf           =  mcxAlloc(n_levels_x+1, EXIT_ON_FAIL)
      ;  char *pbuf           =  mcxAlloc(n_levels_p+1, EXIT_ON_FAIL)
      
      ;  pbuf[n_levels_p-1]   =  '\0'
      ;  xbuf[n_levels_x-1]   =  '\0'

      ;  ilAccumulate(ilp)
      ;  ilAccumulate(ilx)

      ;  for (x=0;x<n_levels_x-1;x++)
         {  int n   =   *(ilx->L+x)
         ;  int t   =   n_cols ? ((1000*n / n_cols)+50) / 100 : 0
         ;  xbuf[x] =   n == 0 ?  '_' :  tags[t]
      ;  }
         for (x=0;x<n_levels_p-1;x++)
         {  int  n  =   *(ilp->L+x)
         ;  int t   =   n_cols ? ((1000*n / n_cols)+50) / 100 : 0
         ;  pbuf[x] =   n == 0 ?  '_' : tags[t]
      ;  }

         stats->levels_expand =  mcxTingNew(xbuf) /* mq freed by whom? */
      ;  stats->levels_prune  =  mcxTingNew(pbuf) /* mq freed by whom? */
      ;  mcxFree(xbuf)
      ;  mcxFree(pbuf)
   ;  }

      mclvFree(&chaosVec)
   ;  stats->homgVec = homgVec
   ;  return sq
;  }


mclExpandStats* mclExpandStatsNew
(  dim   n_windows
,  int*  window_sizes
,  int   nx          /* one of the heaps */
,  int   ny          /* one of the heaps */
,  const mclVector* dom_cols
)  
   {  mclExpandStats* stats  =  (mclExpandStats*) mcxAlloc
                                (  sizeof(mclExpandStats)
                                ,  EXIT_ON_FAIL
                                )
   ;  dim z
   ;  stats->mx_chr        =  mclxCartesian
                              (  mclvCopy(NULL, dom_cols)
                              ,  mclvCanonical(NULL, 5, 1.0)
                              ,  0.0
                              )
   ;  stats->mass_final_low=  mcxAlloc(n_windows*sizeof(double), EXIT_ON_FAIL)
   ;  stats->mass_prune_low=  mcxAlloc(n_windows*sizeof(double), EXIT_ON_FAIL)

   ;  stats->n_windows     =  n_windows
   ;  stats->window_sizes  =  mcxAlloc(sizeof(int) * n_windows, EXIT_ON_FAIL)
   ;  memcpy(stats->window_sizes, window_sizes, n_windows * sizeof(int))

   ;  stats->windowPrune   =  mcxAlloc(n_windows*sizeof(mcxHeap), EXIT_ON_FAIL)
   ;  stats->windowFinal   =  mcxAlloc(n_windows*sizeof(mcxHeap), EXIT_ON_FAIL)

   ;  stats->homgVec       =  NULL

   ;  for(z=0;z<stats->n_windows;z++)
      {  mcxHeapNew
         (  (stats->windowPrune)+z
         ,  window_sizes[z]
         ,  sizeof(float)
         ,  fltCmp
         )
      ;  mcxHeapNew
         (  (stats->windowFinal)+z
         ,  window_sizes[z]
         ,  sizeof(float)
         ,  fltCmp
         )
   ;  }

      stats->nx   =  nx
   ;  stats->ny   =  ny

   ;  stats->levels_expand    =  NULL
   ;  stats->levels_prune     =  NULL
   ;  stats->il_levels_expand =  NULL
   ;  stats->il_levels_prune  =  NULL

   ;  stats->lap  =  0.0

   ;  mclExpandStatsReset(stats)       /* this initializes several members */
   ;  return stats
;  }


void mclExpandStatsReset
(  mclExpandStats* stats
)  
   {  dim z
   ;  stats->chaosMax         =  0.0
   ;  stats->chaosAvg         =  0.0
   ;  stats->homgMax          =  0.0
   ;  stats->homgMin          =  PVAL_MAX
   ;  stats->homgAvg          =  0.0
   ;  stats->n_selects        =  0
   ;  stats->n_recoveries     =  0
   ;  stats->n_below_pct      =  0
   ;  stats->n_cols           =  0
   ;  stats->lap              =  0.0

   ;  for (z=0;z<stats->n_windows;z++)
      {  stats->mass_final_low[z] =  0.0
      ;  stats->mass_prune_low[z] =  0.0
      ;  mcxHeapClean((stats->windowPrune)+z)
      ;  mcxHeapClean((stats->windowFinal)+z)
   ;  }

      stats->mass_final_all   =  0.0
   ;  stats->mass_prune_all   =  0.0

   ;  stats->il_levels_expand =  ilInstantiate
                                 (stats->il_levels_expand, n_levels, NULL)
   ;  stats->il_levels_prune  =  ilInstantiate
                                 (stats->il_levels_prune,n_levels, NULL)

   ;  mcxTingFree(&(stats->levels_expand))
   ;  mcxTingFree(&(stats->levels_prune))

   ;  mclvFree(&(stats->homgVec))
;  }


void mclExpandStatsFree
(  mclExpandStats** statspp
)  
   {  mclExpandStats* stats = *statspp

   ;  mcxTingFree(&(stats->levels_expand))
   ;  mcxTingFree(&(stats->levels_prune))

   ;  ilFree(&(stats->il_levels_expand))
   ;  ilFree(&(stats->il_levels_prune))

   ;  mcxFree(stats->mass_prune_low)
   ;  mcxFree(stats->mass_final_low)

   ;  mclxFree(&(stats->mx_chr))

   ;  mcxFree(stats->window_sizes)
   ;  mclvFree(&(stats->homgVec))

   ;  mcxNFree
      (  stats->windowPrune
      ,  stats->n_windows
      ,  sizeof(mcxHeap)
      ,  mcxHeapRelease
      )
   ;  mcxNFree
      (  stats->windowFinal
      ,  stats->n_windows
      ,  sizeof(mcxHeap)
      ,  mcxHeapRelease
      )
   ;  mcxFree(stats)
   ;  *statspp = NULL
;  }


void mclExpandStatsPrint
(  mclExpandStats*  stats
,  FILE*             fp
)
   {  fprintf
      (  fp
      ,  "#]%3d%3d%3d %3d%3d%3d %s %s %-7d %-7d %-7d\n"
      ,  (int) (100.0*stats->mass_prune_all)
      ,  (int) (100.0*stats->mass_prune_low[stats->ny])
      ,  (int) (100.0*stats->mass_prune_low[stats->nx])
      ,  (int) (100.0*stats->mass_final_all)
      ,  (int) (100.0*stats->mass_final_low[stats->ny])
      ,  (int) (100.0*stats->mass_final_low[stats->nx])
      ,  stats->levels_expand->str
      ,  stats->levels_prune->str
      ,  (int) stats->n_recoveries
      ,  (int) stats->n_selects
      ,  (int) stats->n_below_pct
      )
;  }


static void levelAccount
(  mcxIL*   il
,  int      size
)
   {  int   i  =  0
   ;  while (size < levels[i] && i<n_levels)
      i++
   ;  (*(il->L+i))++
;  }


static void vecMeasure
(  mclVector*  vec
,  double      *maxval
,  double      *center
)  
   {  mclIvp*  ivp      =  vec->ivps
   ;  int      n_ivps   =  vec->n_ivps
   ;  double   m        =  0.0
   ;  double   c        =  0.0

   ;  while (--n_ivps >= 0)
      {  double val      =  (ivp++)->val
      ;  c += val * val
      ;  if (val > m)
         m = val
   ;  }

      *maxval           =  m
   ;  *center           =  c
;  }


/* do the init stuff that depends on the dimension of the input graph */

void mclExpandParamDim
(  mclExpandParam*  mxp
,  const mclMatrix *mx
)
   {  dim n_windows     =  mcl_n_windows
   ;  dim n_nodes       =  N_COLS(mx)
   ;  int* window_sizes =  mclDefaultWindowSizes

   ;  if (mxp->nw)
      n_windows = MCX_MIN(n_windows, mxp->nw)

   ;  while ((dim) window_sizes[n_windows-1] >= n_nodes && n_windows > 1)
      n_windows--

   ;  mxp->windowSizes = ilInstantiate(mxp->windowSizes, n_windows, window_sizes)
   ;  mxp->dimension   = n_nodes

   ;  if (mxp->nx >= n_windows)
      {  if (0) mcxTell("mcl-proc", "setting nx to <%d>", (int) n_windows)
      ;  mxp->nx = n_windows -1
   ;  }
      if (mxp->ny >= n_windows)
      {  if (0) mcxTell("mcl-proc", "setting ny to <%d>", (int) n_windows)
      ;  mxp->ny = n_windows -1
   ;  }
      if (mxp->nj >= n_windows)
      {  if (0) mcxTell("mcl-proc", "setting nj to <%d>", (int) n_windows)
      ;  mxp->nj = n_windows -1
   ;  }

      mxp->stats  =  mclExpandStatsNew
                     (  mxp->windowSizes->n
                     ,  mxp->windowSizes->L
                     ,  mxp->nx
                     ,  mxp->ny
                     ,  mx->dom_cols
                     )
;  }


/* 
 *   for each extra set of (1,2,5 * 10^k), we need to add the
 *       '_ek_____.'   part, the
 *       ' 1  2  5|'   part, and the 
 *       '--------^'   part, the
 *       '         '   part (in the 'mass window' line).
 *   Also, the number that is now '63' and used in the shrink statements,
 *   increases by 9.
*/

void mclExpandInitLog
(  mcxTing* Log
,  mclExpandParam* mxp  
)
   {  mcxTing* line  =  mcxTingEmpty(NULL, 80)

   ;  if (!mxp->windowSizes)
         mcxErr("mclExpandInitLog PBD", "not correctly initialized")
         /* mclExpandParamDim probably not called */
      ,  mcxExit(1)

   ;  mcxTingWrite(line, "mcl pruning statistics\n")
   ;  if (0)  mcxTingKAppend(line, "-", 3*mxp->windowSizes->n+63)
   ;  mcxTingAppend
      (  line
      ,  "mass windows                                                    "
      )
   ;  mcxTingShrink(line, 3*mxp->windowSizes->n-63)
   ;  mcxTingAppend(line, "   footprint expand/prune\n")
   ;  mcxTingAppend
      (  line
      ,  "._e0_____._e1_____._e2_____._e3_____._e4_____._e5_____._e6_____."
      )
   ;  mcxTingShrink(line, 3*mxp->windowSizes->n-63)
   ;  mcxTingAppend(line, "    e4___e3___e2___.e4___e3___e2___ ..... #R\n")
   ;  mcxTingWrite(Log, line->str)

   ;  mcxTingWrite
      (  line
      ,  "| 1  2  5| 1  2  5| 1  2  5| 1  2  5| 1  2  5|"
          " 1  2  5| 1  2  5|"
      )
   ;  mcxTingShrink(line, 3*mxp->windowSizes->n-63)
   ;  *(line->str+(line->len-1)) = '|'
   ;  mcxTingAppend(line, "all 8532c8532c8532c 8532c8532c8532c ..... #S\n")
   ;  mcxTingAppend(Log, line->str)

   ;  mcxTingWrite
      (  line
      ,  "^--------^--------^--------^--------^--------^--------^--------^"
      )
   ;  mcxTingShrink(line, 3*mxp->windowSizes->n-63)
   ;  *(line->str+(line->len-1)) = '^'
   ;  mcxTingAppend
      (line, "--- ---------------^--------------- ----- #%\n")

   ;  mcxTingAppend(Log, line->str)
   ;  mcxTingFree(&line)
;  }


void mclExpandAppendLog
(  mcxTing* Log
,  mclExpandStats *s
,  int n_ite
)
   {  dim z
   ;  mcxTing* buf = mcxTingEmpty(NULL, 60)

   ;  for (z=0;z<s->n_windows;z++)
      mcxTingPrintAfter(buf, "%3d", (int) (100.0*s->mass_prune_low[z]))
   ;  mcxTingPrintAfter(buf, " %3d", (int)(100.0*s->mass_prune_all))

   ;  mcxTingAppend(Log, buf->str)

   ;  mcxTingAppend(Log, "                                ")
   ;  mcxTingPrint(buf, " %8d\n", (int) s->n_recoveries)

   ;  for (z=1;buf->str[z+1]==' ';z++)
      buf->str[z] = '.'

   ;  mcxTingAppend(Log, buf->str)

   ;  mcxTingEmpty(buf, 0)
   ;  for (z=0;z<s->n_windows;z++)
      mcxTingPrintAfter(buf, "%3d", (int) (100.0*s->mass_final_low[z]))
   ;  mcxTingPrintAfter(buf, " %3d ", (int)(100.0*s->mass_final_all))

   ;  mcxTingAppend(Log, buf->str)

   ;  mcxTingPrintAfter
      (  Log
      ,  "%s %s"
      ,  s->levels_expand->str
      ,  s->levels_prune->str
      )

   ;  mcxTingPrint(buf, " %8d\n", (int) s->n_selects)

   ;  for (z=1;buf->str[z+1]==' ';z++)
      buf->str[z] = '.'

   ;  mcxTingAppend(Log, buf->str)
   ;  mcxTingKAppend(Log, "-", 3* s->n_windows + 4)
   ;  mcxTingAppend(Log, " --------------- ---------------")

   ;  mcxTingPrint(buf, " %8d\n", (int) s->n_below_pct)

   ;  for (z=1;buf->str[z+1]==' ';z++)
      buf->str[z] = '-'

   ;  mcxTingAppend(Log, buf->str)

   ;  mcxTingPrint
      (  buf
      ,  "ite %d time %.2f chaos %.2f/%.2f homg %.2f/%.2f/%.2f\n"
      ,  (int) n_ite
      ,  (double) s->lap
      ,  (double) s->chaosMax
      ,  (double) s->chaosAvg
      ,  (double) s->homgMax
      ,  (double) s->homgMin
      ,  (double) s->homgAvg
      )

   ;  mcxTingAppend(Log, buf->str)

   ;  mcxTingKAppend(Log, "-", 3* s->n_windows + 45)
   ;  mcxTingAppend(Log, "\n")
   ;  mcxTingFree(&buf)
;  }


void mclExpandStatsHeader
(  FILE* vbfp
,  mclExpandStats* stats
,  mclExpandParam*  mxp
)  
   {  const char* pruneMode
      =  (mxp->modePruning == MCL_PRUNING_ADAPT)
         ?  "adaptive"
         :  "rigid"  
   ;  const char* pStatus
      =  (mxp->modePruning == MCL_PRUNING_ADAPT)
         ?  "minimum"
         :  "rigid"  
   ;

fprintf
(  vbfp
,  "The %s threshold value equals [%f] (-p <f> or -P <i>)).\n"
,  pStatus
,  (double) mxp->precision
)  ;

if (mxp->num_select)
fprintf
(  vbfp
,  "Exact selection prunes to at most %d positive entries (-S).\n"
,  (int) mxp->num_select
)  ;

if (mxp->num_recover)
fprintf
(  vbfp
,  "If %s thresholding leaves less than %2.0f%% mass (-pct) and less than\n"
   "%d entries, as much mass as possible is recovered (-R).\n"
,  pruneMode
,  (double) 100 * mxp->pct
,  (int) mxp->num_recover
)  ;

fprintf
(  vbfp
,  "\nLegend\n"
   "all: average over all cols\n"
   "ny:  window %d (-ny <i>), average over the worst %d cases.\n"
   "nx:  window %d (-nx <i>), average over the worst %d cases.\n"
   "<>:  recovery is %sactivated. (-R, -pct)\n"
   "[]:  selection is %sactivated. (-S)\n"
   "8532c: 'man mcl' explains.\n"
,  (int) stats->ny+1
,  (int) stats->window_sizes[stats->ny]
,  (int) stats->nx+1
,  (int) stats->window_sizes[stats->nx]
,  (mxp->num_recover) ? "" : "NOT "
,  (mxp->num_select) ? "" : "NOT "
)  ;

fprintf
(  vbfp
,  "\n"
"#]---------------------------------------------------------------------------\n"
"#] mass percentages  | distr of vec footprints       |#recover cases <>      \n"
"#]         |         |____ expand ___.____ prune ____||    #select cases []  \n"
"#]  prune  | final   |e4   e3   e2   |e4   e3   e2   ||       |    #below pct\n"
"#]all ny nx|all ny nx|8532c8532c8532c|8532c8532c8532c|V       V        V     \n"
"#]---------.---------.---------------.---------.-----.--------.-------.------\n"
)  ;

   }

