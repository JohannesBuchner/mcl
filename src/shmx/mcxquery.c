/*   (C) Copyright 2006, 2007, 2008, 2009 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
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
#include <signal.h>
#include <time.h>

#include "mcx.h"

#include "util/types.h"
#include "util/ding.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/array.h"
#include "util/compile.h"

#include "impala/matrix.h"
#include "impala/io.h"
#include "clew/clm.h"


int valcmp
(  const void*             i1
,  const void*             i2
)
   {  return *((pval*)i1) > *((pval*)i2) ? 1 : *((pval*)i1) < *((pval*)i2) ? -1 : 0
;  }


double pearson
(  const mclv* a
,  const mclv* b
,  dim n
)
   {  double suma = mclvSum(a)
   ;  double sumb = mclvSum(b)
   ;  double sumasq = mclvPowSum(a, 2.0)
   ;  double sumbsq = mclvPowSum(b, 2.0)

   ;  double nom = sqrt( (n*sumasq - suma*suma) * (n*sumbsq - sumb*sumb) )
   ;  double num = n * mclvIn(a, b) - suma * sumb
   ;  return nom ? num / nom : 0.0
;  }


enum
{  MY_OPT_IMX = MCX_DISP_UNUSED
,  MY_OPT_DIMENSION
,  MY_OPT_VARY_CORRELATION
,  MY_OPT_VARY_THRESHOLD
,  MY_OPT_DIVIDE
,  MY_OPT_WEIGHT_SCALE
,  MY_OPT_FOUT
}  ;


mcxOptAnchor qOptions[] =
{  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "specify input matrix/graph"
   }
,  {  "--dim"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DIMENSION
   ,  NULL
   ,  "get matrix dimensions"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_FOUT
   ,  "<fname>"
   ,  "output file name"
   }
,  {  "--vary-correlation"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_VARY_CORRELATION
   ,  NULL
   ,  "vary correlation threshold, output graph statistics"
   }
,  {  "-vary-threshold"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_VARY_THRESHOLD
   ,  "<num a>,<num z>,<num s>,<num n>"
   ,  "vary threshold from a/n to z/n using steps s/n"
   }
,  {  "-div"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DIVIDE
   ,  "<num>"
   ,  "divide in sets with property <= num and > num"
   }
,  {  "-report-scale"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_WEIGHT_SCALE
   ,  "<num>"
   ,  "edge weight multiplier for summary statistics (to avoid fractions)"
   }
,  {  NULL ,  0 ,  0,  NULL, NULL}
}  ;


static  mcxbool get_dim    =     -1;
static  mcxbool get_stat   =     -1;
static  mcxbool get_stat_list =  -1;
static  mcxbool have_correlation = -1;
static  mcxIO*  xfout      =     (void*) -1;
static  mcxIO* xfmx_g      =   (void*) -1;

static  unsigned int vary_a=  -1;
static  unsigned int vary_z=  -1;
static  unsigned int vary_s=  -1;
static  unsigned int vary_n=  -1;
static  unsigned int divide_g=  -1;
static  double weight_scale = -1;


static mcxstatus qInit
(  void
)
   {  get_dim  =  FALSE
   ;  get_stat =  FALSE
   ;  get_stat_list =  FALSE
   ;  xfout    =  mcxIOnew("-", "w")
   ;  xfmx_g   =  mcxIOnew("-", "r")
   ;  vary_z   =  0
   ;  divide_g =  3
   ;  vary_a   =  0
   ;  vary_s   =  0
   ;  vary_n   =  0
   ;  weight_scale = 0.0
   ;  have_correlation = FALSE
   ;  return STATUS_OK
;  }



static mcxstatus qArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  mcxIOnewName(xfmx_g, val)
      ;  break
      ;

         case MY_OPT_DIMENSION
      :  get_dim = TRUE
      ;  break
      ;

         case MY_OPT_FOUT
      :  mcxIOnewName(xfout, val)
      ;  break
      ;

         case MY_OPT_WEIGHT_SCALE
      :  weight_scale = atof(val)
      ;  break
      ;

         case MY_OPT_DIVIDE
      :  divide_g = atoi(val)
      ;  break
      ;

         case MY_OPT_VARY_CORRELATION
      :  vary_a = 4
      ;  vary_z = 20
      ;  vary_s = 1
      ;  vary_n = 20
      ;  weight_scale = 100
      ;  have_correlation = TRUE
      ;  break
      ;

         case MY_OPT_VARY_THRESHOLD
      :  if (4 != sscanf(val, "%u,%u,%u,%u", &vary_a, &vary_z, &vary_s, &vary_n))
         mcxDie(1, "mcx query", "failed to parse argument as integers start,end,step,norm")
      ;  if (100 * vary_s < vary_z - vary_a)
         mcxDie(1, "mcx query", "argument leads to more than one hundred steps")
      ;  if (!vary_n)
         mcxDie(1, "mcx query", "need nonzero scaling factor (last component)")
      ;  break
      ;

         default
      :  mcxExit(1) 
      ;
   ;  }
      return STATUS_OK
;  }


struct level
{  double   threshold                   /* edges below this cut out    */

;  double   degree_cor                  /* power-law for node degrees? */

;  double   sim_median
;  double   sim_mean
;  double   sim_iqr

;  long     nb_median
;  double   nb_mean
;  double   nb_sum
;  long     nb_iqr

;  double   bigsize
;  double   cc_exp
;  long     n_single
;  long     n_edge
;  long     n_lq
;
}  ;

struct level levels[100];

static double pval_get_double
(  const void* v
)
   {  return *((pval*) v)
;  }

static double ivp_get_double
(  const void* v
)
   {  return ((mclp*) v)->val
;  }


static void vary_threshold
(  mcxIO* xf
,  dim vary_a
,  dim vary_z
,  dim vary_s
,  dim vary_n
)
   {  dim cor_i = 0, j, step
   ;  mcxIO* xftest = mcxIOnew("-", "w")
   ;  mcxTing* fname = mcxTingEmpty(NULL, 20)
   ;  FILE* fp = xfout->fp

   ;  mclx* mx
   ;  unsigned long noe
   ;  pval*  allvals
   ;  dim  n_allvals = 0

   ;  mx = mclxReadx(xf, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH)
   ;  mclxAdjustLoops(mx, mclxLoopCBremove, NULL)
   ;  noe = mclxNrofEntries(mx)
   ;  allvals = mcxAlloc(noe * sizeof allvals[0], EXIT_ON_FAIL)

   ;  if (!have_correlation && !weight_scale)
      weight_scale = vary_n

   ;  for (j=0;j<N_COLS(mx);j++)
      {  mclv* vec = mx->cols+j
      ;  dim k
      ;  if (n_allvals + vec->n_ivps > noe)
         mcxDie(1, "mcx query", "not enough memory")
      ;  for (k=0;k<vec->n_ivps;k++)
         allvals[n_allvals+k] = vec->ivps[k].val
      ;  n_allvals += vec->n_ivps
   ;  }

      qsort(allvals, n_allvals, sizeof(pval), valcmp)

   ;  for (step = vary_a; step < vary_z; step += vary_s)
      {  double cutoff = step * 1.0 / vary_n
      ;  mclv* nnodes = mclvCanonical(NULL, N_COLS(mx), 0.0)
      ;  mclv* degree = mclvCanonical(NULL, N_COLS(mx), 0.0)
      ;  dim i, n_sample = 0, i_new = 0
      ;  double cor, y_prev, iqr = 0.0, sum_vals = 0.0
      ;  mclx* cc
      ;  mclv* sz, *ccsz
      
      ;  mclxSelectValues(mx, &cutoff, NULL, MCLX_EQT_GQ)

      ;  if (0)
         {  mcxTingPrint(fname, "tst.%03d", (int) (cutoff + 0.5))
         ;  mcxIOrenew(xftest, fname->str, NULL)
         ;  mclxWrite(mx, xftest, MCLXIO_VALUE_GETENV, EXIT_ON_FAIL)
         ;  mcxIOclose(xftest)
      ;  }

         sz = mclxColSizes(mx, MCL_VECTOR_COMPLETE)
      ;  mclvSortDescVal(sz)

      ;  cc = clmComponents(mx, NULL)
      ;  ccsz = mclxColSizes(cc, MCL_VECTOR_COMPLETE)

      ;  fprintf(stderr, "analysing cutoff %8.2f\n", cutoff)

      ;  for (i=0;i<n_allvals;i++)
         {  if (allvals[i] >= cutoff)
               allvals[i_new++] = allvals[i]
            ,  sum_vals += allvals[i]
      ;  }

         n_allvals = i_new

      ;  levels[cor_i].sim_median=  mcxMedian(allvals, n_allvals, sizeof allvals[0], pval_get_double, &iqr)
      ;  levels[cor_i].sim_iqr   =  iqr
      ;  levels[cor_i].sim_mean  =  n_allvals ? sum_vals / n_allvals : 0.0

      ;  levels[cor_i].nb_median =  mcxMedian(sz->ivps, sz->n_ivps, sizeof sz->ivps[0], ivp_get_double, &iqr)
      ;  levels[cor_i].nb_iqr    =  iqr
      ;  levels[cor_i].nb_mean   =  mclvSum(sz) / N_COLS(mx)
      ;  levels[cor_i].cc_exp    =  mclvPowSum(ccsz, 2.0) / N_COLS(mx)
      ;  levels[cor_i].nb_sum    =  mclxNrofEntries(mx)

      ;  levels[cor_i].threshold =  cutoff
      ;  levels[cor_i].bigsize   =  cc->cols[0].n_ivps
      ;  levels[cor_i].n_single  =  0
      ;  levels[cor_i].n_edge    =  n_allvals
      ;  levels[cor_i].n_lq      =  0

      ;  for (i=0;i<N_COLS(cc);i++)
         {  dim n = cc->cols[N_COLS(cc)-1-i].n_ivps
         ;  if (n == 1)
            levels[cor_i].n_single++
         ;  if (n <= divide_g)
            levels[cor_i].n_lq += n
         ;  else
            break
      ;  }

         if (levels[cor_i].bigsize <= divide_g)
         levels[cor_i].bigsize = 0

;if(0)
{  mcxTingPrint(fname, "uuu.%03d", (int) (1000 * (cutoff + 0.0001)))
;  mcxIOrenew(xftest, fname->str, NULL)
;  mcxIOopen(xftest, EXIT_ON_FAIL)
;  fprintf(xftest->fp, "degree\tnnodes\tlogn\tlogd\n")
;}

      ;  y_prev = sz->ivps[0].val

      ;  for (i=1;i<sz->n_ivps;i++)
         {  double y = sz->ivps[i].val
         ;  if (y > y_prev - 0.5)
            continue                                              /* same as node degree seen last */
         ;  nnodes->ivps[n_sample].val = log( (i*1.0) / (1.0*N_COLS(mx)))   /* x = #nodes >= k        */
         ;  degree->ivps[n_sample].val = log(y_prev ? y_prev : 1)            /* y = k = degree of node */
         ;  n_sample++
;if(0)fprintf(xftest->fp, "%.0f\t%d\t%.3f\t%.3f\n", (double) y_prev, (int) i, (double) nnodes->ivps[n_sample-1].val, (double) degree->ivps[n_sample-1].val)
         ;  y_prev = y
      ;  }
         nnodes->ivps[n_sample].val = 0
      ;  nnodes->ivps[n_sample++].val = log(y_prev ? y_prev : 1)
;if(0){fprintf(xftest->fp, "%.0f\t%d\t%.3f\t%.3f\n", (double) sz->ivps[sz->n_ivps-1].val, (int) N_COLS(mx), (double) nnodes->ivps[n_sample-1].val, (double) degree->ivps[n_sample-1].val)
;mcxIOclose(xftest)
;}

      ;  mclvResize(nnodes, n_sample)
      ;  mclvResize(degree, n_sample)
      ;  cor = pearson(nnodes, degree, n_sample)

      ;  levels[cor_i].degree_cor =  cor * cor

;if(0)fprintf(stdout, "cor at cutoff %.2f %.3f\n\n", cutoff, levels[cor_i-1].degree_cor)
      ;  mclvFree(&nnodes)
      ;  mclvFree(&degree)
      ;  mclvFree(&sz)
      ;  mclvFree(&ccsz)
      ;  mclxFree(&cc)

      ;  cor_i++
   ;  }
fprintf(fp, "-------------------------------------------------------------------------------\n")
;fprintf(fp, "The graph below plots the R^2 squared value for the fit of a log-log plot of\n")
;fprintf(fp, "<node degree k> versus <#nodes with degree >= k>, for the network resulting\n")
;fprintf(fp, "from applying a particular %s cutoff.\n", have_correlation ? "correlation" : "similarity")
;fprintf(fp, "-------------------------------------------------------------------------------\n")
   ;  for (j=0;j<cor_i;j++)
      {  dim jj
      ;  for (jj=30;jj<=100;jj++)
         {  char c = ' '
         ;  if (jj * 0.01 < levels[j].degree_cor && (jj+1.0) * 0.01 > levels[j].degree_cor)
            c = 'X'
         ;  else if (jj % 5 == 0)
            c = '|'
         ;  fputc(c, fp)
      ;  }
         if (have_correlation)
         fprintf(fp, "%8.2f\n", (double) levels[j].threshold)
      ;  else
         fprintf(fp, "%8.0f\n", (double) levels[j].threshold * weight_scale)
   ;  }
 fprintf(fp, "|----+----|----+----|----+----|----+----|----+----|----+----|----+----|--------\n")
;fprintf(fp, "| R^2   0.4       0.5       0.6       0.7       0.8       0.9    |  1.0    -o)\n")
;fprintf(fp, "+----+----+----+----+----+---------+----+----+----+----+----+----+----+    /\\\\\n")
;fprintf(fp, "| 2 4 6 8   2 4 6 8 | 2 4 6 8 | 2 4 6 8 | 2 4 6 8 | 2 4 6 8 | 2 4 6 8 |   _\\_/\n")
;fprintf(fp, "+----+----|----+----|----+----|----+----|----+----|----+----|----+----+--------\n")
;fprintf(fp, " L       Percentage of nodes in the largest component\n")
;fprintf(fp, " D       Percentage of nodes in components of size at most %d [-div option]\n", (int) divide_g)
;fprintf(fp, " R       Percentage of nodes not in L or D: 100 - L -D\n")
;fprintf(fp, " S       Percentage of nodes that are singletons\n")
;fprintf(fp, " cce     Expected size of component, nodewise [ sum(sz^2) / sum^2(sz) ]\n")
;fprintf(fp, "*EW      Edge weight traits (mean, median and IQR, all scaled!)\n")
;fprintf(fp, "            Scaling is used to avoid printing of fractional parts throughout.\n")
;fprintf(fp, "            The scaling factor is %.2f [-report-scale option]\n", weight_scale)
;fprintf(fp, " ND      Node degree traits [mean, median and IQR]\n")
;if (have_correlation)
 fprintf(fp, "Cutoff   The threshold used.\n")
;else
 fprintf(fp, "*Cutoff   The threshold with scale factor %.2f and fractional parts removed\n", weight_scale)
;fprintf(fp, "Total number of nodes: %ld\n", (long) N_COLS(mx))
;fprintf(fp, "-------------------------------------------------------------------------------\n")
;fprintf(fp, "  L   D   R   S     cce *EWmean  *EWmed *EWiqr NDmean  NDmed  NDiqr     %sCutoff \n", have_correlation ? " " : "*")
;fprintf(fp, "-------------------------------------------------------------------------------\n")
;     for (j=0;j<cor_i;j++)
      {  fprintf
         (  fp
         ,  "%3d %3d %3d %3d %7d %7.0f %7.0f %6.0f %6.1f %6.0f %6.0f    "
         ,  (int) (0.5 + (100.0 * levels[j].bigsize) / N_COLS(mx))
         ,  (int) (0.5 + (100.0 * levels[j].n_lq) / N_COLS(mx))
         ,  (int) (0.5 + (100.0 * (N_COLS(mx) - levels[j].bigsize - levels[j].n_lq)) / N_COLS(mx))
         ,  (int) (0.5 + (100.0 * levels[j].n_single) / N_COLS(mx))
         ,  (int) (0.5 + levels[j].cc_exp)
         ,  (double) levels[j].sim_mean   * weight_scale
         ,  (double) levels[j].sim_median * weight_scale
         ,  (double) levels[j].sim_iqr    * weight_scale
         ,  (double) levels[j].nb_mean
         ,  (double) levels[j].nb_median + 0.5
         ,  (double) levels[j].nb_iqr + 0.5
         )
      ;  if (have_correlation)
         fprintf(fp, "%8.2f\n", (double) levels[j].threshold)
      ;  else
         fprintf(fp, "%8.0f\n", (double) levels[j].threshold  * weight_scale)

         /*
         ,  (long) levels[j].n_edge          * this (n_allvals) bigger than *
         ,  (double) levels[j].nb_sum        * that (mclxNrofEntries) *
         */
   ;  }
   }



/*
  /___  
(o___,)|

 -o)\n")
 /\\\\\n")
_\\_/\n")
*/




static mcxstatus qMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)
   {  if (get_dim + get_stat + get_stat_list + vary_s == 0)
      get_stat = TRUE

   ;  mcxIOopen(xfout, EXIT_ON_FAIL)

   ;  if (vary_s)
      {  vary_threshold
         (  xfmx_g
         ,  vary_a
         ,  vary_z
         ,  vary_s
         ,  vary_n
         )
      ;  exit(0)
   ;  }

      else if (get_dim)
      {  const char* fmt
      ;  int format
      ;  long n_cols, n_rows

      ;  if (mclxReadDimensions(xfmx_g, &n_cols, &n_rows))
         mcxDie(1, "mcx query", "reading %s failed", xfmx_g->fn->str)

      ;  format = mclxIOformat(xfmx_g)
      ;  fmt = format == 'b' ? "binary" : format == 'a' ? "interchange" : "?"
      ;  fprintf
         (  xfout->fp
         ,  "%s format,  row x col dimensions are %ld x %ld\n"
         ,  fmt
         ,  n_rows
         ,  n_cols
         )
   ;  }
      return 0
;  }


mcxDispHook* mcxDispHookquery
(  void
)
   {  static mcxDispHook qEntry
   =  {  "query"
      ,  "query [options]"
      ,  qOptions
      ,  sizeof(qOptions)/sizeof(mcxOptAnchor) - 1

      ,  qArgHandle
      ,  qInit
      ,  qMain

      ,  0
      ,  0
      ,  MCX_DISP_MANUAL
      }
   ;  return &qEntry
;  }


