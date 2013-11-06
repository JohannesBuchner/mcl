/* (c) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "impala/compose.h"
#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/pval.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/app.h"

#include "util/io.h"
#include "util/err.h"
#include "util/minmax.h"
#include "util/opt.h"
#include "util/types.h"

const char* me = "mcxarray";

const char* syntax = "Usage: mcxarray [options] <array data matrix>";

enum
{  MY_OPT_HELP
,  MY_OPT_TP
,  MY_OPT_PEARSON
,  MY_OPT_COSINE
,  MY_OPT_DOCENTER
,  MY_OPT_CENTERVAL
,  MY_OPT_CUTOFF
,  MY_OPT_VERSION
,  MY_OPT_LQ
,  MY_OPT_GQ
,  MY_OPT_O
,  MY_OPT_TEAR
,  MY_OPT_TEARTP
,  MY_OPT_PI
,  MY_OPT_DIGITS
,  MY_OPT_01
}  ;

mcxOptAnchor options[]
=
{
   {  "-h"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_HELP
   ,  NULL
   ,  "print this help"
   }
,  {  "--apropos"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_HELP
   ,  NULL
   ,  "print this help"
   }
,  {  "--version"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "print version information"
   }
,  {  "-t"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TP
   ,  NULL
   ,  "work with the transposed matrix"
   }
,  {  "--pearson"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_PEARSON
   ,  NULL
   ,  "work with Pearson correlation score (default)"
   }
,  {  "--cosine"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_COSINE
   ,  NULL
   ,  "work with the cosine"
   }
,  {  "--01"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_01
   ,  NULL
   ,  "remap to [0,1] interval"
   }
,  {  "--ctr"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DOCENTER
   ,  NULL
   ,  "add center with default value (for graph-type input)"
   }
,  {  "-ctr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CENTERVAL
   ,  NULL
   ,  "add center value (for graph-type input)"
   }
,  {  "-cutoff"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CUTOFF
   ,  "<num>"
   ,  "remove inproduct (output) values smaller than num"
   }
,  {  "-co"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CUTOFF
   ,  "<num>"
   ,  "alias for -cutof"
   }
,  {  "-lq"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_LQ
   ,  NULL
   ,  "ignore data (input) values larger than num"
   }
,  {  "-gq"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_GQ
   ,  NULL
   ,  "ignore data (input) values smaller than num"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_O
   ,  "<fname>"
   ,  "write to file fname"
   }
,  {  "-pi"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_PI
   ,  "<num>"
   ,  "inflate the result"
   }
,  {  "-tear"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TEAR
   ,  "<num>"
   ,  "inflate the input columns"
   }
,  {  "-teartp"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TEARTP
   ,  "<num>"
   ,  "inflate the transposed columns"
   }
,  {  "-digits"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DIGITS
   ,  "<int>"
   ,  "precision to use in interchange format"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  int c, d
   ;  int digits = MCLXIO_VALUE_GETENV
   ;  double cutoff = -1.0, tear = 0.0, teartp = 0.0, pi = 0.0
   ;  double lq = DBL_MAX, gq = -DBL_MAX, ctr = 0.0
   ;  mclx* tbl, *res
   ;  mcxIO* xfin, *xfout
   ;  mclv* ssqs, *sums, *scratch
   ;  mcxbool transpose = FALSE, to01 = FALSE
   ;  const char* out = "out.array"
   ;  mcxbool mode = 'p'
   ;  int n_mod
   ;  int n_arg_read = 0
   ;  mcxstatus parseStatus = STATUS_OK

   ;  mcxOption* opts, *opt
   
   ;  opts = mcxOptExhaust
             (options, (char**) argv, argc, 1, &n_arg_read, &parseStatus)

   ;  if (!opts)
      exit(0)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  mcxOptApropos(stdout, me, syntax, 0, 0, options)

         ;  return 0
         ;

            case MY_OPT_TP
         :  transpose = TRUE
         ;  break
         ;

            case MY_OPT_PEARSON
         :  mode = 'p'
         ;  break
         ;

            case MY_OPT_COSINE
         :  mode = 'c'
         ;  break
         ;

            case MY_OPT_01
         :  to01 = TRUE
         ;  break
         ;

            case MY_OPT_DOCENTER
         :  ctr = 1.0
         ;  break
         ;

            case MY_OPT_CENTERVAL
         :  ctr = atof(opt->val)
         ;  break
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  return 0
         ;

            case MY_OPT_CUTOFF
         :  cutoff = atof(opt->val)
         ;  break
         ;

            case MY_OPT_GQ
         :  gq = atof(opt->val)
         ;  break
         ;

            case MY_OPT_LQ
         :  lq = atof(opt->val)
         ;  break
         ;

            case MY_OPT_O
         :  out = opt->val
         ;  break
         ;

            case MY_OPT_PI
         :  pi = atof(opt->val)
         ;  break
         ;

            case MY_OPT_TEAR
         :  tear = atof(opt->val)
         ;  break
         ;

            case MY_OPT_TEARTP
         :  teartp = atof(opt->val)
         ;  break
         ;

            case MY_OPT_DIGITS
         :  digits = strtol(opt->val, NULL, 10)
         ;  break
      ;  }
      }

      if (n_arg_read+1 != argc-1)
      mcxDie(1, me, "expecting single trailing <fname> argument")

   ;  mcxOptFree(&opts)

   ;  xfin = mcxIOnew(argv[argc-1], "r")
   ;  xfout = mcxIOnew(out, "w")
   ;  mcxIOopen(xfin, EXIT_ON_FAIL)
   ;  mcxIOopen(xfout, EXIT_ON_FAIL)
   ;  tbl = mclxRead(xfin, EXIT_ON_FAIL)

   ;  if (lq < DBL_MAX)
      {  double mass = mclxMass(tbl)
      ;  double kept = mclxSelectValues(tbl, NULL, &lq, MCLX_EQT_LQ)
      ;  fprintf(stderr, "orig %.2f kept %.2f\n", mass, kept)
   ;  }

      if (gq > -DBL_MAX)
      {  double mass = mclxMass(tbl)
      ;  double kept = mclxSelectValues(tbl, &gq, NULL, MCLX_EQT_GQ)
      ;  fprintf(stderr, "orig %.2f kept %.2f\n", mass, kept)
   ;  }

      if (ctr && MCLD_EQUAL(tbl->dom_cols, tbl->dom_rows))
      mcxDie(1, me, "-ctr option disabled (mclxCenter needs inspection)")

   ;  if (tear)
      mclxInflate(tbl, tear)

   ;  if (transpose)
      {  mclx* tblt = mclxTranspose(tbl)
      ;  mclxFree(&tbl)
      ;  tbl = tblt
      ;  if (teartp)
         mclxInflate(tbl, teartp)
   ;  }

      ssqs = mclvCopy(NULL, tbl->dom_cols)
   ;  sums = mclvCopy(NULL, tbl->dom_cols)
   ;  scratch = mclvCopy(NULL, tbl->dom_cols)

   ;  for (c=0;c<N_COLS(tbl);c++)
      {  double sumsq = mclvPowSum(tbl->cols+c, 2.0)
      ;  double sum = mclvSum(tbl->cols+c)
      ;  ssqs->ivps[c].val = sumsq
      ;  sums->ivps[c].val = sum
   ;  }

      res   =
      mclxAllocZero
      (  mclvCopy(NULL, tbl->dom_cols)
      ,  mclvCopy(NULL, tbl->dom_cols)
      )

   ;  n_mod =  MAX(1+(N_COLS(tbl)-1)/40, 1)

   ;  {  double N  = MAX(N_ROWS(tbl), 1)

      ;  for (c=0;c<N_COLS(tbl);c++)
         {  mclvZeroValues(scratch)
         ;  for (d=c;d<N_COLS(tbl);d++)
            {  double ip = mclvIn(tbl->cols+c, tbl->cols+d)
            ;  double score = 0.0
            ;  if (mode == 'c')
               {  double nom = sqrt(ssqs->ivps[c].val  * ssqs->ivps[d].val)
               ;  score = nom ? ip / nom : 0.0
            ;  }
               else if (mode == 'p')
               {  double s1 = sums->ivps[c].val
               ;  double sq1= ssqs->ivps[c].val
               ;  double s2 = sums->ivps[d].val
               ;  double sq2= ssqs->ivps[d].val
               ;  double nom= sqrt((sq1 - s1*s1/N) * (sq2 - s2*s2/N))

               ;  double num= ip - s1*s2/N
               ;  double f1 = sq1 - s1*s1/N
               ;  double f2 = sq2 - s2*s2/N
               ;  score = nom ? (num / nom) : 0.0
;if (0) fprintf(stderr, "--%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", s1, sq1, s2, sq2, f1, f2, num, nom)
            ;  }
               if (score >= cutoff)
               scratch->ivps[d].val = score
         ;  }
            mclvAdd(scratch, res->cols+c, res->cols+c)
         ;  if ((c+1) % n_mod == 0)
               fputc('.', stderr)
            ,  fflush(NULL)
      ;  }
      }

      mclxAddTranspose(res, 0.5)

   ;  if (to01)
      {  double half = 0.5
      ;  mclx* halves
         =  mclxCartesian
            (  mclvCopy(NULL, res->dom_cols)
            ,  mclvCopy(NULL, res->dom_rows)
            ,  0.5
            )
      ;  mclxUnary(res, fltxMul, &half)
      ;  mclxMerge(res, halves, fltAdd)
      ;  mclxFree(&halves)
   ;  }

      if (pi)
      mclxInflate(res, pi)

   ;  mclxWrite(res, xfout, digits, EXIT_ON_FAIL)
   ;  return 0
;  }

