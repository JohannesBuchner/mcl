/*   (C) Copyright 2006, 2007, 2008, 2009, 2010 Stijn van Dongen
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
#include "util/rand.h"
#include "util/compile.h"
#include "util/minmax.h"

#include "impala/matrix.h"
#include "impala/compose.h"
#include "impala/io.h"

#include "clew/clm.h"
#include "gryphon/path.h"
#include "mcl/transform.h"


int valcmp
(  const void*             i1
,  const void*             i2
)
   {  return *((pval*)i1) < *((pval*)i2) ? 1 : *((pval*)i1) > *((pval*)i2) ? -1 : 0
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
,  MY_OPT_ICL
,  MY_OPT_TAB
,  MY_OPT_DIMENSION
,  MY_OPT_NODE
,  MY_OPT_VARY_CORRELATION
,  MY_OPT_VARY_THRESHOLD
,  MY_OPT_VARY_KNN
,  MY_OPT_DIVIDE
,  MY_OPT_WEIGHT_SCALE
,  MY_OPT_MYTH
,  MY_OPT_TESTCYCLE
,  MY_OPT_TESTCYCLE_N
,  MY_OPT_TRANSFORM
,  MY_OPT_THREAD
,  MY_OPT_CLCF
,  MY_OPT_KNNREDUCE
,  MY_OPT_FOUT
}  ;


mcxOptAnchor qOptions[] =
{  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "specify input matrix/graph"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "specify tab file to be used with matrix input"
   }
,  {  "-icl"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ICL
   ,  "<fname>"
   ,  "specify input clustering"
   }
,  {  "--node-attr"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_NODE
   ,  NULL
   ,  "output for each node its weight statistics and degree"
   }
,  {  "-t"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_THREAD
   ,  "<num>"
   ,  "number of threads to use (with -knn)"
   }
,  {  "--knn-reduce"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_KNNREDUCE
   ,  NULL
   ,  "do not rebase to input graph, use last reduction"
   }
,  {  "--dim"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DIMENSION
   ,  NULL
   ,  "get matrix dimensions"
   }
,  {  "--test-cycle"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TESTCYCLE
   ,  NULL
   ,  "test whether graph has cycles"
   }
,  {  "-test-cycle"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TESTCYCLE_N
   ,  "<num>"
   ,  "output at most <num> nodes in cycles; 0 for all"
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
   ,  "a,z,s,n"
   ,  "vary threshold from a/n to z/n in steps s/n"
   }
,  {  "-vary-knn"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_VARY_KNN
   ,  "a,z,s,n"
   ,  "vary knn from z to a in steps s, scale edge weights by n"
   }
,  {  "-div"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DIVIDE
   ,  "<num>"
   ,  "divide in sets with property <= num and > num"
   }
,  {  "-tf"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TRANSFORM
   ,  "<func(arg)[, func(arg)]*>"
   ,  "apply unary transformations to matrix values"
   }
,  {  "--clcf"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_CLCF
   ,  NULL
   ,  "compute clustering coefficient"
   }
,  {  "--scale-free?"
   ,  MCX_OPT_HIDDEN
   ,  MY_OPT_MYTH
   ,  NULL
   ,  "very simple scale-freeness test, correlation of log/log values"
   }
,  {  "-report-scale"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_WEIGHT_SCALE
   ,  "<num>"
   ,  "edge weight multiplier for summary statistics (to avoid fractions)"
   }
,  {  NULL ,  0 ,  0,  NULL, NULL}
}  ;


static  const char* me     =  "mcx query";
static  mcxIO*  xfout_g    =   (void*) -1;
static  mcxIO* xfmx_g      =   (void*) -1;
static  mcxIO* xfcl_g      =   (void*) -1;
static  mcxIO* xftab_g     =   (void*) -1;

static  unsigned int vary_a=  -1;
static  unsigned int vary_z=  -1;
static  unsigned int vary_s=  -1;
static  unsigned int vary_n=  -1;
static  unsigned int divide_g=  -1;
static  double weight_scale = -1;
static  unsigned mode_vary = -1;
static  unsigned mode_get  =     -1;
static  unsigned n_limit = 0;
static  mcxbool weefreemen = -1;
static  mcxbool knnexact_g = -1;
static  mcxbool doclcf_g = -1;
static  mcxbool user_imx = -1;
static  mcxbool transpose  = -1;
static  dim n_thread_g = -1;
static  mclgTF* transform  =   (void*) -1;
static  mcxTing* transform_spec = (void*) -1;


static struct level* levels;

static mclv* matrix_vector
(  const mclx* mx
,  const mclv* vec
)
   {  mclv* res = mclvClone(mx->dom_rows)
   ;  dim i, j
   ;  mclvMakeConstant(res, 0.0)
   ;  for (i=0;i<vec->n_ivps;i++)
      {  mclv* c = mx->cols + vec->ivps[i].idx
      ;  for (j=0;j<c->n_ivps;j++)
         res->ivps[c->ivps[j].idx].val += 1.0
   ;  }
      mclvUnary(res, fltxCopy, NULL)
   ;  return res
;  }


static mclv* run_through
(  const mclx* mx
)
   {  mclv* starts = mclvClone(mx->dom_cols)
   ;  dim n_starts_previous = starts->n_ivps + 1
   ;  dim n_steps = 0

   ;  while (n_starts_previous > starts->n_ivps)
      {  mclv* new_starts = matrix_vector(mx, starts)
      ;  mclvMakeCharacteristic(new_starts)
      ;  n_starts_previous = starts->n_ivps
      ;  mclvFree(&starts)
      ;  starts = new_starts
      ;  n_steps++
      ;  fputc('.', stderr)
   ;  }
      if (n_steps)
      fputc('\n', stderr)
   ;  return starts
;  }


static int test_cycle
(  const mclx* mx
,  dim n_limit
)
   {  mclv* starts = run_through(mx), *starts2
   ;  if (starts->n_ivps)
      {  dim i
      ;  if (n_limit)
         {  mclx* mxt = mclxTranspose(mx)
         ;  starts2 = run_through(mxt)
         ;  mclxFree(&mxt)
         ;  mclvBinary(starts, starts2, starts, fltMultiply)

         ;  mcxErr
            (me, "cycles detected (%u nodes)", (unsigned) starts->n_ivps)

         ;  if (starts->n_ivps)
            {  fprintf(stdout, "%lu", (ulong) starts->ivps[0].idx)
            ;  for (i=1; i<MCX_MIN(starts->n_ivps, n_limit); i++)
               fprintf(stdout, " %lu", (ulong) starts->ivps[i].idx)
            ;  fputc('\n', stdout)
         ;  }
            else
            mcxErr(me, "strange, no nodes selected")
      ;  }
         else
         mcxErr(me, "cycles detected")
      ;  return 1
   ;  }

      mcxTell(me, "no cycles detected")
   ;  return 0
;  }


static mcxstatus qInit
(  void
)
   {  mode_get =  0
   ;  n_limit  =  0
   ;  xfout_g  =  mcxIOnew("-", "w")
   ;  xftab_g  =  NULL
   ;  vary_z   =  0
   ;  divide_g =  3
   ;  vary_a   =  0
   ;  vary_s   =  0
   ;  vary_n   =  1
   ;  xfmx_g   =  mcxIOnew("-", "r")
   ;  xfcl_g   =  NULL
   ;  n_thread_g = 1
   ;  mode_vary = 0
   ;  transpose=  FALSE
   ;  knnexact_g = TRUE
   ;  doclcf_g = FALSE
   ;  weefreemen = FALSE
   ;  user_imx =  FALSE
   ;  weight_scale = 0.0
   ;  transform      =  NULL
   ;  transform_spec =  NULL
   ;  return STATUS_OK
;  }



static mcxstatus qArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  mcxIOnewName(xfmx_g, val)
      ;  user_imx = TRUE
      ;  break
      ;

         case MY_OPT_TAB
      :  xftab_g = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_ICL
      :  xfcl_g =  mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_TRANSFORM
      :  transform_spec = mcxTingNew(val)
      ;  break
      ;

         case MY_OPT_DIMENSION
      :  mode_get = 'd'
      ;  break
      ;

         case MY_OPT_TESTCYCLE
      :  mode_get = 'c'
      ;  break
      ;

         case MY_OPT_TESTCYCLE_N
      :  n_limit = atoi(val)
      ;  mode_get = 'c'
      ;  break
      ;

         case MY_OPT_CLCF
      :  doclcf_g = TRUE
      ;  break
      ;

         case MY_OPT_KNNREDUCE
      :  knnexact_g = FALSE
      ;  break
      ;

         case MY_OPT_THREAD
      :  n_thread_g = atoi(val)
      ;  break
      ;

         case MY_OPT_NODE
      :  mode_get = 'n'
      ;  break
      ;

         case MY_OPT_FOUT
      :  mcxIOnewName(xfout_g, val)
      ;  break
      ;

         case MY_OPT_WEIGHT_SCALE
      :  weight_scale = atof(val)
      ;  break
      ;

         case MY_OPT_MYTH
      :  weefreemen = TRUE
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
      ;  mode_vary = 'c'
      ;  break
      ;

         case MY_OPT_VARY_KNN
      :  if (4 != sscanf(val, "%u,%u,%u,%u", &vary_a, &vary_z, &vary_s, &vary_n))
         mcxDie(1, me, "failed to parse argument as integers start,end,step,norm")
      ;  mode_vary = 'k'
      ;  break
      ;

         case MY_OPT_VARY_THRESHOLD
      :  if (4 != sscanf(val, "%u,%u,%u,%u", &vary_a, &vary_z, &vary_s, &vary_n))
         mcxDie(1, me, "failed to parse argument as integers start,end,step,norm")
      ;  mode_vary = 't'
      ;  break
      ;

         default
      :  mcxExit(1) 
      ;
   ;  }

      if (mode_vary && 1000 * vary_s < vary_z - vary_a)
      mcxDie(1, me, "argument leads to more than one thousand steps")

   ;  if (!vary_n)
      mcxDie(1, me, "need nonzero scaling factor (last component)")

   ;  return STATUS_OK
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
;  double   clcf
;  long     n_single
;  long     n_edge
;  long     n_lq
;
}  ;


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


dim get_n_sort_allvals
(  const mclx* mx
,  pval* allvals
,  dim noe
,  double* sum_vals
)
   {  dim n_allvals = 0
   ;  double s = 0.0
   ;  dim j
   ;  for (j=0;j<N_COLS(mx);j++)
      {  mclv* vec = mx->cols+j
      ;  dim k
      ;  if (n_allvals + vec->n_ivps > noe)
         mcxDie(1, me, "panic/impossible: not enough memory")
      ;  for (k=0;k<vec->n_ivps;k++)
         allvals[n_allvals+k] = vec->ivps[k].val
      ;  n_allvals += vec->n_ivps
      ;  s += mclvSum(vec)
   ;  }
      qsort(allvals, n_allvals, sizeof(pval), valcmp)
   ;  sum_vals[0] = s
   ;  return n_allvals
;  }


static void vary_threshold
(  mcxIO* xf
,  FILE*  fp
,  dim vary_a
,  dim vary_z
,  dim vary_s
,  dim vary_n
,  unsigned mode
)
   {  dim cor_i = 0, j, step

   ;  mclx* mx
   ;  unsigned long noe
   ;  pval*  allvals
   ;  dim  n_allvals = 0
   ;  double sum_vals = 0.0

   ;  mx = mclxRead(xf, EXIT_ON_FAIL)
   ;  mcxIOclose(xf)

   ;  if (transform_spec)
      { if (!(transform = mclgTFparse(NULL, transform_spec)))
         mcxDie(1, me, "input -tf spec does not parse")
      ;  mclgTFexec(mx, transform)
   ;  }

      noe = mclxNrofEntries(mx)
   ;  allvals = mcxAlloc(noe * sizeof allvals[0], EXIT_ON_FAIL)

   ;  if (!weight_scale)
      {  if (mode == 'c')
         weight_scale = 1.0
      ;  else
         weight_scale = vary_n
   ;  }

      n_allvals = get_n_sort_allvals(mx, allvals, noe, &sum_vals)

   ;  if (mode == 'c')
      {  double smallest = n_allvals ? allvals[n_allvals-1] : -DBL_MAX
      ;  if (vary_a * 1.0 / vary_n < smallest)
         {  while (vary_a * 1.0 / vary_n < smallest)
            vary_a++
         ;  vary_a--
      ;  }
         mcxTell
         (  me
         ,  "smallest correlation is %.2f, using starting point %.2f"
         ,  smallest
         ,  vary_a * 1.0 / vary_n
         )
   ;  }

;fprintf(fp, "-------------------------------------------------------------------------------\n")
;fprintf(fp, " L       Percentage of nodes in the largest component\n")
;fprintf(fp, " D       Percentage of nodes in components of size at most %d [-div option]\n", (int) divide_g)
;fprintf(fp, " R       Percentage of nodes not in L or D: 100 - L -D\n")
;fprintf(fp, " S       Percentage of nodes that are singletons\n")
;fprintf(fp, " cce     Expected size of component, nodewise [ sum(sz^2) / sum^2(sz) ]\n")
;fprintf(fp, "*EW      Edge weight traits (mean, median and IQR, all scaled!)\n")
;fprintf(fp, "            Scaling is used to avoid printing of fractional parts throughout.\n")
;fprintf(fp, "            The scaling factor is %.2f [-report-scale option]\n", weight_scale)
;fprintf(fp, " ND      Node degree traits [mean, median and IQR]\n")
;fprintf(fp, " CCF     Clustering coefficient")
;if (!doclcf_g)
 fprintf(fp, " (not computed; use --clcf to include this)\n")
;else
 fputc('\n', fp)
;if (mode == 'c')
 fprintf(fp, "Cutoff   The threshold used.\n")
;else if (mode == 't')
 fprintf(fp, "*Cutoff  The threshold with scale factor %.2f and fractional parts removed\n", weight_scale)
;else if (mode == 'k')
 fprintf(fp, "k-NN     The knn parameter\n")
;fprintf(fp, "Total number of nodes: %lu\n", (ulong) N_COLS(mx))
;fprintf(fp, "-------------------------------------------------------------------------------\n")
;fprintf(fp, "  L   D   R   S     cce *EWmean  *EWmed *EWiqr NDmean  NDmed  NDiqr CCF %s%6s \n", mode == 'c' ? " " : "*", mode == 'k' ? "k-NN" : "Cutoff")
;fprintf(fp, "-------------------------------------------------------------------------------\n")

   ;  for (step = vary_a; step <= vary_z; step += vary_s)
      {  double cutoff = step * 1.0 / vary_n
      ;  mclv* nnodes = mclvCanonical(NULL, N_COLS(mx), 0.0)
      ;  mclv* degree = mclvCanonical(NULL, N_COLS(mx), 0.0)
      ;  dim i, n_sample = 0
      ;  double cor, y_prev, iqr = 0.0
      ;  mclx* cc = NULL, *res = NULL
      ;  mclv* sz, *ccsz = NULL
      ;  dim step2 = vary_z + vary_a - step

      ;  sum_vals = 0.0
      
      ;  if (mode == 't' || mode == 'c')
            mclxSelectValues(mx, &cutoff, NULL, MCLX_EQT_GQ)
         ,  res = mx
      ;  else if (mode == 'k')
         {  res = knnexact_g ? mclxCopy(mx) : mx
         ;  mclxKNNdispatch(res, step2, n_thread_g)
      ;  }

         sz = mclxColSizes(res, MCL_VECTOR_COMPLETE)
      ;  mclvSortDescVal(sz)

      ;  cc = clmComponents(res, NULL)     /* fixme: user has to specify -tf '#max()' if graph is directed */
      ;  if (cc)
         ccsz = mclxColSizes(cc, MCL_VECTOR_COMPLETE)

      ;  if (mode == 't' || mode == 'c')
         {  for
            (
            ;  n_allvals > 0 && allvals[n_allvals-1] < cutoff
            ;  n_allvals--
            )
         ;  sum_vals = 0.0
         ;  for (i=0;i<n_allvals;i++)
            sum_vals += allvals[i]
      ;  }
         else if (mode == 'k')
         {  n_allvals = get_n_sort_allvals(res, allvals, noe, &sum_vals)
      ;  }

         levels[cor_i].sim_median=  mcxMedian(allvals, n_allvals, sizeof allvals[0], pval_get_double, &iqr)
      ;  levels[cor_i].sim_iqr   =  iqr
      ;  levels[cor_i].sim_mean  =  n_allvals ? sum_vals / n_allvals : 0.0

      ;  levels[cor_i].nb_median =  mcxMedian(sz->ivps, sz->n_ivps, sizeof sz->ivps[0], ivp_get_double, &iqr)
      ;  levels[cor_i].nb_iqr    =  iqr
      ;  levels[cor_i].nb_mean   =  mclvSum(sz) / N_COLS(res)
      ;  levels[cor_i].cc_exp    =  cc ? mclvPowSum(ccsz, 2.0) / N_COLS(res) : 0
      ;  levels[cor_i].nb_sum    =  mclxNrofEntries(res)

      ;  if (doclcf_g)
         {  mclv* clcf = mclgCLCFdispatch(res, n_thread_g)
         ;  levels[cor_i].clcf      =  mclvSum(clcf) / N_COLS(mx)
         ;  mclvFree(&clcf)
      ;  }
         else
         levels[cor_i].clcf = 0.0

      ;  levels[cor_i].threshold =  mode_vary == 'k' ? step2 : cutoff
      ;  levels[cor_i].bigsize   =  cc ? cc->cols[0].n_ivps : 0
      ;  levels[cor_i].n_single  =  0
      ;  levels[cor_i].n_edge    =  n_allvals
      ;  levels[cor_i].n_lq      =  0

      ;  if (cc)
         for (i=0;i<N_COLS(cc);i++)
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

      ;  y_prev = sz->ivps[0].val

                  /* wiki says:
                     A scale-free network is a network whose degree distribution follows a power
                     law, at least asymptotically. That is, the fraction P(k) of nodes in the
                     network having k connections to other nodes goes for large values of k as P(k)
                     ~ k^−g where g is a constant whose value is typically in the range 2<g<3,
                     although occasionally it may lie outside these bounds.
                 */
      ;  for (i=1;i<sz->n_ivps;i++)
         {  double y = sz->ivps[i].val
         ;  if (y > y_prev - 0.5)
            continue                                              /* same as node degree seen last */
         ;  nnodes->ivps[n_sample].val = log( (i*1.0) / (1.0*N_COLS(res)))    /* x = #nodes >= k, as fraction   */
         ;  degree->ivps[n_sample].val = log(y_prev ? y_prev : 1)            /* y = k = degree of node         */
         ;  n_sample++
;if(0)fprintf(stderr, "k=%.0f\tn=%d\t%.3f\t%.3f\n", (double) y_prev, (int) i, (double) nnodes->ivps[n_sample-1].val, (double) degree->ivps[n_sample-1].val)
         ;  y_prev = y
      ;  }
         nnodes->ivps[n_sample].val = 0
      ;  nnodes->ivps[n_sample++].val = log(y_prev ? y_prev : 1)
;if(0){fprintf(stderr, "k=%.0f\tn=%d\t%.3f\t%.3f\n", (double) sz->ivps[sz->n_ivps-1].val, (int) N_COLS(res), (double) nnodes->ivps[n_sample-1].val, (double) degree->ivps[n_sample-1].val)
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

;if(1){
      ;  fprintf
         (  fp
         ,  "%3d %3d %3d %3d %7d "
            "%7.0f %7.0f %6.0f"
            "%6.1f %6.0f %6.0f"

         ,  0 ? 1 : (int) (0.5 + (100.0 * levels[cor_i].bigsize) / N_COLS(mx))
         ,  0 ? 1 : (int) (0.5 + (100.0 * levels[cor_i].n_lq) / N_COLS(mx))
         ,  0 ? 1 : (int) (0.5 + (100.0 * (N_COLS(mx) - levels[cor_i].bigsize - levels[cor_i].n_lq)) / N_COLS(mx))
         ,  0 ? 1 : (int) (0.5 + (100.0 * levels[cor_i].n_single) / N_COLS(mx))
         ,  0 ? 1 : (int) (0.5 + levels[cor_i].cc_exp)

         ,  0 ? 1.0 : (double) (levels[cor_i].sim_mean   * weight_scale)
         ,  0 ? 1.0 : (double) (levels[cor_i].sim_median * weight_scale)
         ,  0 ? 1.0 : (double) (levels[cor_i].sim_iqr    * weight_scale)

         ,  0 ? 1.0 : (double) (levels[cor_i].nb_mean                 )
         ,  0 ? 1.0 : (double) (levels[cor_i].nb_median + 0.5         )
         ,  0 ? 1.0 : (double) (levels[cor_i].nb_iqr + 0.5            )
         )

      ;  if (doclcf_g)
         fprintf(fp, " %3d", 0 ? 1 : (int) (0.5 + (100.0 * levels[cor_i].clcf)))
      ;  else
         fputs("   -", fp)

      ;  if (mode == 'c')
         fprintf(fp, "%8.2f\n", (double) levels[cor_i].threshold)
      ;  else if (mode == 't')
         fprintf(fp, "%8.0f\n", (double) levels[cor_i].threshold  * weight_scale)
      ;  else if (mode == 'k')
         fprintf(fp, "%8.0f\n", (double) levels[cor_i].threshold)
;}
      ;  cor_i++
      ;  if (res != mx)
         mclxFree(&res)
   ;  }

      if (weefreemen)
      {
fprintf(fp, "-------------------------------------------------------------------------------\n")
;fprintf(fp, "The graph below plots the R^2 squared value for the fit of a log-log plot of\n")
;fprintf(fp, "<node degree k> versus <#nodes with degree >= k>, for the network resulting\n")
;fprintf(fp, "from applying a particular %s cutoff.\n", mode == 'c' ? "correlation" : "similarity")
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
         if (mode == 'c')
         fprintf(fp, "%8.2f\n", (double) levels[j].threshold)
      ;  else
         fprintf(fp, "%8.0f\n", (double) levels[j].threshold * weight_scale)
   ;  }
 fprintf(fp, "|----+----|----+----|----+----|----+----|----+----|----+----|----+----|--------\n")
;fprintf(fp, "| R^2   0.4       0.5       0.6       0.7       0.8       0.9    |  1.0    -o)\n")
;fprintf(fp, "+----+----+----+----+----+---------+----+----+----+----+----+----+----+    /\\\\\n")
;fprintf(fp, "| 2 4 6 8   2 4 6 8 | 2 4 6 8 | 2 4 6 8 | 2 4 6 8 | 2 4 6 8 | 2 4 6 8 |   _\\_/\n")
;fprintf(fp, "+----+----|----+----|----+----|----+----|----+----|----+----|----+----+--------\n")
;     }
      else
      fprintf(fp, "-------------------------------------------------------------------------------\n")

   ;  mclxFree(&mx)
   ;  mcxFree(allvals)
;  }



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
   {  mclx* cl = NULL
   ;  mclv* clannot = NULL
   ;  mclTab* tab = NULL

   ;  srandom(mcxSeed(135313531))

   ;  mcxIOopen(xfout_g, EXIT_ON_FAIL)
   ;  levels = mcxAlloc(1001 * sizeof levels[0], EXIT_ON_FAIL)

   ;  if (!mode_vary && !mode_get)
      mode_get = 'n'

   ;  if (xftab_g)
      tab = mclTabRead(xftab_g, NULL, EXIT_ON_FAIL)

   ;  if (xfcl_g)
      {  dim i
      ;  cl = mclxRead(xfcl_g, EXIT_ON_FAIL)
      ;  clannot = cl->dom_rows
      ;  for (i=0;i<N_COLS(cl);i++)
         {  mclv* cls = cl->cols+i
         ;  dim j
         ;  for (j=0;j<cls->n_ivps;j++)
            {  dim ndid  = cls->ivps[j].idx
            ;  mclp* ivp = mclvGetIvp(clannot, ndid, NULL)
            ;  if (!ivp)
               mcxDie(1, "query", "peculiarly peculiar cannot find %lu", (ulong) ndid)
            ;  ivp->val = cls->n_ivps
         ;  }
         }
         mcxIOclose(xfcl_g)
      ;  if (mode_get == 'n' && !user_imx)
         {  fputs("node\tclsize\n", xfout_g->fp)
         ;  for (i=0;i<clannot->n_ivps;i++)
            {  const mclp* p = clannot->ivps+i
            ;  mcx_dump_node(xfout_g->fp, tab, p->idx)
            ;  fprintf(xfout_g->fp, "\t%lu\n", (ulong) (0.5 + p->val))
         ;  }
            return 0
      ;  }
      }

   ;  if (mode_vary)
      vary_threshold
      (  xfmx_g
      ,  xfout_g->fp
      ,  vary_a
      ,  vary_z
      ,  vary_s
      ,  vary_n
      ,  mode_vary
      )

   ;  else if (mode_get)
      {  if (mode_get == 'd')
         {  const char* fmt
         ;  unsigned int format
         ;  long n_cols, n_rows

         ;  if (mclxReadDimensions(xfmx_g, &n_cols, &n_rows))
            mcxDie(1, me, "reading %s failed", xfmx_g->fn->str)
         ;  format = mclxIOformat(xfmx_g)
         ;  mcxIOclose(xfmx_g)

         ;  fmt = format == 'b' ? "binary" : format == 'a' ? "interchange" : "?"
         ;  fprintf
            (  xfout_g->fp
            ,  "%s format,  row x col dimensions are %ld x %ld\n"
            ,  fmt
            ,  n_rows
            ,  n_cols
            )
      ;  }
         else if (mode_get == 'n')
         {  dim i
         ;  mclx* mx = mclxRead(xfmx_g, EXIT_ON_FAIL)

         ;  if (transform_spec)
            { if (!(transform = mclgTFparse(NULL, transform_spec)))
               mcxDie(1, me, "input -tf spec does not parse")
            ;  mclgTFexec(mx, transform)
         ;  }

            if (cl && !MCLD_EQUAL(cl->dom_rows, mx->dom_cols))
            mcxDie(1, "query", "cluster row domain and matrix column domains differ")

         ;  fputs("node\tdegree\tmean\tmin\tmax\tmedian\tiqr", xfout_g->fp)
         ;  if (cl)
            fputs("\tclsize", xfout_g->fp)

         ;  fputc('\n', xfout_g->fp)

         ;  for (i=0;i<N_COLS(mx);i++)
            {  mclv* v = mclvClone(mx->cols+i)
            ;  double iqr = 0, med = 0, avg = 0, max = -DBL_MAX, min = DBL_MAX

            ;  mclvSortAscVal(v)

            ;  if (v->n_ivps)
               {  med = mcxMedian(v->ivps, v->n_ivps, sizeof v->ivps[0], ivp_get_double, &iqr)
               ;  avg = mclvSum(v) / v->n_ivps
               ;  max = v->ivps[v->n_ivps-1].val
               ;  min = v->ivps[0].val
            ;  }

               mcx_dump_node(xfout_g->fp, tab, mx->cols[i].vid)

            ;  fprintf
               (  xfout_g->fp
               ,  "\t%lu\t%g\t%g\t%g\t%g\t%g"
               ,  (ulong) v->n_ivps
               ,  avg
               ,  min
               ,  max
               ,  med
               ,  iqr
               )
            ;  if (clannot)
               fprintf(xfout_g->fp, "\t%lu", (ulong) (0.5 + clannot->ivps[i].val))

            ;  fputc('\n', xfout_g->fp)
            ;  mclvFree(&v)
         ;  }
            mclxFree(&mx)
      ;  }
         else if (mode_get == 'c')
         {  mclx* mx = mclxReadx(xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH | MCLX_REQUIRE_CANONICAL)
         ;  mclxAdjustLoops(mx, mclxLoopCBremove, NULL)
         ;  return test_cycle(mx, n_limit)
      ;  }
      }

      mcxIOclose(xfout_g)
   ;  mcxIOfree(&xfout_g)
   ;  mcxIOfree(&xfmx_g)
   ;  mcxFree(levels)
   ;  return 0
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


/*
-------------------------------------------------------------------------------
  L   D   R   S     cce *EWmean  *EWmed *EWiqr NDmean  NDmed  NDiqr      Cutoff 
-------------------------------------------------------------------------------
100   0   0   0   11142     140      69     11 1444.3   1034   2076        0.20
100   0   0   0   11142     210      69     11 1444.3   1034   2076        0.25
100   0   0   0   11142     280      69     11 1444.3   1034   2076        0.30
*/

