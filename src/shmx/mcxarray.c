/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
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
,  MY_OPT_IMX
,  MY_OPT_TAB
,  MY_OPT_L
,  MY_OPT_NORMALIZE
,  MY_OPT_RSKIP
,  MY_OPT_CSKIP
,  MY_OPT_PEARSON
,  MY_OPT_COSINE
,  MY_OPT_TP
,  MY_OPT_CUTOFF
,  MY_OPT_VERSION
,  MY_OPT_LQ
,  MY_OPT_GQ
,  MY_OPT_O
,  MY_OPT_TEAR
,  MY_OPT_TEARTP
,  MY_OPT_PI
,  MY_OPT_DIGITS
,  MY_OPT_HELP
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
,  {  "--transpose"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TP
   ,  NULL
   ,  "work with the transposed matrix"
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
,  {  "--cosine"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_COSINE
   ,  NULL
   ,  "work with the cosine"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "matrix file name"
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
   ,  "column containing labels (default 1)"
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


                     /* user rows will be columns. naming below is extremely confusing.
                      * user rows/cols are mcl cols/rows.
                     */
static mclx* read_data
(  mcxIO* xfin
,  mcxIO* xftab
,  unsigned skipr
,  unsigned skipc
,  unsigned labelidx
)
   {  mcxTing* line = mcxTingEmpty(NULL, 1000)
   ;  int  N_cols =  0
   ;  int  n_rows =  0
   ;  int  N_rows =  100
   ;  mclv* cols  =  mcxNAlloc(N_rows, sizeof(mclVector), mclvInit_v, EXIT_ON_FAIL)
   ;  mclv* scratch = mclvCanonical(NULL, 100, 1.0)
   ;  mclx* mx    =  mcxAlloc(sizeof mx[0], EXIT_ON_FAIL)
   ;  mcxHash* index = xftab ? mcxHashNew(100, mcxStrHash, mcxStrCmp) : NULL
   ;  long linect = 0

   ;  while (STATUS_OK == mcxIOreadLine(xfin, line, MCX_READLINE_CHOMP))
      {  const char* p = line->str
      ;  const char* z = p + line->len
      ;  int n_read = 0
      ;  int  n_cols =  0
      ;  double val = 0.0
      ;  unsigned skipcc = skipc

      ;  linect++

      ;  if (skipr > 0)
         {  skipr--
         ;  continue
      ;  }

         while (p < z)
         {  mcxbool seetab = strchr(p, '\t') ? TRUE : FALSE
         ;  while (p < z && skipcc > 0)
            {  const char* o = p
            ;  p = seetab ? strchr(p, '\t') : mcxStrChrIs(p, isspace, -1)
            ;  if (!p)
               break
            ;  if (xftab && (skipc+1-skipcc) == labelidx)
               {  char* label = mcxStrNDup(o, (dim) (p-o))
               ;  mcxKV* kv = mcxHashSearch(label, index, MCX_DATUM_INSERT)
               ;  if (kv->key != label)
                  mcxDie(1, me, "label <%s> occurs more than once", label)
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
         mcxDie(1, "mcxarray", "different column counts: %d vs %d", N_cols, n_cols)

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

      if (xftab)
      mcxIOclose(xftab)
   ;  mx->dom_cols = mclvCanonical(NULL, n_rows, 1.0)
   ;  mx->dom_rows = mclvCanonical(NULL, N_cols, 1.0)
   ;  mx->cols
      =  mcxNRealloc(cols, n_rows, N_rows, sizeof(mclVector), NULL, EXIT_ON_FAIL)
   ;  mcxTingFree(&line)

   ;  mclvFree(&scratch)
   ;  return mx
;  }


int main
(  int                  argc
,  const char*          argv[]
)
   {  int digits = MCLXIO_VALUE_GETENV
   ;  double cutoff = -1.0001, tear = 0.0, teartp = 0.0, pi = 0.0
   ;  double lq = DBL_MAX, gq = -DBL_MAX
   ;  mclx* tbl, *res
   ;  mcxIO* xfin = mcxIOnew("-", "r"), *xfout, *xftab = NULL
   ;  mclv* Nssqs, *sums, *scratch
   ;  mcxbool transpose = FALSE
   ;  mcxbool read_matrix = FALSE, z_score = FALSE
   ;  mcxbool cutoff_specified = FALSE
   ;  const char* out = "out.array"
   ;  mcxbool mode = 'p'
   ;  mcxstatus parseStatus = STATUS_OK
   ;  int n_mod
   ;  unsigned rskip = 0, cskip = 0, labelidx = 0
   ;  double N = 1.0

   ;  mcxOption* opts, *opt
   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)
   
   ;  if
      (!(opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)))
      exit(0)

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

            case MY_OPT_IMX
         :  mcxIOnewName(xfin, opt->val)
         ;  read_matrix = TRUE
         ;  break
         ;

            case MY_OPT_L
         :  labelidx = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_TAB
         :  xftab = mcxIOnew(opt->val, "w")
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

            case MY_OPT_COSINE
         :  mode = 'c'
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

   ;  mcxOptFree(&opts)

   ;  if (!cutoff_specified)
      mcxDie(1, me, "-co <cutoff> option is required")
   ;  if (labelidx && labelidx > cskip)
      mcxDie(1, me, "labelidx value requires larger or equally large skipc argument")
   ;  else if (xftab && !labelidx && cskip == 1)
      labelidx = 1
   ;  else if (xftab && !labelidx)
      mcxDie(1, me, "which column gives the label? Use -l")

   ;  xfout = mcxIOnew(out, "w")
   ;  mcxIOopen(xfin, EXIT_ON_FAIL)
   ;  mcxIOopen(xfout, EXIT_ON_FAIL)
   ;  if (xftab)
      mcxIOopen(xftab, EXIT_ON_FAIL)

   ;  tbl
      =     read_matrix
         ?  mclxRead(xfin, EXIT_ON_FAIL)
         :  read_data(xfin, xftab, rskip, cskip, labelidx)

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

      if (tear)
      mclxInflate(tbl, tear)

   ;  if (transpose)
      {  mclx* tblt = mclxTranspose(tbl)
      ;  mclxFree(&tbl)
      ;  tbl = tblt
      ;  if (teartp)
         mclxInflate(tbl, teartp)
   ;  }

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
         ;  dim d

         ;  if (mode == 'c')
            {  for (d=c;d<N_COLS(tbl);d++)
               {  double ip = mclvIn(tbl->cols+c, tbl->cols+d)
               ;  double nom = sqrt(Nssqs->ivps[c].val  * Nssqs->ivps[d].val) / N
               ;  double score = nom ? ip / nom : 0.0
               ;  if (score >= cutoff)
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
   *     /  _    __        __ 2   _       _    __        __ 2   _   |
   * \  /  |     \   2     \       |     |     \   2     \       | 
   *  \/   |_  n /_ x   -  /_ x   _|  *  |_  n /_ y   -  /_ y   _| 
  */

            else if (mode == 'p')
            {  for (d=c;d<N_COLS(tbl);d++)
               {  double ip = mclvIn(tbl->cols+c, tbl->cols+d)
               ;  double score

               ;  double s2   =  sums->ivps[d].val
               ;  double nom  =  sqrt((Nsq1 - s1sq) * (Nssqs->ivps[d].val - s2*s2))
               ;  score       =  nom ? ((N*ip - s1*s2) / nom) : -1.0
               ;  if (score >= cutoff)
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
      }

      mclxAddTranspose(res, 0.5)

   ;  mclxFree(&tbl)
   ;  mclvFree(&Nssqs)
   ;  mclvFree(&sums)
   ;  mclvFree(&scratch)

   ;  if (pi)
      mclxInflate(res, pi)

   ;  mclxWrite(res, xfout, digits, EXIT_ON_FAIL)
   ;  mclxFree(&res)

   ;  mcxIOfree(&xfin)
   ;  mcxIOfree(&xfout)
   ;  return 0
;  }

