/*   (C) Copyright 2006, 2007, 2008 Stijn van Dongen
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
#include "util/compile.h"

#include "impala/matrix.h"
#include "impala/io.h"


int valcmp
(  const void*             i1
,  const void*             i2
)
   {  return *((pval*)i1) > *((pval*)i2) ? 1 : *((pval*)i1) < *((pval*)i2) ? -1 : 0
;  }


enum
{  MY_OPT_IMX = MCX_DISP_UNUSED
,  MY_OPT_DIMENSION
,  MY_OPT_STATISTICS
,  MY_OPT_STATISTICS_LIST
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
,  {  "--edge"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_STATISTICS
   ,  NULL
   ,  "get simple edge weight statistics"
   }
,  {  "--edge-list"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_STATISTICS_LIST
   ,  NULL
   ,  "get simple edge weight statistics for each node separately"
   }
,  {  NULL ,  0 ,  0,  NULL, NULL}
}  ;


static  mcxbool get_dim    =     -1;
static  mcxbool get_stat   =     -1;
static  mcxbool get_stat_list =  -1;
static  mcxIO*  xfout      =     (void*) -1;
static  mcxIO* xfmx_g      =   (void*) -1;


static mcxstatus qInit
(  void
)
   {  get_dim  =  FALSE
   ;  get_stat =  FALSE
   ;  get_stat_list =  FALSE
   ;  xfout    =  mcxIOnew("-", "w")
   ;  xfmx_g   =  mcxIOnew("-", "r")
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

         case MY_OPT_STATISTICS_LIST
      :  get_stat_list = TRUE
      ;  break
      ;

         case MY_OPT_FOUT
      :  mcxIOnewName(xfout, val)
      ;  break
      ;

         case MY_OPT_STATISTICS
      :  get_stat = TRUE
      ;  break
      ;

         default
      :  mcxExit(1) 
      ;
   ;  }
      return STATUS_OK
;  }


static mcxstatus qMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)
   {  if (get_dim + get_stat + get_stat_list == 0)
      get_stat = TRUE

   ;  mcxIOopen(xfout, EXIT_ON_FAIL)

   ;  if (get_dim)
      {  const char* fmt
      ;  int format
      ;  long n_cols, n_rows

      ;  if (mclxReadDimensions(xfmx_g, &n_cols, &n_rows))
         mcxDie(1, "mcx q", "reading %s failed", xfmx_g->fn->str)

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

      if (get_stat)
      {  mclx* mx = mclxRead(xfmx_g, EXIT_ON_FAIL)
      ;  mclv* sums = mclxColSums(mx, MCL_VECTOR_COMPLETE)
      ;  double total = mclvSum(sums)
      ;  dim n_copied = 0

      ;  unsigned long noe = mclxNrofEntries(mx)
      ;  double val_avg, val_sdv, val_med
      ;  double nb_avg , nb_sdv , nb_med
      ;  dim i, j

      ;  pval*  allvals = mcxAlloc(noe * sizeof allvals[0], EXIT_ON_FAIL)
      ;  mclv* colsizes = mclxColSizes(mx, MCL_VECTOR_COMPLETE)

      ;  if (!noe)
         {  fprintf
            (  xfout->fp
            ,  "nb_all=0 nb_avg=0 nb_sdv=0 nb_med=0 val_avg=0 val_sdv=0 val_med=0"
            )
         ;  mcxExit(0)
      ;  }

         val_avg = total / noe
      ;  val_sdv = 0.0

      ;  nb_avg = noe / N_COLS(mx)
      ;  nb_sdv = 0.0
      ;  nb_med = 0.0

      ;  for (i=0;i<N_COLS(mx);i++)
         {  mclv* vec = mx->cols+i
         ;  nb_sdv += (nb_avg - (long) vec->n_ivps) * (nb_avg - (long) vec->n_ivps)
         ;  for (j=0;j<vec->n_ivps;j++)
            {  double val = vec->ivps[j].val
            ;  val_sdv += (val -val_avg) * (val -val_avg)
            ;  allvals[n_copied+j] = val
         ;  }
            n_copied += vec->n_ivps
      ;  }

         val_sdv = sqrt(val_sdv/noe)
      ;  nb_sdv = sqrt(nb_sdv/N_COLS(mx))

      ;  qsort(allvals, noe, sizeof(pval), valcmp)

      ;  if (noe > 1)
         val_med = (allvals[noe/2] + allvals[(noe-1)/2]) / 2.0
      ;  else if (noe == 1)
         val_med = allvals[0]
      ;  else
         val_med = 0.0

      ;  mclvSort(colsizes, mclpValCmp)
      ;  if (N_COLS(mx) > 1)
         nb_med = (colsizes->ivps[N_COLS(mx)/2].val + colsizes->ivps[(N_COLS(mx)-1)/2].val) / 2.0
      ;  else if (N_COLS(mx) == 1)
         nb_med = colsizes->ivps[0].val
      ;  else
         nb_med = 0.0

      ;  fprintf
         (  xfout->fp
         ,  "nb_all=%lu"      " nb_avg=%.3f"    " nb_sdv=%.3f"    " nb_med=%.0f"
            " val_avg=%.3f"   " val_sdv=%.3f"   " val_med=%.3f"
            " val_min=%.3f"   " val_max=%.3f"
            " n_cols=%lu"     " n_rows=%lu"
         ,  noe            ,  nb_avg         ,  nb_sdv         ,  nb_med
         ,  val_avg        ,  val_sdv        ,  val_med
         ,  allvals[0]     ,  allvals[noe-1]
         ,  (ulong) N_COLS(mx)
         ,  (ulong) N_ROWS(mx)
         )

      ;  for (i=0;i<=8;i++)
         {  dim o = (noe * i) / 8
         ;  if (o >= noe)
            o = noe-1
         ;  fprintf(xfout->fp, " val_oc%lu=%.3f", (ulong) i, allvals[o])
      ;  }
         fputc('\n', xfout->fp)
   ;  }
      else if (get_stat_list)
      {  mclx* mx = mclxRead(xfmx_g, EXIT_ON_FAIL)
      ;  mclv* sums = mclxColSums(mx, MCL_VECTOR_COMPLETE)
      ;  dim i
      ;  for (i=0;i<sums->n_ivps;i++)
         {  unsigned long ne = mx->cols[i].n_ivps
         ;  double ew = sums->ivps[i].val
         ;  if (ne)
            fprintf(xfout->fp, "%lu\t%lu\t%g\n", (ulong) sums->ivps[i].idx, ne, ew/ne)
         ;  else
            fprintf(xfout->fp, "%lu\t%lu\t-\n", (ulong) sums->ivps[i].idx, ne)
      ;  }
      }
      return 0
;  }


mcxDispHook* mcxDispHookq
(  void
)
   {  static mcxDispHook qEntry
   =  {  "q"
      ,  "q [options]"
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


