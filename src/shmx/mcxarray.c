/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007, 2008, 2009  Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/*    TODO
 * support missing data. sum and sumsq can still be precomputed,
 *    only inproduct has to be adjusted every time.
 *    Figure out what N should be though.
 * Euclidean distance,
 * Taxi distance
*/


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

#include "impala/compose.h"
#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/pval.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/app.h"

#include "clew/clm.h"

#include "util/io.h"
#include "util/ting.h"
#include "util/ding.h"
#include "util/err.h"
#include "util/minmax.h"
#include "util/opt.h"
#include "util/types.h"

const char* me = "mcxarray";

const char* syntax = "Usage: mcxarray <-data <data-file> | -imx <mcl-file> [options]";

enum
{  MY_OPT_DATA
,  MY_OPT_TABLE
,  MY_OPT_TAB
,  MY_OPT_L
,  MY_OPT_NORMALIZE
,  MY_OPT_RSKIP
,  MY_OPT_CSKIP
,  MY_OPT_PEARSON
,  MY_OPT_RANK
,  MY_OPT_COSINE
,  MY_OPT_TP
,  MY_OPT_CUTOFF
,  MY_OPT_VERSION
,  MY_OPT_LQ
,  MY_OPT_GQ
,  MY_OPT_O
,  MY_OPT_WRITEBINARY
,  MY_OPT_PI
,  MY_OPT_TRANSFORM
,  MY_OPT_DIGITS
,  MY_OPT_WRITE_TABLE
,  MY_OPT_HELP
}  ;


  /* Pearson correlation coefficient:
   *                     __          __   __ 
   *                     \           \    \  
   *                  n  /_ x y   -  /_ x /_ y
   *   -----------------------------------------------------------
   *      ___________________________________________________________
   *     /       __        __ 2                __        __ 2       |
   * \  /  /     \   2     \       \     /     \   2     \       \ 
   *  \/   \   n /_ x   -  /_ x    /  *  \   n /_ y   -  /_ y    / 
  */


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


typedef struct rank_unit
{  long     index
;  double   ord
;  double   value
;
}  rank_unit   ;


void*  rank_unit_init
(  void*   ruv
)
   {  rank_unit* ru =   (rank_unit*) ruv
   ;  ru->index     =   -1
   ;  ru->value     =   -1
   ;  ru->ord       =   -1.0
   ;  return ru
;  }


int rank_unit_cmp_index
(  const void* rua
,  const void* rub
)
   {  long a = ((rank_unit*) rua)->index
   ;  long b = ((rank_unit*) rub)->index
   ;  return a < b ? -1 : a > b ? 1 : 0
;  }


int rank_unit_cmp_value
(  const void* rua
,  const void* rub
)
   {  double a = ((rank_unit*) rua)->value
   ;  double b = ((rank_unit*) rub)->value
   ;  return a < b ? -1 : a > b ? 1 : 0
;  }


mcxOptAnchor options[]
=
{
   {  "-h"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_HELP
   ,  NULL
   ,  "print this help"
   }
,  {  "--help"
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
,  {  "--transpose"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TP
   ,  NULL
   ,  "work with the transposed data matrix"
   }
,  {  "-skipc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CSKIP
   ,  "<num>"
   ,  "skip this many columns"
   }
,  {  "-skipr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_RSKIP
   ,  "<num>"
   ,  "skip this many rows"
   }
,  {  "-n"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_NORMALIZE
   ,  "z"
   ,  "normalize on z-scores"
   }
,  {  "--pearson"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_PEARSON
   ,  NULL
   ,  "work with Pearson correlation score (default)"
   }
,  {  "--spearman"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_RANK
   ,  NULL
   ,  "work with Spearman rank correlation score"
   }
,  {  "--cosine"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_COSINE
   ,  NULL
   ,  "work with the cosine"
   }
,  {  "--write-binary"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_WRITEBINARY
   ,  NULL
   ,  "write in binary format (use with low -co and subsequent mcx q --vary-threshold)"
   }
,  {  "-data"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DATA
   ,  "<fname>"
   ,  "data file name"
   }
,  {  "-write-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "write labels to tab file"
   }
,  {  "-l"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_L
   ,  "<int>"
   ,  "column (or row, with --transpose) containing labels (default 1)"
   }
,  {  "-write-table"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_WRITE_TABLE
   ,  "<fname>"
   ,  "write table file to file (debug option)"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TABLE
   ,  "<fname>"
   ,  "matrix file name"
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
,  {  "-tf"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TRANSFORM
   ,  "<func(arg)[, func(arg)]*>"
   ,  "apply unary transformations to matrix values"
   }
,  {  "-digits"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DIGITS
   ,  "<int>"
   ,  "precision to use in interchange format"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


static mclTab* tab_g = NULL;

      /* user rows will be columns. naming below is extremely confusing.
       * user rows/cols are mcl cols/rows.
      */
static mclx* read_data
(  mcxIO* xfin
,  mcxIO* xftab
,  unsigned skipr
,  unsigned skipc
,  unsigned labelidx
,  mcxbool  transpose
)
   {  mcxTing* line = mcxTingEmpty(NULL, 1000)
   ;  dim  N_cols =  0
   ;  int  n_rows =  0
   ;  int  N_rows =  100
   ;  mclv* cols  =  mcxNAlloc(N_rows, sizeof(mclVector), mclvInit_v, EXIT_ON_FAIL)
   ;  mclv* scratch = mclvCanonical(NULL, 100, 1.0)
   ;  mclx* mx    =  mcxAlloc(sizeof mx[0], EXIT_ON_FAIL)
   ;  mcxHash* index = xftab ? mcxHashNew(100, mcxStrHash, mcxStrCmp) : NULL       /* user rows */
   ;  long linect = 0
   ;  unsigned skiprr = skipr
   ;  mcxbool seetab_labels = FALSE

   ;  while (STATUS_OK == mcxIOreadLine(xfin, line, MCX_READLINE_CHOMP))
      {  const char* p = line->str
      ;  const char* z = p + line->len
      ;  int n_read = 0
      ;  dim  n_cols =  0
      ;  double val = 0.0
      ;  unsigned skipcc = skipc

      ;  linect++

      ;  if (skiprr > 0)
         {  if (transpose && skipr+1 - skiprr == labelidx)        /* this is the line with labels */
            {  seetab_labels = strchr(p, '\t') ? TRUE : FALSE
            ;  while (p < z)
               {  const char* o = p
               ;  p = seetab_labels ? strchr(p, '\t') : mcxStrChrIs(p, isspace, -1)

               ;  if (!p)
                  p = z

               ;  if (skipcc == 0)              /* skipped all we needed to */
                  {  N_cols++
                  ;  if (xftab)
                     {  char* label = mcxStrNDup(o, (dim) (p-o))
                     ;  mcxKV* kv = mcxHashSearch(label, index, MCX_DATUM_INSERT)
                     ;  if (kv->key != label)
                        mcxDie(1, me, "column label <%s> occurs more than once", label)
                     ;  fprintf(xftab->fp, "%d\t%s\n", (int) (N_cols-1), label)
                  ;  }
                  }
                  else
                  skipcc--
               ;  p = seetab_labels ? p + 1 : mcxStrChrAint(p, isspace, -1)
            ;  }
            }
            skiprr--
         ;  continue
      ;  }

         while (p < z)
         {  mcxbool seetab = strchr(p, '\t') ? TRUE : FALSE
         ;  if ((seetab_labels && !seetab) || (!seetab_labels && !seetab))
            mcxDie(1, "mcxarray", "Confused: spaces or tabs as separator? at line %d", (int) linect)
         ;  while (p < z && skipcc > 0)
            {  const char* o = p
            ;  p = seetab ? strchr(p, '\t') : mcxStrChrIs(p, isspace, -1)
            ;  if (!p)
               break
            ;  if (!transpose && xftab && (skipc+1-skipcc) == labelidx)
               {  char* label = mcxStrNDup(o, (dim) (p-o))
               ;  mcxKV* kv = mcxHashSearch(label, index, MCX_DATUM_INSERT)
               ;  if (kv->key != label)
                  mcxDie(1, me, "row label <%s> occurs more than once", label)
               ;  fprintf(xftab->fp, "%d\t%s\n", n_rows, label)
            ;  }
               p = seetab ? p + 1 : mcxStrChrAint(p, isspace, -1)
            ;  skipcc--
         ;  }

            if (skipcc || p >= z)
            break
            
         ;  if (1 != sscanf(p, "%lf%n", &val, &n_read))
            break                /* fixme err? */
         ;  p += n_read
         ;  if (n_cols >= scratch->n_ivps)
            mclvCanonicalExtend(scratch, scratch->n_ivps * 1.44, 0.0)
         ;  scratch->ivps[n_cols].val = val
         ;  n_cols++
      ;  }

         if (!n_cols && !N_cols)
         mcxDie(1, "mcxarray", "nothing read at line %ld", linect)
      ;  else if (!N_cols)
         N_cols = n_cols
      ;  else if (n_cols != N_cols)
         mcxDie(1, "mcxarray", "different column counts: %lu vs %lu", (ulong) N_cols, (ulong) n_cols)

      ;  if (n_rows == N_rows)
         {  N_rows *= 1.44
         ;  cols
            =  mcxNRealloc
               (cols, N_rows, n_rows, sizeof(mclVector), mclvInit_v, EXIT_ON_FAIL)
      ;  }

         mclvFromIvps(cols+n_rows, scratch->ivps, n_cols)
      ;  cols[n_rows].vid = n_rows
      ;  n_rows++
   ;  }

      if (!n_rows)
      mcxErr(me, "no rows read")

   ;  if (xftab)
      {  if (!strcmp(xftab->fn->str, "-"))
         mcxErr(me, "cannot read back tab, warnings will be as offsets")
      ;  else
            mcxIOclose(xftab)
         ,  mcxIOrenew(xftab, NULL, "r")
         ,  tab_g = mclTabRead(xftab, NULL, EXIT_ON_FAIL)
   ;  }

      if (xftab)
      mcxIOclose(xftab)

   ;  mx->dom_cols = mclvCanonical(NULL, n_rows, 1.0)
   ;  mx->dom_rows = mclvCanonical(NULL, N_cols, 1.0)
   ;  mx->cols
      =  mcxNRealloc(cols, n_rows, N_rows, sizeof(mclVector), NULL, EXIT_ON_FAIL)
   ;  mcxTingFree(&line)
   ;  mclvFree(&scratch)

   ;  if (transpose)
      {  mclx* mxtp = mclxTranspose(mx)
      ;  mclxFree(&mx)
      ;  mx = mxtp
   ;  }
      return mx
;  }


int main
(  int                  argc
,  const char*          argv[]
)
   {  int digits = MCLXIO_VALUE_GETENV
   ;  double cutoff = -1.0001, pi = 0.0
   ;  double lq = DBL_MAX, gq = -DBL_MAX
   ;  mclx* res
   ;  mcxIO* xfin = mcxIOnew("-", "r"), *xfout = NULL, *xftab = NULL, *xftbl = NULL
   ;  mclv* Nssqs, *sums, *scratch
   ;  mcxbool transpose = FALSE
   ;  mcxbool read_table = FALSE, z_score = FALSE
   ;  mcxbool cutoff_specified = FALSE
   ;  mcxbool write_binary = FALSE
   ;  const char* out = "-"
   ;  mcxbool mode = 'p'
   ;  mcxstatus parseStatus = STATUS_OK
   ;  int n_mod
   ;  unsigned rskip = 0, cskip = 0, labelidx = 0
   ;  double N = 1.0
   ;  mclpAR* transform  =   NULL
   ;  mcxTing* transform_spec = NULL

   ;  mcxOption* opts, *opt
   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)
   
   ;  if
      (!(opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)))
      exit(0)

   ;  mcxLogLevel =
      MCX_LOG_AGGR | MCX_LOG_MODULE | MCX_LOG_IO | MCX_LOG_GAUGE | MCX_LOG_WARN
   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)
   ;  mclx_app_init(stderr)

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

            case MY_OPT_TABLE
         :  mcxIOnewName(xfin, opt->val)
         ;  read_table = TRUE
         ;  break
         ;

            case MY_OPT_L
         :  labelidx = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_WRITE_TABLE
         :  xftbl = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_TAB
         :  xftab = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_TRANSFORM
         :  transform_spec = mcxTingNew(opt->val)
         ;  break
         ;

            case MY_OPT_DATA
         :  mcxIOnewName(xfin, opt->val)
         ;  break
         ;

            case MY_OPT_RSKIP
         :  rskip = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_CSKIP
         :  cskip = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_NORMALIZE
         :  z_score = TRUE
         ;  break
         ;

            case MY_OPT_PEARSON
         :  mode = 'p'
         ;  break
         ;

            case MY_OPT_RANK
         :  mode = 'r'
         ;  break
         ;

            case MY_OPT_COSINE
         :  mode = 'c'
         ;  break
         ;

            case MY_OPT_WRITEBINARY
         :  write_binary = TRUE
         ;  break
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  return 0
         ;

            case MY_OPT_CUTOFF
         :  cutoff = atof(opt->val)
         ;  cutoff_specified = TRUE
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

            case MY_OPT_DIGITS
         :  digits = strtol(opt->val, NULL, 10)
         ;  break
      ;  }
      }

   ;  mcxOptFree(&opts)

   ;  if (!cutoff_specified)
      mcxDie(1, me, "-co <cutoff> option is required")
   ;  if (labelidx && ((transpose && labelidx > rskip) || (!transpose && labelidx > cskip)))
      mcxDie(1, me, "-l value requires larger or equally large skipc/skipr argument")
   ;  else if (transpose && xftab && !labelidx && rskip == 1)
      labelidx = 1
   ;  else if (!transpose && xftab && !labelidx && cskip == 1)
      labelidx = 1
   ;  else if (xftab && !labelidx)
      mcxDie(1, me, "which column or row gives the label? Use -l")

   ;  xfout = mcxIOnew(out, "w")
   ;  mcxIOopen(xfin, EXIT_ON_FAIL)
   ;  mcxIOopen(xfout, EXIT_ON_FAIL)
   ;  if (xftab)
      mcxIOopen(xftab, EXIT_ON_FAIL)
   ;

      {  mclx* tbl =    read_table
               ?  mclxRead(xfin, EXIT_ON_FAIL)
               :  read_data(xfin, xftab, rskip, cskip, labelidx, transpose)

      ;  if (read_table && transpose)
         {  mclx* tp = mclxTranspose(tbl)
         ;  mclxFree(&tbl)
         ;  tbl = tp
      ;  }

         if (xftbl)
         {  if (write_binary)
            mclxbWrite(tbl, xftbl, EXIT_ON_FAIL)
         ;  else
            mclxWrite(tbl, xftbl, digits, EXIT_ON_FAIL)
         ;  mcxIOfree(&xftbl)
      ;  }

         if (lq < DBL_MAX)
         {  double mass = mclxMass(tbl)
         ;  double kept = mclxSelectValues(tbl, NULL, &lq, MCLX_EQT_LQ)
         ;  fprintf(stderr, "orig %.2f kept %.2f\n", mass, kept)
      ;  }

         if (gq > -DBL_MAX)
         {  double mass = mclxMass(tbl)
         ;  double kept = mclxSelectValues(tbl, &gq, NULL, MCLX_EQT_GQ)
         ;  fprintf(stderr, "orig %.2f kept %.2f\n", mass, kept)
      ;  }

         if (z_score)
         {  mclx* tblt = mclxTranspose(tbl)
   #if 0
   ;mcxIO* xftest = mcxIOnew("tttt", "w")
   #endif
         ;  double std, mean
         ;  dim c
         ;  for (c=0;c<N_COLS(tblt);c++)
            {  mclvMean(tblt->cols+c, N_ROWS(tblt), &mean, &std)
            ;  mclvMove(tblt->cols+c, mean, std)
         ;  }
            mclxFree(&tbl)
         ;  tbl = mclxTranspose(tblt)
   #if 0
   ;mclxWrite(tbl, xftest, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   #endif
         ;  mclxFree(&tblt)
      ;  }

         if (mode == 'r')
         {  dim d
         ;  rank_unit* ru
            =  mcxNAlloc
               (  N_ROWS(tbl) + 1            /* one extra for sentinel */
               ,  sizeof(rank_unit)
               ,  rank_unit_init
               ,  EXIT_ON_FAIL
               )
         ;  for (d=0;d<N_COLS(tbl);d++)
            {  mclv* v = tbl->cols+d
            ;  dim stretch = 1
            ;  dim i
            ;  for (i=0;i<v->n_ivps;i++)
               {  ru[i].index = v->ivps[i].idx
               ;  ru[i].value = v->ivps[i].val
               ;  ru[i].ord = -14
            ;  }
               qsort(ru, v->n_ivps, sizeof ru[0], rank_unit_cmp_value)
            ;  ru[v->n_ivps].value = ru[v->n_ivps-1].value + 1    /* sentinel out-of-bounds value */
            ;  for (i=0;i<v->n_ivps;i++)
               {  if (ru[i].value != ru[i+1].value)    /* input current stretch */
                  {  dim j
                  ;  for (j=0;j<stretch;j++)
                     ru[i-j].ord = 1 + (i+1 + i-stretch) / 2.0
                  ;  stretch = 1
               ;  }
                  else
                  stretch++
            ;  }
               qsort(ru, v->n_ivps, sizeof ru[0], rank_unit_cmp_index)
            ;  for (i=0;i<v->n_ivps;i++)
               v->ivps[i].val = ru[i].ord
   /* fprintf(stderr, "vid %d id %d value from %.4g to %.1f\n", (int) v->vid, (int) v->ivps[i].idx, (double) ru[i].value, (double) ru[i].ord) */
         ;  }
         }

         Nssqs = mclvCopy(NULL, tbl->dom_cols)
      ;  sums = mclvCopy(NULL, tbl->dom_cols)
      ;  scratch = mclvCopy(NULL, tbl->dom_cols)

      ;  N  = MCX_MAX(N_ROWS(tbl), 1)      /* fixme; bit odd */

      ;  {  dim c
         ;  for (c=0;c<N_COLS(tbl);c++)
            {  double sumsq = mclvPowSum(tbl->cols+c, 2.0)
            ;  double sum = mclvSum(tbl->cols+c)
            ;  Nssqs->ivps[c].val = N * sumsq
            ;  sums->ivps[c].val = sum
         ;  }
         }

         res   =
         mclxAllocZero
         (  mclvCopy(NULL, tbl->dom_cols)
         ,  mclvCopy(NULL, tbl->dom_cols)
         )

      ;  n_mod =  MCX_MAX(1+(N_COLS(tbl)-1)/40, 1)

      ;  {  dim c
         ;  for (c=0;c<N_COLS(tbl);c++)
            {  ofs s = 0
            ;  double s1   =  sums->ivps[c].val
            ;  double s1sq =  s1 * s1
            ;  double Nsq1 =  Nssqs->ivps[c].val
            ;  double nomleft = sqrt(Nsq1 - s1sq)
            ;  dim d

            ;  if (mode == 'c')
               {  for (d=c;d<N_COLS(tbl);d++)
                  {  double ip = mclvIn(tbl->cols+c, tbl->cols+d)
                  ;  double nom = sqrt(Nssqs->ivps[c].val  * Nssqs->ivps[d].val) / N
                  ;  double score = nom ? ip / nom : 0.0, absscore = score

                  ;  if (absscore < 0)
                     absscore *= -1
                     
                  ;  if (absscore >= cutoff)
                        scratch->ivps[s].val = score
                     ,  scratch->ivps[s].idx = d
                     ,  s++
               ;  }
               }

     /* Pearson correlation coefficient:
      *                     __          __   __ 
      *                     \           \    \  
      *                  n  /_ x y   -  /_ x /_ y
      *   -----------------------------------------------------------
      *      ___________________________________________________________
      *     /       __        __ 2                __        __ 2       |
      * \  /  /     \   2     \       \     /     \   2     \       \ 
      *  \/   \   n /_ x   -  /_ x    /  *  \   n /_ y   -  /_ y    / 
     */

               else if (mode == 'p' || mode == 'r')
               {  for (d=c;d<N_COLS(tbl);d++)
                  {  double ip   =  mclvIn(tbl->cols+c, tbl->cols+d)
                  ;  double s2   =  sums->ivps[d].val
                  ;  double nom  =  nomleft * sqrt(Nssqs->ivps[d].val - s2*s2)
                  ;  double score=  nom ? ((N*ip - s1*s2) / nom) : -1.0
                  ;  double absscore = score
                  ;  ofs offending = nomleft ? d : c        /* prepare in case !nom */

                  ;  if (!nom && tbl->dom_cols->ivps[offending].val < 1.5)
                     {  char* label = tab_g ? mclTabGet(tab_g, offending, NULL) : NULL
                     ;  if (label)
                        mcxErr(me, "constant data for label <%s> - no pearson", label)
                     ;  else
                        mcxErr
                        (  me
                        ,  "constant data for %s %ld (mcl identifier %ld) - no pearson"
                        ,  (transpose ? "column" : "row")
                        ,  (long) (offending+1)
                        ,  (long) offending
                        )
                     ;  tbl->dom_cols->ivps[offending].val = 2
                  ;  }

                     if (absscore < 0)
                     absscore *= -1
                     
                  ;  if (absscore >= cutoff)
                        scratch->ivps[s].val = score
                     ,  scratch->ivps[s].idx = d
                     ,  s++
               ;  }
               }

               mclvFromIvps(res->cols+c, scratch->ivps, s)

            ;  if ((c+1) % n_mod == 0)
                  fputc('.', stderr)
               ,  fflush(NULL)
         ;  }
            if (c % n_mod)
            fputc('\n', stderr)
      ;  }

         mclxAddTranspose(res, 0.5)
      ;  mclxFree(&tbl)
   ;  }

      if (transform_spec)
      { if (!(transform = mclpTFparse(NULL, transform_spec)))
         mcxDie(1, me, "input -tf spec does not parse")
      ;  mclxUnaryList(res, transform)
   ;  }

   ;  if (pi)
      mclxInflate(res, pi)

   ;  mclvFree(&Nssqs)
   ;  mclvFree(&sums)
   ;  mclvFree(&scratch)

   ;  if (write_binary)
      mclxbWrite(res, xfout, EXIT_ON_FAIL)
   ;  else
      mclxWrite(res, xfout, digits, EXIT_ON_FAIL)

   ;  mclxFree(&res)

   ;  mcxIOfree(&xfin)
   ;  mcxIOfree(&xfout)
   ;  return 0
;  }

