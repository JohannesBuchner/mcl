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
#include "util/minmax.h"



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
   ;  int a = 1, i, k=0, l
   ;  int n_print = 7, e_max = 16
   ;  double v_pow = 1.0, e_pow = 1.0, e_shift = 0.0, v_shift = 0.0
   ;  double inflation = 1.0
   ;  const char* caption = ""
   ;  mcxbool make_stochastic = FALSE

   ;  while(a < argc)
      {  if (!strcmp(argv[a], "-icl"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  xf_cl =  mcxIOnew(argv[a], "r")
      ;  }
         else if (!strcmp(argv[a], "-h"))
         {  help
         :  mcxUsage(stdout, me, usagelines)
         ;  mcxExit(STATUS_FAIL)
      ;  }

         else if (!strcmp(argv[a], "-imx"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  xf_mx = mcxIOnew(argv[a], "r")
      ;  }
         else if (!strcmp(argv[a], "-I"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  inflation = atof(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-e-max"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  e_max = atoi(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-caption"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  caption = argv[a]
      ;  }
         else if (!strcmp(argv[a], "-n-line"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  n_print = atoi(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-v-shift"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  v_shift = atof(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-e-shift"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  e_shift = atof(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-e-pow"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  e_pow = atof(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-v-pow"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  v_pow = atof(argv[a])
      ;  }
         else if (!strcmp(argv[a], "--st"))
         {  make_stochastic =  TRUE
      ;  }
         else if (!strcmp(argv[a], "-h"))
         {  goto help
      ;  }
         else if (0)
         {  arg_missing:
         ;  mcxErr
            (  me
            ,  "flag <%s> needs argument; see help (-h)"
            ,  argv[argc-1]
            )
         ;  mcxExit(1)
      ;  }
         else
         {  mcxErr
            (  me
            ,  "unrecognized flag <%s>; see help (-h)"
            ,  argv[a]
            )
         ;  mcxExit(1)
      ;  }
         a++
   ;  }

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


