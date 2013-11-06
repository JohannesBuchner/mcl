/*      Copyright (C) 2002, 2003, 2004, 2005 Stijn van Dongen
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
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/app.h"

#include "mcl/interpret.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"

/*
 *    Ideal scenario: only read in the vectors that are needed, i.e. with a mask.
 *    The mask can be obtained from header information.
 *    (The needed vectors are those for which the vids are in the residue,
 *    which contains the vectors not covered by the clustering).
 *    This has not yet been implemented (and is for performance only).
*/

const char* usagelines[] =
{  "Usage: clmresidue [options] -icl <fname> -imx <fname>"
,  ""
,  "Options:"
,  "  -rpm <fname>     residue projection matrix, node/cluster weight distribution"
,  "  -mvp <fname>     majority vote projection, clustering for the full graph"  
,  NULL
}  ;

int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO       *xfcl      =  NULL
   ;  mcxIO       *xfmx      =  NULL
   ;  mcxIO       *xfResidue  =  NULL
   ;  mcxTing     *fnResidue  =  mcxTingNew("out.rpm")
   ;  mcxIO       *xfProjection  =  NULL
   ;  mcxTing     *fnProjection  =  mcxTingNew("out.mvp")

   ;  mclMatrix   *cl         =  NULL
   ;  mclMatrix   *cl2el      =  NULL
   ;  mclMatrix   *mx         =  NULL
   ;  mclMatrix   *mxres      =  NULL
   ;  mclMatrix   *clmxres    =  NULL

   ;  mclVector   *meet       =  NULL
   ;  mclVector   *residue    =  NULL

   ;  int         a           =  1
   ;  int         adapt       =  0
   ;  const char* me          =  "clmresidue"
   ;  int i, o, m, e
   ;  int n_fix   =  0
   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_NO)

   ;  if (argc == 1)
      {  help
      :  mcxUsage(stdout, me, usagelines)
      ;  mcxExit(0)
   ;  }

      while(a < argc)
      {  if (!strcmp(argv[a], "-icl"))
         {  if (a++ + 1 < argc)
            xfcl =  mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-mvp"))
         {  if (a++ + 1 < argc)
            mcxTingWrite(fnProjection, argv[a])
         ;  else
            goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-rpm"))
         {  if (a++ + 1 < argc)
            mcxTingWrite(fnResidue, argv[a])
         ;  else
            goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-imx"))
         {  if (a++ + 1 < argc)
            xfmx = mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "--version"))
         {  app_report_version(me)
         ;  exit(0)
      ;  }
         else if (!strcmp(argv[a], "--adapt"))
         {  adapt = 1
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

      if (!xfcl || !xfmx)
      goto help

   ;  xfResidue = mcxIOnew(fnResidue->str, "w")
   ;  xfProjection = mcxIOnew(fnProjection->str, "w")

   ;  cl =  mclxRead(xfcl, EXIT_ON_FAIL)
   ;  mx =  mclxReadx(xfmx, EXIT_ON_FAIL, MCL_READX_GRAPH)

   ;  meet = mcldMeet(cl->dom_rows, mx->dom_cols, NULL)

   ;  if (meet->n_ivps != N_ROWS(cl))
      {  if (adapt)
         {  mclMatrix* tcl
         ;  mcxTell(me,  "Clustering has spurious nodes - must project")
         ;  report_domain(me, N_ROWS(mx), N_ROWS(cl), meet->n_ivps)
         ;  tcl = mclxSub(cl, cl->dom_cols, meet) 
         ;  mclxFree(&cl)
         ;  cl = tcl
      ;  }
         else
         report_exit(me, SHCL_ERROR_DOMAIN)
   ;  }

      residue = mcldMinus(mx->dom_cols, meet, NULL)

     /* fixme; no dummy cluster added -  breaks general behaviour */
   ;  if (!residue->n_ivps)
      {  mcxErr(me,  "Ranges are identical - no residue to work with!")
      ;  mcxErr(me,  "You still get a <%ld>x<0> matrix", (long) N_COLS(cl))
      ;  clmxres = mclxAllocZero(mclvInit(NULL), mclvCopy(NULL, cl->dom_cols))
      ;  mclxWrite(clmxres, xfResidue, 6, EXIT_ON_FAIL);
      ;  mcxExit(0)
   ;  }
      else
      {  mcxTell(me, "Residue has <%ld> nodes", (long) residue->n_ivps)
   ;  }

     /* fixme: this only in adapt mode */
      if (mclcEnstrict(cl, &o, &m, &e, ENSTRICT_TRULY))
      {  report_partition(me, cl, xfcl->fn, o, m, e)
      ;  report_fixit(me,  n_fix++)
   ;  }

     /* add residue nodes as separate cluster */
      {  long newvid     =  mclvHighestIdx(cl->dom_cols) + 1
      ;  mclVector* clus
      ;  int n_cols_new = N_COLS(cl) + 1

      ;  cl->cols       =  (mclVector*) mcxRealloc
                           (  cl->cols
                           ,  n_cols_new * sizeof(mclVector)
                           ,  EXIT_ON_FAIL
                           )

     /* fixme: this code also in mcl/clm.c: factor out? */
      ;  clus = cl->cols + n_cols_new -1
      ;  mclvInit(clus)                            /* make consistent state */
      ;  clus->vid = newvid                        /* give vector identity  */
      ;  mclvRenew(clus, residue->ivps, residue->n_ivps)    /* fill   */
      ;  mclvInsertIdx(cl->dom_cols, newvid, 1.0)  /* update column domain  */
      ;  mcldMerge(cl->dom_rows, residue, cl->dom_rows)  /* add res to dom  */
     /*  NOW N_COLS(cl) has changed (by mclvInsertIdx) */

      ;  mcxTell(me, "Added dummy cluster <%ld> for residue nodes", (long) newvid)
   ;  }

      cl2el  = mclxTranspose(cl)
   ;  mxres = mclxSub(mx, residue, mx->dom_rows)
   ;  clmxres = mclxCompose(cl2el, mxres, 0)

   ;  mcxIOfree(&xfcl)
   ;  mcxIOfree(&xfmx)

   ;  mclxWrite(clmxres, xfResidue, 6, EXIT_ON_FAIL);
      /* fixme: digits option */

     /* now empty cluster and allot all nodes */
   ;  mclvResize(cl->cols+N_COLS(cl)-1, 0)

   ;  for (i=0;i<N_COLS(clmxres);i++)
      {  long vid = clmxres->cols[i].vid, cid
      ;  mclVector* clus
         /* what if projection vector has zero entries [possible]?
          * let's continue then.
         */ 
      ;  if (!clmxres->cols[i].n_ivps)
         continue
      ;  mclvSortDescVal(clmxres->cols+i)
     /* DANGER: safer to sort copy */
      ;  cid = clmxres->cols[i].ivps[0].idx
      ;  clus = mclxGetVector(cl, cid, EXIT_ON_FAIL, NULL)
      ;  mclvInsertIdx(clus, vid, 1.0)
   ;  }
      mclxFree(&clmxres)
   /* clmxres was corrupted in loop above. */

   ;  mclcEnstrict(cl, &o, &m, &e, ENSTRICT_TRULY)
   /* perhaps the extra dummy cluster was totally emptied */

   ;  mclxWrite(cl, xfProjection, MCLXIO_VALUE_NONE, EXIT_ON_FAIL);
   ;  return 0
;  }

