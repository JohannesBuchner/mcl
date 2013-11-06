/*      Copyright (C) 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <string.h>
#include <stdio.h>

#include "report.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/tab.h"
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/scan.h"
#include "impala/app.h"

#include "mcl/interpret.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/opt.h"
#include "util/minmax.h"


enum
{  MY_OPT_ICL
,  MY_OPT_IMX
,  MY_OPT_INFLATION
,  MY_OPT_ST
,  MY_OPT_E_MAX
,  MY_OPT_E_POW
,  MY_OPT_E_SHIFT
,  MY_OPT_V_POW
,  MY_OPT_V_SHIFT
,  MY_OPT_N_LINE
,  MY_OPT_CAPTION
,  MY_OPT_HELP
,  MY_OPT_APROPOS
,  MY_OPT_VERSION
}  ;


const char* syntax = "Usage: clmps [options]";


mcxOptAnchor options[] =
{  {  "--version"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "output version information, exit"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "input graph file"
   }
,  {  "-icl"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ICL
   ,  "<fname>"
   ,  "input cluster file"
   }
,  {  "-I"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_INFLATION
   ,  "<num>"
   ,  "apply inflation to input graph"
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
,  {  "-v-shift"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_V_SHIFT
   ,  "<num>"
   ,  ""
   }
,  {  "-v-pow"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_V_POW
   ,  "<num>"
   ,  ""
   }
,  {  "-e-shift"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_E_SHIFT
   ,  "<num>"
   ,  ""
   }
,  {  "-e-pow"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_E_POW
   ,  "<num>"
   ,  ""
   }
,  {  "--st"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_ST
   ,  NULL
   ,  ""
   }
,  {  "-caption"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CAPTION
   ,  "<string>"
   ,  ""
   }
,  {  "-n-line"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_N_LINE
   ,  "<num>"
   ,  ""
   }
,  {  "-e-max"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_E_MAX
   ,  "<num>"
   ,  ""
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;





const char* usagelines[] =
{  "Usage: clmdag <options>"
,  NULL
}  ;

const char *me = "clmps";


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO *xf_cl = NULL, *xf_mx = NULL
   ;  mclx *mx = NULL, *mxt = NULL
   ;  mclv* sums = NULL, *diagv = NULL
   ;  int i, k=0, l
   ;  int n_print = 7, e_max = 16
   ;  double v_pow = 1.0, e_pow = 1.0, e_shift = 0.0, v_shift = 0.0
   ;  double inflation = 1.0
   ;  const char* caption = ""
   ;  mcxbool make_stochastic = FALSE

   ;  mcxOption* opts, *opt
   ;  mcxstatus parseStatus   =  STATUS_OK

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

      ;  switch(anch->id)
         {  case MY_OPT_VERSION
         :  app_report_version(me)
         ;  exit(0)
         ;

            case MY_OPT_ICL
         :  xf_cl =  mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_IMX
         :  xf_mx = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_INFLATION
         :  inflation = atof(opt->val)
         ;  break
         ;

            case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 0, 0, options)
         ;  exit(0)
         ;

            case MY_OPT_ST
         :  make_stochastic =  TRUE
         ;  break
         ;

            case MY_OPT_E_MAX
         :  e_max = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_E_POW
         :  e_pow = atof(opt->val)
         ;  break
         ;

            case MY_OPT_E_SHIFT
         :  e_shift = atof(opt->val)
         ;  break
         ;

            case MY_OPT_V_POW
         :  v_pow = atof(opt->val)
         ;  break
         ;

            case MY_OPT_V_SHIFT
         :  v_shift = atof(opt->val)
         ;  break
         ;

            case MY_OPT_CAPTION
         :  caption = opt->val
         ;  break
         ;

            case MY_OPT_N_LINE
         :  n_print = atoi(opt->val)
         ;  break
         ;

            default
         :  mcxDie(1, me, "most unusual") 
      ;  }
      }

      if (!xf_mx)
         mcxErr(me, "need cluster file")
      ,  mcxExit(1)

   ;  mx =  mclxReadx(xf_mx, EXIT_ON_FAIL, MCL_READX_GRAPH)
   ;  if (make_stochastic)
      mclxMakeStochastic(mx)
   ;  mxt = mclxTranspose(mx)
   ;  sums = mclxColSums(mxt, MCL_VECTOR_COMPLETE)
   ;  if (inflation - 1.0)
      mclxInflate(mx, inflation)
   ;  diagv = mclxDiagValues(mx, MCL_VECTOR_COMPLETE)

   ;  for (l=0;l<20;l++)
      {  double lo = (20-l-1)/20.0
      ;  double hi = lo + 0.05
      ;  for (i=0;i<N_COLS(mx);i++)
         {  int j
         ;  mclv* vec = mx->cols+i
         ;  if (e_max)
            mclvSort(vec, mclpValRevCmp)
         ;  for (j=0;j<vec->n_ivps&&(!e_max||j<e_max);j++,k++)
            {  mclp* ivp = vec->ivps+j
            ;  double ans = 1.0-pow(ivp->val, e_pow)

            ;  if (lo > ans || hi <= ans)
               continue

            ;  ans += e_shift

            ;  if (ivp->idx != vec->vid)
               fprintf
               (  stdout
               , "v%ld v%ld %.2f she ", (long) ivp->idx, (long) vec->vid, ans)

            ;  if (!((k+1)%n_print))
               fputs("\n", stdout)
         ;  }
            if (e_max)
            mclvSort(vec, mclpIdxCmp)
      ;  }
      }
      if (!((k+1)%n_print))
      fputs("\n", stdout)

   ;  for (i=0;i<sums->n_ivps;i++)
      {  double s = sums->ivps[i].val
      ;  double d = diagv->ivps[i].val
      ;  double sp = pow(s, v_pow)
      ;  long vid = sums->ivps[i].idx
      ;  double ans = v_shift + ((1.0 - d) / (1.0 + sp))

      ;  fprintf(stdout, "v%ld %.2f shn ", vid, ans)
      ;  if (!((i+1)%n_print))
         fputs("\n\n", stdout)
   ;  }

      if (!((i+1)%n_print))
      fputs("\n", stdout)

   ;  fprintf(stdout, "/caption (%s) def\n", caption)
   ;  return 0
;  }


