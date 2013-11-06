/*      Copyright (C) 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* modes of operation.
 *    --center    vary between center and max (dag only)
 *       default: vary between self and max (works for both ite and dag)
*/

#include <string.h>

#include "report.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/ivp.h"
#include "impala/io.h"
#include "impala/app.h"

#include "mcl/interpret.h"
#include "mcl/clm.h"

#include "util/types.h"
#include "util/err.h"
#include "util/ting.h"
#include "util/minmax.h"
#include "util/opt.h"
#include "util/array.h"


enum
{  MY_OPT_INPUTFILE
,  MY_OPT_OUTPUTFILE
,  MY_OPT_DAG
,  MY_OPT_TIGHT
,  MY_OPT_TSTEP
,  MY_OPT_TBOUND
,  MY_OPT_ENSTRICT
,  MY_OPT_SORT
,  MY_OPT_USE_CENTER
,  MY_OPT_HELP
,  MY_OPT_APROPOS
,  MY_OPT_VERSION
}  ;

const char* syntax = "Usage: clmimac [options]";

mcxOptAnchor options[] =
{  {  "--enstrict"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_ENSTRICT   
   ,  NULL
   ,  "clean up clustering if not a partition"
   }
,  {  "--version"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "output version information, exit"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUTFILE
   ,  "<fname>"
   ,  "output cluster file"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_INPUTFILE
   ,  "<fname>"
   ,  "input matrix file, presumably dumped mcl iterand or dag"
   }
,  {  "-t"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TIGHT
   ,  "<num>"
   ,  "(tightness) double in range [0.0,100.0]"
   }
,  {  "-tstep"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TSTEP
   ,  "<num>"
   ,  "step size for multiple tightness (default off)"
   }
,  {  "-tbound"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TBOUND
   ,  "<num>"
   ,  "bound on tightness in the presence of steps"
   }
,  {  "--apropos"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "this info"
   }
,  {  "-h"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_HELP
   ,  NULL
   ,  "this info"
   }
,  {  "-sort"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SORT
   ,  "<str>"
   ,  "sort mode for clusters, str one of <revsize|size|lex>"
   }
,  {  "-dag"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DAG
   ,  "<fname>"
   ,  "output DAG associated with matrix"
   }
,  {  "--center"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_USE_CENTER
   ,  NULL
   ,  "vary tightness over [center, max] interval"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxstatus parseStatus   =  STATUS_OK

   ;  mcxbool keep_overlap    =  TRUE
   ;  mcxbool use_center      =  FALSE
   ;  mclInterpretParam* ipp  =  mclInterpretParamNew()
   ;  mcxTing* fnstem         =  NULL
   ;  mcxIO* xfdag            =  NULL
   ;  const char* sortmode    =  "revsize"

   ;  int flags = 0

   ;  mcxIO* xfin
   ;  mclx *mx = NULL, *cl = NULL, *dag = NULL
   ;  int o, e, m
   ;  double t = 0.0
   ;  double toffset =  0.0         /* tight factor */
   ;  double tbound  =  100.0       /* tight factor */
   ;  double tstep   =  200.0       /* tight step */
   ;  const char* me = "clmimac"
   ;  const char* arg_fnin = NULL

   ;  mcxOption* opts, *opt

   ;  ipp->w_selfval = 0.999
   ;  ipp->w_maxval  = 0.001
   ;  ipp->w_center  = 0.0

   ;  if (argc <= 1)
      {  mcxOptApropos(stdout, me, syntax, 0, 0, options)
      ;  exit(0)
   ;  }

      mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)
   ;  opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)

   ;  if (parseStatus != STATUS_OK)
      {  mcxErr(me, "initialization failed")
      ;  exit(1)
   ;  }

      for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  if (anch->flags & MCX_OPT_UNUSED)          /* fixme, not functional */
         {  mcxWarn
            (  me
            ,  "EXPERIMENTAL using option <%s>, may produce unexpected results"
            ,  anch->tag
            )
      ;  }

         switch(anch->id)
         {  case MY_OPT_ENSTRICT
         :  keep_overlap = FALSE
         ;  break
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  exit(0)
         ;

            case MY_OPT_INPUTFILE
         :  arg_fnin = opt->val
         ;  break
         ;

            case MY_OPT_OUTPUTFILE
         :  fnstem = mcxTingNew(opt->val)
         ;  break
         ;

            case MY_OPT_USE_CENTER
         :  use_center = TRUE
         ;  break
         ;

            case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 0, 0, options)
         ;  exit(0)
         ;

            case MY_OPT_TBOUND
         :  tbound = atof(opt->val)
         ;  break
         ;

            case MY_OPT_TSTEP
         :  tstep = atof(opt->val)
         ;  break
         ;

            case MY_OPT_TIGHT
         :  toffset = atof(opt->val)
         ;  break
         ;

            case MY_OPT_DAG
         :  xfdag = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_SORT
         :  sortmode = opt->val
         ;  break
         ;

            default
         :  mcxExit(1) 
      ;  }
      }

      mcxOptFree(&opts)

   ;  if (!arg_fnin)
      {  mcxErr(me, "-imx option is required")
      ;  mcxExit(0)
   ;  }

      xfin     =  mcxIOnew(arg_fnin, "r")
   ;  mx       =  mclxReadx(xfin, EXIT_ON_FAIL, MCL_READX_GRAPH)
   ;  mclxMakeStochastic(mx)

   ;  if (!fnstem)
      fnstem = mcxTingNew("out.imac")
   ;

      for (t = toffset; t <= tbound; t += tstep)
      {  mcxIO* xfout
      ;  int dagdepth

      ;  xfout =  mcxIOnew(fnstem->str, "w")
      ;  if (tstep <= 100.0)
         mcxTingPrintAfter(xfout->fn, "%02.0f", t)

      ;  ipp->w_maxval = t / 100.0
      ;  ipp->w_partialsum = 0.0

      ;  if (use_center)
            ipp->w_selfval =  0.0
         ,  ipp->w_center  =  (100.0 - t) / 100.0
      ;  else
            ipp->w_selfval =  (100.0 - t) / 100.0
         ,  ipp->w_center  =  0.0

      ;  cl = mclInterpret(mx, ipp, &dagdepth, &dag)

      ;  mcxTell(me, "dagdepth <%d>", dagdepth)

      ;  if (xfdag)
         mclxWrite(dag, xfdag, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
      ;  else
         mclxFree(&dag)

      ;  if (keep_overlap)
         flags |= ENSTRICT_KEEP_OVERLAP

      ;  mclcEnstrict(cl, &o, &m, &e, flags)  

      ;  if (o)
         mcxTell
         (  me
         ,  "%s <%d> instances of overlap (t parameter %.0f)"
         ,  keep_overlap ? "found" : "removed"
         ,  (int) o
         ,  (double) t
         )

      ;  if (m)
         mcxTell
         (  me
         ,  "collected <%d> missing nodes (t parameter %.0f)"
         ,  (int) m
         ,  (double) t
         )

      ;  if (e)
         mcxTell
         (  me
         ,  "removed <%d> empty clusters (t parameter %.0f)"
         ,  (int) e
         ,  (double) t
         )

      ;  if (!strcmp(sortmode, "size"))
         mclxColumnsRealign(cl, mclvSizeCmp)
      ;  else if (!strcmp(sortmode, "revsize"))
         mclxColumnsRealign(cl, mclvSizeRevCmp)
      ;  else if (!strcmp(sortmode, "lex"))
         mclxColumnsRealign(cl, mclvLexCmp)
      ;  else if (!strcmp(sortmode, "none"))
        /* ok */
      ;  else
         mcxErr(me, "ignoring unknown sort mode <%s>", sortmode)

      ;  mclxWrite(cl, xfout, MCLXIO_VALUE_NONE, EXIT_ON_FAIL)
      ;  mcxIOfree(&xfout)
   ;  }

      return 0
;  }


