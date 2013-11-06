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
#include "impala/io.h"
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/app.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"


const char* usagelines[] =
{  "Usage: clmmeet [-o <meet file>][--adapt] <cl file> <cl file>*"
,  ""
,  "<meet file>  File where the meet is written (in MCL matrix format)."
,  "  The meet is the largest clustering that is a"
,  "  subclustering of all clusterings specified."
,  "<cl file>    File containing clustering (in MCL matrix format)."
,  "  At least one such file is required."
,  "  Two or more clusterings make it interesting."
,  NULL
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO          **xfMcs        =  NULL
   ;  mcxIO          *xfOut         =  NULL
   ;  mcxTing        *fnout         =  mcxTingNew("out.meet")  
   ;  const char     *dieMsg        =  NULL

   ;  mclMatrix      *lft           =  NULL
   ;  mclMatrix      *rgt           =  NULL
   ;  mclMatrix      *dst           =  NULL

   ;  const char     *me            =  "clmmeet"

   ;  int            o, m, e
   ;  int            a              =  1
   ;  int            n_mx           =  0
   ;  int            j
   ;  int            n_err          =  0
   ;  int            adapt          =  0

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  if (argc == 1)
      goto help

   ;  xfMcs    =  (mcxIO**) mcxAlloc
                  (  (argc)*sizeof(mcxIO*)
                  ,  EXIT_ON_FAIL
                  )
   ;  while(a<argc)
      {  if (!strcmp(argv[a], "-h"))
         {  help
         :  mcxUsage(stdout, me, usagelines)
         ;  mcxExit(0)
      ;  }
         else if (!strcmp(argv[a], "--version"))
         {  app_report_version(me)
         ;  exit(0)
      ;  }
         else if (!strcmp(argv[a], "-o"))
         {  if (a+1 >= argc)
            goto arg_missing
         ;  mcxTingWrite(fnout, argv[++a])
      ;  }
         else if (!strcmp(argv[a], "--adapt"))
         {  adapt = 1
      ;  }
         else if (0)
         {  arg_missing:
            mcxErr
            (  me
            ,  "flag <%s> needs argument (see -h for usage)"
            ,  argv[argc-1]
            )
         ;  mcxExit(1)
      ;  }
         else if (0)
         {  die:
            mcxErr(me, dieMsg)
         ;  mcxExit(1)
      ;  }
         else
         {  break
      ;  }
         a++
   ;  }

      xfOut = mcxIOnew(fnout->str, "w")
   ;  mcxIOopen(xfOut, EXIT_ON_FAIL)

   ;  for(j=a;j<argc;j++)
      {  xfMcs[n_mx] = mcxIOnew(argv[j], "r")
      ;  mcxIOopen(xfMcs[n_mx], EXIT_ON_FAIL)
      ;  n_mx++
   ;  }

      if (!n_mx)
      {  dieMsg = "at least one clustering matrix required (use -h for usage)"
      ;  goto die
   ;  }

  /* I'd preferably do a decent initialization with lft = mclcTop() *before*
   * this loop (removing the need for ugly tmp assignment), but that requires
   * we know the correct domain to pass to it.  For that, we need to peak into
   * the first matrix, but that functionality is not yet there.
  */

      for (j=0;j<n_mx;j++)
      {  mclMatrix* tmp = mclxRead (xfMcs[j], EXIT_ON_FAIL)

      ;  if (mclcEnstrict(tmp, &o, &m, &e, ENSTRICT_TRULY))
         {  report_partition(me, tmp, xfMcs[j]->fn, o, m, e)
         ;  if (adapt)
            report_fixit(me, n_err++)
         ;  else
            report_exit(me, SHCL_ERROR_PARTITION)
      ;  }

         if (!lft)
         {  lft = tmp
         ;  continue
      ;  }
         else
         {  rgt = tmp
      ;  }

         if (!mcldEquate(lft->dom_rows, rgt->dom_rows, MCLD_EQ_EQUAL))
         {  if (adapt)
            {  mclVector* meet = mcldMeet(lft->dom_rows, rgt->dom_rows, NULL)
            ;  mclMatrix* lftn = mclcProject(lft, meet)
            ;  mclMatrix* rgtn = mclcProject(rgt, meet)
            ;  mcxErr
               (  me
               ,  "Domain inequality, ite <%d>,"
                  " file <%s> (projecting domains)"
               ,  (int) j
               ,  xfMcs[j]->fn->str
               )

            ;  report_domain(me, N_ROWS(lft), N_ROWS(rgt), meet->n_ivps)
            ;  mcxTell(me, "Using projections onto the meet of domains")

            ;  report_fixit(me, n_err++)
            ;  mclxFree(&lft)
            ;  mclxFree(&rgt)
            ;  mclvFree(&meet)
            ;  lft = lftn
            ;  rgt = rgtn
         ;  }
            else
            {  report_exit(me, SHCL_ERROR_DOMAIN)
         ;  }
         }

         mcxIOrelease(xfMcs[j])

      ;  dst   =  mclcMeet(lft, rgt, EXIT_ON_FAIL)
      ;  lft   =  dst
      ;  mclxFree(&rgt)
   ;  }

      mclxColumnsRealign(lft, mclvSizeRevCmp)
   ;  mclxWrite(lft, xfOut, MCLXIO_VALUE_NONE, EXIT_ON_FAIL)

   ;  mclxFree(&lft)
   ;  mcxIOfree(&xfOut)
   ;  free(xfMcs)

   ;  return(0)
;  }

