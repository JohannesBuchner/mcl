/*  Copyright (C) 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "impala/ivp.h"
#include "impala/iface.h"
#include "impala/io.h"
#include "impala/version.h"

#include "taurus/parse.h"

#include "util/ting.h"
#include "util/equate.h"
#include "util/io.h"
#include "util/types.h"
#include "util/minmax.h"
#include "util/tok.h"
#include "util/opt.h"
#include "util/err.h"

#include "proc.h"
#include "procinit.h"

#define CHB(a,b,c,d,e,f,g) mcxOptCheckBounds("mcl-lib", a, b, c, d, e, f, g)

static const char* me = "mcl-lib";

enum
{  PROC_OPT_INITLENGTH = 1000
,  PROC_OPT_MAINLENGTH
,  PROC_OPT_INITINFLATION
,  PROC_OPT_MAININFLATION
,  PROC_OPT_XPNDINFLATION
,  PROC_OPT_SCHEME
,  PROC_OPT_MY_SCHEME
,  PROC_OPT_ETHREADS
                        ,  PROC_OPT_ADAPT
,  PROC_OPT_SHOW        =  PROC_OPT_ADAPT + 2
,  PROC_OPT_VERBOSITY
,  PROC_OPT_SILENCE
                        ,  PROC_OPT_PROGRESS
,  PROC_OPT_PRUNE       =  PROC_OPT_PROGRESS + 2
,  PROC_OPT_PPRUNE
,  PROC_OPT_RECOVER
,  PROC_OPT_MARK
,  PROC_OPT_SELECT
                        ,  PROC_OPT_PCT
,  PROC_OPT_NJ          =  PROC_OPT_PCT + 2
,  PROC_OPT_WARNFACTOR
                        ,  PROC_OPT_WARNPCT
,  PROC_OPT_DUMPSTEM    =  PROC_OPT_WARNPCT + 2
,  PROC_OPT_DUMP
,  PROC_OPT_DUMPSUBI
,  PROC_OPT_DUMPSUBD
,  PROC_OPT_DUMPDOM
,  PROC_OPT_DUMPINTERVAL
                        ,  PROC_OPT_DUMPMODULO
,  PROC_OPT_TRACK       =  PROC_OPT_DUMPMODULO + 2
,  PROC_OPT_TRACKI
,  PROC_OPT_TRACKM
,  PROC_OPT_DEVEL
,  PROC_OPT_THREADS
,  PROC_OPT_ITHREADS
,  PROC_OPT_NX
,  PROC_OPT_NY
,  PROC_OPT_NW
,  PROC_OPT_NL
,  PROC_OPT_ADAPTEXPONENT
,  PROC_OPT_ADAPTFACTOR
,  PROC_OPT_WEIGHT_MAXVAL
,  PROC_OPT_WEIGHT_SELFVAL

}  ;


mcxOptAnchor mclProcOptions[] =
{  {  "--track"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  PROC_OPT_TRACK
   ,  NULL
   ,  "does nothing"
   }
,  {  "-tracki"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_TRACKI
   ,  NULL
   ,  "does nothing"
   }
,  {  "-trackm"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_TRACKM
   ,  NULL
   ,  "does nothing"
   }
,  {  "--adapt"
   ,  MCX_OPT_DEFAULT
   ,  PROC_OPT_ADAPT
   ,  NULL
   ,  "(better don't use) apply adaptive first-level pruning"
   }
,  {  "-adapt-exponent"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_ADAPTEXPONENT
   ,  "<num>"
   ,  "parameter in constructing adaptive threshold"
   }
,  {  "-ae"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_ADAPTEXPONENT
   ,  "<num>"
   ,  "alias to -adapt-exponent"
   }
,  {  "-af"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_ADAPTFACTOR
   ,  "<num>"
   ,  "parameter in constructing adaptive threshold"
   }
,  {  "-adapt-factor"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_ADAPTFACTOR
   ,  "<num>"
   ,  "parameter in constructing adaptive threshold"
   }
,  {  "--show"
   ,  MCX_OPT_DEFAULT
   ,  PROC_OPT_SHOW
   ,  NULL
   ,  "(small graphs only [#<20]) dump iterands to *screen*"
   }
,  {  "-ei"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_XPNDINFLATION
   ,  "<num>"
   ,  "apply inflation first (use e.g. with --expand-only)"
   }
,  {  "-l"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_INITLENGTH
   ,  "<int>"
   ,  "length of initial run (default 0)"
   }
,  {  "-L"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_MAINLENGTH
   ,  "<int>"
   ,  "length of main run (default unbounded)"
   }
,  {  "-i"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_INITINFLATION
   ,  "<num>"
   ,  "initial inflation value (default 2.0)"
   }
,  {  "-I"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_MAININFLATION
   ,  "<num>"
   ,  "\tM!\tDmain inflation value (default 2.0)"
   }
,  {  "-v"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_VERBOSITY
   ,  "{pruning|explain|clusters|progress|all}"
   ,  "mode verbose"
   }
,  {  "-V"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_SILENCE
   ,  "{pruning|explain|clusters|progress|all}"
   ,  "mode silent"
   }
,  {  "-progress"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_PROGRESS
   ,  "<int>"
   ,  "length of progress bar or minus chunk size"
   }
,  {  "-devel"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_DEVEL
   ,  "<int>"
   ,  "development lever"
   }
,  {  "-t"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_THREADS
   ,  "<int>"
   ,  "thread number, inflation and expansion"
   }
,  {  "-ti"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_ITHREADS
   ,  "<int>"
   ,  "inflation thread number"
   }
,  {  "-te"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_ETHREADS
   ,  "<int>"
   ,  "\tM!\tDexpansion thread number, use with multiple CPUs"
   }
,  {  "-nj"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_NJ
   ,  "<int>"
   ,  "size of jury, bigger = friendlier"
   }
,  {  "-nx"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_NX
   ,  "<int>"
   ,  "width of nx pruning monitoring window (cf -v pruning)"
   }
,  {  "-ny"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_NY
   ,  "<int>"
   ,  "width of ny pruning monitoring window (cf -v pruning)"
   }
,  {  "-nw"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_NW
   ,  "<int>"
   ,  "upper bound on pruning monitoring window count"
   }
,  {  "-nl"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_NL
   ,  "<int>"
   ,  "upper bound on number of iterations pruning is logged"
   }
,  {  "-p"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_PRUNE
   ,  "<num>"
   ,  "the rigid pruning threshold"
   }
,  {  "-P"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_PPRUNE
   ,  "<int>"
   ,  "(inverted) rigid pruning threshold (cf -z)"
   }
,  {  "-S"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_SELECT
   ,  "<int>"
   ,  "select down to <int> entries if needed"
   }
,  {  "-R"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_RECOVER
   ,  "<int>"
   ,  "recover to maximally <int> entries if needed"
   }
,  {  "-M"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_MARK
   ,  "<int>"
   ,  "set recovery and selection both to <int>"
   }
,  {  "-pct"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_PCT
   ,  "<pct>"
   ,  "try recovery if mass is less than <pct>"
   }
,  {  "-scheme"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_SCHEME
   ,  "<int>"
   ,  "\tM!\tDuse a preset resource scheme (cf --show-schemes)"
   }
,  {  "-my-scheme"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_MY_SCHEME
   ,  "<int>"
   ,  "set tag for custom scheme (cf -P -R -S -pct)"
   }
,  {  "-wself"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_WEIGHT_SELFVAL
   ,  "<num>"
   ,  "intermediate iterand interpretation option"
   }
,  {  "-wmax"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  PROC_OPT_WEIGHT_MAXVAL
   ,  "<num>"
   ,  "intermediate iterand interpretation option"
   }
,  {  "-warn-pct"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_WARNPCT
   ,  "<pct>"
   ,  "warn if pruning reduces mass to <pct> weight"
   }
,  {  "-warn-factor"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_WARNFACTOR
   ,  "<int>"
   ,  "warn if pruning reduces entry count by <int>"
   }
,  {  "-dump"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMP
   ,  "<mode>"
   ,  "<mode> in chr|ite|cls|dag (cf manual page)"
   }
,  {  "-dump-subi"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPSUBI
   ,  "<spec>"
   ,  "dump the columns in <spec>"
   }
,  {  "-dump-subd"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPSUBD
   ,  "<spec>"
   ,  "dump columns in <spec> domains"
   }
,  {  "-dump-dom"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPDOM
   ,  "<mx>"
   ,  "domain matrix"
   }
,  {  "-dump-stem"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPSTEM
   ,  "<str>"
   ,  "use <str> to construct dump (file) names"
   }
,  {  "-ds"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPSTEM
   ,  "<str>"
   ,  "alias to -dump-stem"
   }
,  {  "-dump-interval"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPINTERVAL
   ,  "<int>:<int>"
   ,  "only dump for iterand indices in this interval"
   }
,  {  "-di"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPINTERVAL
   ,  "<int>:<int>"
   ,  "alias to -dump-stem"
   }
,  {  "-dump-modulo"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPMODULO
   ,  "<int>"
   ,  "only dump if the iterand index modulo <int> vanishes"
   }
,  {  "-dm"
   ,  MCX_OPT_HASARG
   ,  PROC_OPT_DUMPMODULO
   ,  "<int>"
   ,  "alias to -dump-modulo"
   }
,  {  NULL
   ,  0
   ,  0                       
   ,  NULL
   ,  NULL
   }
}  ;

static int  scheme[7][5]
=  
   {  {  3000,  400,  500, 90 }
   ,  {  4000,  500,  600, 90 }
   ,  {  5000,  600,  700, 90 }
   ,  {  6000,  700,  800, 90 }
   ,  {  7000,  800,  900, 90 }
   ,  { 10000, 1100, 1400, 90 }
   ,  { 10000, 1200, 1600, 90 }
   }  ;

static int           n_prune     =  -1;
static int           n_select    =  -1;
static int           n_recover   =  -1;
static int           n_scheme    =  -1;
static int           user_scheme =   0;
static int           n_pct       =  -1;

void makeSettings
(  mclExpandParam* mxp
)  ;

void  mclSetProgress
(  int n_nodes
,  mclProcParam* mpp
)
   {  mclExpandParam *mxp = mpp->mxp
   ;

     /* 
      *     because of initialization the condition below means that
      *     user has not used -progress flag or given it argument 0.
      *     This setup is very ugly, general problem when trying to
      *     make constituting elements of default settings interact.
     */
      if ((mxp->n_ethreads || mpp->n_ithreads) && !mxp->usrVectorProgression)
      {  BIT_ON(mxp->verbosity, XPNVB_MPROGRESS)
      ;  BIT_OFF(mxp->verbosity, XPNVB_VPROGRESS)
   ;  }

      if (mxp->vectorProgression)
      {  
         if (mxp->vectorProgression > 0)
         mxp->vectorProgression
         =  MAX(1 + (n_nodes -1)/mxp->vectorProgression, 1)
      ;  else
         mxp->vectorProgression = -mxp->vectorProgression
   ;  }
      else if
         (  !mxp->vectorProgression
         && n_nodes >= 2000
         && !mxp->n_ethreads
         && !mpp->n_ithreads
         && !XPNVB(mxp, XPNVB_MPROGRESS)
         )
      mcxTell
      (  me
      ,  "advice: for larger graphs such as this, -progress <n> will "
         "reflect progress"
      )
;  }


mclv* convert_spec
(  mcxTing* spec
,  long min
)
   {  char*       z       =   NULL
   ;  mcxTokFunc  tf

   ;  tf.opts = MCX_TOK_DEL_WS

   ;  if (mcxTokExpectFunc(&tf, spec->str, spec->len, &z, -1, -1, NULL))
      return NULL

   ;  return ilSpecToVec(tf.args->next, &min, NULL, RETURN_ON_FAIL)
;  }


mcxstatus consider_spec
(  mclProcParam* mpp
,  mcxTing* speci
,  mcxTing* specd
,  mcxIO*   xfdom
)
   {  mclv *veci = NULL, *vecd = NULL, *vect = NULL
   ;  mclx* dom = NULL
   ;  mcxstatus status = STATUS_FAIL

   ;  if (!speci && !specd && !xfdom)
      return STATUS_OK
   
   ;  while (1)
      {  if (specd)
         {  if (!xfdom)
            {  mcxErr(me, "-dump-subd <spec> needs -dump-dom <mx> option")
            ;  break
         ;  }
            else if (!(dom = mclxRead(xfdom, RETURN_ON_FAIL)))
            break

         ;  if (!(vect = convert_spec(specd, -2)))
            break

         ;  if (!(vecd = mclxUnionv(dom, vect, NULL)))
            break

         ;  mcldMerge(mpp->dump_list, vecd, mpp->dump_list)
      ;  }

         if (speci)
         {  if (!(veci = convert_spec(speci, 0)))
            break
         ;  mcldMerge(mpp->dump_list, veci, mpp->dump_list)
      ;  }

         status = STATUS_OK
      ;  if (mpp->dump_list->n_ivps)
         BIT_ON(mpp->dumping, MCPVB_SUB)
      ;  break
   ;  }

      mcxTingFree(&speci)
   ;  mcxTingFree(&specd)

   ;  mclvFree(&veci)
   ;  mclvFree(&vecd)
   ;  mclvFree(&vect)

   ;  mcxIOfree(&xfdom)
   ;  mclxFree(&dom)

   ;  return status
;  }


mcxstatus mclProcessInit
(  const mcxOption*  opts
,  mcxHash*          myOpts
,  mclProcParam*     mpp
)
   {  int               i        =  0
   ;  float             f        =  0.0
   ;  float             f_0      =  0.0
   ;  float             f_1      =  1.0
   ;  int               i_0      =  0
   ;  int               i_1      =  1
   ;  int               i_7      =  7
   ;  int               i_100    =  100
   ;  float             f_e1     =  1e-1
   ;  float             f_30     =  30.0
   ;  mclExpandParam    *mxp     =  mpp->mxp
   ;  mcxTing*          speci    =  NULL
   ;  mcxTing*          specd    =  NULL
   ;  mcxIO*            xfdom    =  NULL
   ;  const mcxOption*  opt

   ;  mcxOptPrintDigits  =  1

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = mcxOptFind(opt->anch->tag, myOpts)
      ;  mcxbool  vok = TRUE            /* value ok (not illegal) */
      ;  mcxbool  verbosity
      ;  mcxbits  bit = 0
      ;  const char* arg

      ;  if (!anch)     /* not in myOpts */
         continue

      ;  switch(anch->id)
         {  case PROC_OPT_TRACK
         :  mclTrackImpalaPruning   =  1
         ;  break
         ;

            case PROC_OPT_ADAPT
         :  mxp->modePruning = MCL_PRUNING_ADAPT
         ;  break
         ;

            case PROC_OPT_SHOW
         :  mpp->printMatrix  =  TRUE
         ;  break
         ;

#if 0
            case PROC_OPT_CACHE_XP
         :  mpp->fname_expanded =  mcxTingNew(opt->val)
         ;  break
         ;
#endif

            case PROC_OPT_INITINFLATION
         :  f = atof(opt->val)
         ;  vok = CHB(anch->tag, 'f', &f, fltGq, &f_e1, fltLq, &f_30)
         ;  if (vok && (f<1.1 || f>5.0))
            mcxWarn
            (  "mcl"
            ,  "warning: extreme/conceivable/normal ranges for -I are "
               "(1.0, 8.0] / [1.1, 5.0] / [1.2, 3.0]"
            )
         ;  if (vok) mpp->initInflation = f
         ;  break
         ;

            case PROC_OPT_MAININFLATION
         :  f =  atof(opt->val)
         ;  vok = CHB(anch->tag, 'f', &f, fltGq, &f_e1, fltLq, &f_30)
         ;  if (vok && (f<1.1 || f>5.0))
            mcxWarn
            (  "mcl"
            ,  "warning: extreme/conceivable/normal ranges for -I are "
               "(1.0, 8.0] / [1.1, 5.0] / [1.2, 3.0]"
            )
         ;  if (vok) mpp->mainInflation = f
         ;  break
         ;

            case PROC_OPT_XPNDINFLATION
         :  f = atof(opt->val)
         ;  vok = CHB(anch->tag, 'f', &f, fltGq, &f_e1, fltLq, &f_30)
         ;  if (vok && (f<1.1 || f>5.0))
            mcxWarn
            (  "mcl"
            ,  "warning: extreme/conceivable/normal ranges for -I are "
               "(1.0, 8.0] / [1.1, 5.0] / [1.2, 3.0]"
            )
         ;  if (vok) mpp->inflate_expanded = f
         ;  break
         ;

            case PROC_OPT_VERBOSITY
         :  case PROC_OPT_SILENCE
         :  verbosity = anch->id  == PROC_OPT_VERBOSITY ? TRUE : FALSE
         ;  arg = opt->val

         ;  if (strcmp(arg, "pruning") == 0)
            bit = XPNVB_PRUNING
         ;  else if (strcmp(arg, "explain") == 0)
            bit = XPNVB_EXPLAIN
         ;  else if (strcmp(arg, "clusters") == 0)
            bit = XPNVB_CLUSTERS
         ;  else if (strcmp(arg, "progress") == 0)
            bit = XPNVB_VPROGRESS
         ;  else if (strcmp(arg, "all") == 0)
            bit = ~0

         ;  if (verbosity)
            BIT_ON(mxp->verbosity, bit)
         ;  else
            BIT_OFF(mxp->verbosity, bit)
         ;  break
         ;

            case PROC_OPT_INITLENGTH
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok) mpp->initLoopLength = i
         ;  break
         ;

            case PROC_OPT_MAINLENGTH
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok) mpp->mainLoopLength = i
         ;  break
         ;

            case PROC_OPT_PROGRESS
         :  mxp->usrVectorProgression = atoi(opt->val)
         ;  if (!mxp->usrVectorProgression)
            {  mxp->vectorProgression = 0
            ;  BIT_ON(mxp->verbosity, XPNVB_MPROGRESS)
            ;  BIT_OFF(mxp->verbosity, XPNVB_VPROGRESS)
         ;  }
            else
            mxp->verbosity |= XPNVB_VPROGRESS
         ;  mxp->vectorProgression = mxp->usrVectorProgression
         ;  break
         ;

            case PROC_OPT_TRACKI
         :  if (  sscanf
                  (  opt->val,  "%d:%d"
                  ,  &mclTrackImpalaPruningOffset
                  ,  &mclTrackImpalaPruningBound
                  ) != 2
               )
            {  mcxErr
               (  me
               ,  "flag <-tracki> expects i:j format, j=0 denoting infinity"
               )
            ;  vok = FALSE
            /* hierverder mq, no bound checking */
         ;  }
            mclTrackImpalaPruning = 1
         ;  break
         ;

            case PROC_OPT_THREADS
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
            {  mxp->n_ethreads = i
            ;  mpp->n_ithreads = i
            ;  BIT_OFF(mxp->verbosity, XPNVB_VPROGRESS)
         ;  }
            break
         ;

            case PROC_OPT_ITHREADS
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
            {  mpp->n_ithreads = i
            ;  BIT_OFF(mxp->verbosity, XPNVB_PRUNING)
         ;  }
            break
         ;

            case PROC_OPT_ETHREADS
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
            {  mxp->n_ethreads = i
            ;  BIT_OFF(mxp->verbosity, XPNVB_PRUNING)
         ;  }
            break
         ;

            case PROC_OPT_NJ
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)
         ;  if (vok) mxp->nj = i-1
         ;  break
         ;

            case PROC_OPT_NX
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)
         ;  if (vok) mxp->nx = i-1
         ;  break
         ;

            case PROC_OPT_NY
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)
         ;  if (vok) mxp->ny = i-1
         ;  break
         ;

            case PROC_OPT_NL
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)
         ;  if (vok) mxp->nl = i
         ;  break
         ;

            case PROC_OPT_NW
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok) mxp->nw = i
         ;  break
         ;

            case PROC_OPT_TRACKM
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)
         ;  if (vok)
            {  mclTrackImpalaPruning = 1
            ;  mclTrackImpalaPruningInterval = i
         ;  }
            break
         ;

#if 0
            case PROC_OPT_DIGITS
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, intLq, &i_10)
         ;  if (vok) mpp->printDigits = i
         ;  break
         ;
#endif
            case PROC_OPT_PPRUNE
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
               n_prune =  i
            ,  user_scheme = 1
         ;  break
         ;

            case PROC_OPT_PRUNE
         :  f = atof(opt->val)
         ;  vok = CHB(anch->tag, 'f', &f, fltGq, &f_0, fltLq, &f_e1)
         ;  if (vok)
               n_prune = f ? (int) (1.0 / f) : 0
            ,  user_scheme = 1
         ;  break
         ;

            case PROC_OPT_WARNFACTOR
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok) mxp->warnFactor =  i
         ;  break
         ;

            case PROC_OPT_WARNPCT
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, intLt, &i_100)
         ;  if (vok) mxp->warnPct  =  ((double) i) / 100.0
         ;  break
         ;

            case PROC_OPT_WEIGHT_MAXVAL
         :  mpp->ipp->w_maxval = atof(opt->val)
         ;  break
         ;

            case PROC_OPT_WEIGHT_SELFVAL
         :  mpp->ipp->w_selfval = atof(opt->val)
         ;  break
         ;

            case PROC_OPT_MY_SCHEME
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
            mxp->my_scheme = i
         ;  break
         ;

            case PROC_OPT_SCHEME
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, intLq, &i_7)
         ;  if (vok)
            {  n_scheme    =  i-1
            ;  n_prune     =  scheme[n_scheme][0]
            ;  n_select    =  scheme[n_scheme][1]
            ;  n_recover   =  scheme[n_scheme][2]
            ;  n_pct       =  scheme[n_scheme][3]
            ;  mxp->scheme =  i
         ;  }
            break
         ;

            case PROC_OPT_PCT
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, intLt, &i_100)
         ;  if (vok)
               n_pct =  i
            ,  user_scheme = 1
         ;  break
         ;

            case PROC_OPT_MARK
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
               n_recover   =  i
            ,  n_select    =  i
            ,  user_scheme =  1
         ;  break
         ;

            case PROC_OPT_RECOVER
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
               n_recover   =  i
            ,  user_scheme = 1
         ;  break
         ;

            case PROC_OPT_SELECT
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_0, NULL, NULL)
         ;  if (vok)
               n_select =  i
            ,  user_scheme = 1
         ;  break
         ;

            case PROC_OPT_ADAPTEXPONENT
         :  f  =  atof(opt->val)
         ;  vok = CHB(anch->tag, 'f', &f, fltGq, &f_1, NULL, NULL)
         ;  if (vok)
            {  mxp->modePruning =  MCL_PRUNING_ADAPT
            ;  mxp->cutExp  =  f
         ;  }
            break
         ;

            case PROC_OPT_ADAPTFACTOR
         :  f  =  atof(opt->val)
         ;  vok = CHB(anch->tag, 'f', &f, fltGq, &f_1, NULL, NULL)
         ;  if (vok)
            {  mxp->modePruning    =  MCL_PRUNING_ADAPT
            ;  mxp->cutCof         =  f
         ;  }
            break
         ;

            case PROC_OPT_DEVEL
         :  mpp->devel = atoi(opt->val)
         ;  break
         ;

            case PROC_OPT_DUMPSTEM
         :  mcxTingWrite(mpp->dump_stem, opt->val)
         ;  break
         ;

            case PROC_OPT_DUMPSUBD
         :  specd = mcxTingPrint(NULL, "d(%s)", opt->val)
         ;  break
         ;

            case PROC_OPT_DUMPDOM
         :  xfdom = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case PROC_OPT_DUMPSUBI
         :  speci = mcxTingPrint(NULL, "i(%s)", opt->val)
         ;  break
         ;

            case PROC_OPT_DUMP
         :  arg = opt->val
         ;  if (strstr(arg, "chr"))
            BIT_ON(mpp->dumping, MCPVB_CHR)
         ;  if (strstr(arg, "ite"))
            BIT_ON(mpp->dumping, MCPVB_ITE)
         ;  if (strstr(arg, "cls"))
            BIT_ON(mpp->dumping, MCPVB_CLUSTERS)
         ;  if (strstr(arg, "dag"))
            BIT_ON(mpp->dumping, MCPVB_DAG)
         ;  if (strstr(arg, "lines"))
            BIT_ON(mpp->dumping, MCPVB_LINES)
         ;  if (strstr(arg, "cat"))
            BIT_ON(mpp->dumping, MCPVB_CAT)
         ;  if (strstr(arg, "labels"))
            BIT_ON(mpp->dumping, MCPVB_TAB)
         ;  break
         ;

            case PROC_OPT_DUMPINTERVAL
         :  if (!strcmp(opt->val, "all"))
            mpp->dump_bound = 1000
         ;  else if (sscanf(opt->val,"%d:%d",&mpp->dump_offset,&mpp->dump_bound)!=2)
            {  mcxErr
               (  me
               ,  "flag -dump-interval expects i:j format, j=0 denoting infinity"
               )
            ;  vok = FALSE
            /* hierverder mq, no bound checking */
         ;  }
            break
         ;

            case PROC_OPT_DUMPMODULO
         :  i = atoi(opt->val)
         ;  vok = CHB(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)
         ;  if (vok) mpp->dump_modulo = i
         ;  break
         ;

         }

         if (vok != TRUE)            
         return STATUS_FAIL
   ;  }

  /*
   * this does the scheme thing
  */
      makeSettings(mxp)

   ;  if (consider_spec(mpp, speci, specd, xfdom))
      /* this frees speci and specd and xfdom */
      return STATUS_FAIL

   ;  if (mclTrackImpalaPruning)
      {  mclTrackStreamImpala = mcxIOnew("-", "w")
      ;  mcxIOopen(mclTrackStreamImpala, EXIT_ON_FAIL)
   ;  }

      return STATUS_OK
;  }


void mclShowSchemes
(  void
)
   {  int i
   ;  fprintf
      (  stdout
      ,  "%20s%15s%15s%15s\n"  
      ,  "Pruning"
      ,  "Selection"
      ,  "Recovery"
      ,  "  Recover percentage"
      )
   ;  for (i=0;i<7;i++)
      fprintf
      (  stdout
      ,  "Scheme %1d%12d%15d%15d%15d\n"
      ,  i+1
      ,  scheme[i][0]
      ,  scheme[i][1]
      ,  scheme[i][2]
      ,  scheme[i][3]
      )
;  }


void makeSettings
(  mclExpandParam* mxp
)
   {  int s = mxp->scheme-1
   ;  mxp->num_prune    =  n_prune  >= 0  ?  n_prune :  scheme[s][0]

   ;  mxp->precision    =     mxp->num_prune
                           ?  0.99999 / mxp->num_prune
                           :  0.0

   ;  mxp->num_select   =  n_select < 0  ?  scheme[s][1] :  n_select
   ;  mxp->num_recover  =  n_recover< 0  ?  scheme[s][2] :  n_recover
   ;  mxp->pct          =  n_pct    < 0  ?  scheme[s][3] :  n_pct

   ;  if (user_scheme)
      mxp->scheme = 0          /* this interfaces to alg.c. yesitisugly */

   ;  mxp->pct         /=  100.0
;  }


void mclShowSettings
(  FILE* fp
,  mclProcParam* mpp
,  mcxbool user
)
   {  mclIvp ivps[10]
   ;  mclExpandParam *mxp = mpp->mxp

   ;  if (user)
      {  fprintf(fp, "[mcl] cell size: %u\n", (unsigned) (sizeof(ivps)/10))
      ;  fprintf
         (  fp
         ,  "[mcl] cell contents: "
            IVP_NUM_TYPE
            " and "
            IVP_VAL_TYPE
            "\n"
         )
      ;  fprintf(fp, "[mcl] largest index allowed: %ld\n", (long) PNUM_MAX)
      ;  fprintf(fp, "[mcl] smallest index allowed: 0\n")
   ;  }

      fprintf
      (  fp , "%-40s%8d%8s%s\n"
            ,  "Prune number"
            ,  mxp->num_prune
            ,  ""
            ,  "[-P n]"
      )

   ;  fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "Selection number"
            ,  mxp->num_select
            ,  ""
            , "[-S n]"
      )

   ;  fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "Recovery number"
            ,  mxp->num_recover
            ,  ""
            ,  "[-R n]"
      )

   ;  fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "Recovery percentage"
            ,  (int) (100*mxp->pct+0.5)
            ,  ""
            ,  "[-pct n]"
      )

   ;  if (user) fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "nx (x window index)"
            ,  mxp->nx + 1
            ,  ""
            ,  "[-nx n]"
      )

   ;  if (user) fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "ny (y window index)"
            ,  mxp->ny + 1
            ,   ""
            ,  "[-ny n]"
      )

   ;  if (user) fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "nj (jury window index)"
            ,  mxp->nj + 1
            ,   ""
            ,  "[-nj n]"
      )

   ;  if (user || mxp->modePruning == MCL_PRUNING_ADAPT) fprintf
      (  fp ,  "%-40s%11.2f%5s%s\n"
            ,  "adapt-exponent"
            ,  mxp->cutExp
            ,  ""
            ,  "[-ae f]"
      )

   ;  if (user || mxp->modePruning == MCL_PRUNING_ADAPT) fprintf
      (  fp ,  "%-40s%11.2f%5s%s\n"
            ,  "adapt-factor"
            ,  mxp->cutCof
            ,  ""
            ,  "[-af f]"
      )

   ;  if (user) fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "warn-pct"
            ,  (int) ((100.0 * mxp->warnPct) + 0.5)
            ,  ""
            ,  "[-warn-pct k]"
      )

   ;  if (user) fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "warn-factor"
            ,  mxp->warnFactor
            ,  ""
            ,  "[-warn-factor k]"
      )

   ;  if (user) fprintf
      (  fp ,  "%-40s%8s%8s%s\n"
            ,  "dumpstem"
            ,  mpp->dump_stem->str
            ,  ""
            ,  "[-dump-stem str]"
      )

   ;  if (user || mpp->initLoopLength) fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "Initial loop length"
            ,  mpp->initLoopLength
            ,  ""
            ,  "[-l n]"
      )

   ;  fprintf
      (  fp ,  "%-40s%8d%8s%s\n"
            ,  "Main loop length"
            ,  mpp->mainLoopLength
            ,  ""
            ,  "[-L n]"
      )

   ;  if (user || mpp->initLoopLength) fprintf
      (  fp ,  "%-40s%10.1f%6s%s\n"
            ,  "Initial inflation"
            ,  mpp->initInflation
            ,  ""
            ,  "[-i f]"
      )

   ;  fprintf
      (  fp ,  "%-40s%10.1f%6s%s\n"
            ,  "Main inflation"
            ,  mpp->mainInflation
            ,  ""
            ,  "[-I f]"
      )
;  }


void mclProcOptionsInit
(  void
)
   {  mcxOptAnchorSortById
      (mclProcOptions, sizeof(mclProcOptions)/sizeof(mcxOptAnchor) -1)
;  }

