/*      Copyright (C) 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
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
#include "impala/scan.h"
#include "impala/io.h"
#include "impala/app.h"
#include "impala/iface.h"
#include "mcl/interpret.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/err.h"
#include "util/types.h"
#include "util/opt.h"
#include "util/minmax.h"


enum
{  MY_OPT_HELP
,  MY_OPT_APROPOS
,  MY_OPT_VERSION
,  MY_OPT_ADAPT
,  MY_OPT_APPEND_GR
,  MY_OPT_APPEND_PF
,  MY_OPT_PI
,  MY_OPT_RULE
,  MY_OPT_HEADER
,  MY_OPT_TAG
}  ;

const char* syntax = "Usage: clminfo [options] <mx file> <cl file>+";

mcxOptAnchor options[] =
{  {  "--version"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "output version information, exit"
   }
,  {  "-h"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_HELP
   ,  NULL
   ,  "this info (currently)"
   }
,  {  "--apropos"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "this info"
   }
,  {  "-pi"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_PI
   ,  "<num>"
   ,  "apply inflation with parameter <num> beforehand"
   }
,  {  "--adapt"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_ADAPT
   ,  NULL
   ,  "adapt cluster/matrix domains if they do not match"
   }
,  {  "--append-pf"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_APPEND_PF
   ,  NULL
   ,  "append performance measures to cluster file(s)"
   }
,  {  "--append-gr"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_APPEND_GR
   ,  NULL
   ,  "append granularity measures to cluster file(s)"
   }
,  {  "-tag"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAG
   ,  "<str>"
   ,  "write tag=<str> in performance measure"
   }
,  {  "--header"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_HEADER
   ,  NULL
   ,  "print header"
   }
,  {  "--rule"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_RULE
   ,  NULL
   ,  "print separating rule between results"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO *xfmx          =  NULL
   ;  mclMatrix *cl        =  NULL
   ;  mclMatrix *mx        =  NULL
   ;  const char* me       =  "clminfo"
   ;  const char* tag      =  NULL
   ;  double inflation     =  0.0
   ;  int preprune         =  0
   ;  mcxstatus parseStatus=  STATUS_OK

   ;  mcxbool do_header    =  FALSE
   ;  mcxbool do_rule      =  FALSE
   ;  mcxbool append_granul=  FALSE
   ;  mcxbool append_perf  =  FALSE
   ;  mcxbool adapt        =  FALSE

   ;  int a =  1
   ;  int n_arg_read = 0
   ;  int n_err = 0
   ;  mcxTing* ginfo = mcxTingEmpty(NULL, 40)
   ;  mcxOption* opts, *opt

   ;  if (argc <= 1)
      {  mcxOptApropos(stdout, me, syntax, 0, 0, options)
      ;  exit(0)
   ;  }

      mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)
   ;  opts = mcxOptExhaust
            (options, (char**) argv, argc, 1, &n_arg_read, &parseStatus)

   ;  if (parseStatus != STATUS_OK)
      {  mcxErr(me, "initialization failed")
      ;  exit(1)
   ;  }

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  mcxOptApropos(stdout, me, syntax, 0, MCX_OPT_DISPLAY_SKIP, options)
         ;  exit(0)
         ;

            case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 0, MCX_OPT_DISPLAY_SKIP, options)
         ;  exit(0)
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  exit(0)
         ;

            case MY_OPT_ADAPT
         :  adapt = TRUE
         ;  break
         ;

            case MY_OPT_APPEND_GR
         :  append_granul = TRUE
         ;  break
         ;

            case MY_OPT_APPEND_PF
         :  append_perf = TRUE
         ;  break
         ;

            case MY_OPT_PI
         :  inflation = atof (opt->val)
         ;  break
         ;

            case MY_OPT_RULE
         :  do_rule = TRUE 
         ;  break
         ;

            case MY_OPT_HEADER
         :  do_header = TRUE 
         ;  break
         ;

            case MY_OPT_TAG
         :  tag = argv[a]
         ;  break
         ;

            default
         :  mcxExit(1) 
         ;
         }
      }

      a = 1 + n_arg_read

   ;  if (a + 1 >= argc)
      mcxDie(1, me, "need matrix file and cluster file")

   ;  xfmx  =  mcxIOnew(argv[a++], "r")
   ;  mx    =  mclxReadx(xfmx, EXIT_ON_FAIL, MCL_READX_GRAPH)

   ;  if (tag)
      mcxTingPrint
      (  ginfo
      ,  "tag=%s\n"
      ,  tag
      )

   ;  mcxTingPrintAfter
      (  ginfo
      ,  "target-name=%s\n"
      ,  xfmx->fn->str
      )

   ;  if (inflation)
      {  mclxInflate(mx, inflation)
      ;  mcxTingPrintAfter(ginfo, "inflation=%.2f\n", (double) inflation)
   ;  }

      if (preprune)
      {  mclxMakeSparse(mx, 0, preprune)
      ;  mcxTingPrintAfter(ginfo, "preprune=%df\n", (int) preprune)
   ;  }

      mcxIOfree(&xfmx)

   ;  while(a < argc)
      {  mcxIO *xfcl =  mcxIOnew(argv[a], "r")
      ;  mclMatrix* tmx = NULL
      ;  int o, m, e
      ;  cl =  mclxRead(xfcl, EXIT_ON_FAIL)

      ;  if (!mcldEquate(mx->dom_cols, cl->dom_rows, MCLD_EQ_EQUAL))
         {  mclVector* meet = mcldMeet(mx->dom_cols, cl->dom_rows, NULL)
         ;  mcxErr
            (  me
            ,  "Domain mismatch for matrix ('left') and clustering ('right')"
            )
         ;  report_domain(me, N_ROWS(mx), N_ROWS(cl), meet->n_ivps)
         ;  if (adapt)
            {  mclMatrix* tcl = mclxSub(cl, cl->dom_cols, meet)
            ;  report_fixit(me, n_err++)
            ;  mclxFree(&cl)
            ;  cl  = tcl
            ;  tmx = mx
            ;  mx  = mclxSub(tmx, meet, meet)
         ;  }
            else
            report_exit(me, SHCL_ERROR_DOMAIN)
      ;  }

         if (mclcEnstrict(cl, &o, &m, &e, ENSTRICT_KEEP_OVERLAP))
         {  report_partition(me, cl, xfcl->fn, o, m, e)
         ;  if (m || e)
               adapt
            ?  report_fixit(me, n_err++)
            :  report_exit(me, SHCL_ERROR_PARTITION)
      ;  }

         mcxIOclose(xfcl)

      ;  {  double vals[8]
         ;  mcxTing* linfo = mcxTingNew(ginfo->str)
         ;  mcxTingPrintAfter(linfo, "source-name=%s\n", xfcl->fn->str)

         ;  if (do_rule)
            {  mcxTing* rule = mcxTingKAppend(NULL, "=", 78)
            ;  mcxTingPrintSplice(rule, 0, TING_INS_CENTER, " %s ", xfcl->fn->str)
            ;  fprintf(stdout, "%s\n", rule->str)
            ;  mcxTingFree(&rule)
         ;  }

            if (append_granul || append_perf)
            {  mcxIOrenew(xfcl, NULL, "a")
            ;  if (mcxIOopen(xfcl, RETURN_ON_FAIL))
               {  mcxErr(me, "cannot open file %s for appending", xfcl->fn->str)
               ;  append_granul = 0
               ;  append_perf = 0
            ;  }
               else
               mcxTell
               (me, "appending performance measures to file %s", xfcl->fn->str)
         ;  }

            clmGranularity(cl, vals)
         ;  clmGranularityPrint
            (stdout, do_header ? linfo->str : NULL, vals, do_header)
         ;  if (append_granul)
            clmGranularityPrint (xfcl->fp, linfo->str, vals, 1)

         ;  clmPerformance(mx, cl, vals)
         ;  clmPerformancePrint
            (stdout, do_header ? linfo->str : NULL, vals, do_header)
         ;  if (append_perf)
            clmPerformancePrint (xfcl->fp, linfo->str, vals, 1)
      ;  }

         mclxFree(&cl)
      ;  if (tmx)
         {  mclxFree(&mx)
         ;  mx = tmx
      ;  }

         mcxIOfree(&xfcl)
      ;  a++
   ;  }

      mclxFree(&mx)
   ;  return 0
;  }


