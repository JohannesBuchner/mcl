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

const char *me = "clmdag";


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO *xf_cl = NULL, *xf_mx = NULL
   ;  mclx *cl = NULL, *elcl = NULL
   ;  int a = 1, i

   ;  while(a < argc)
      {  if (!strcmp(argv[a], "-icl"))
         {  if (a++ + 1 < argc)
            xf_cl =  mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-h"))
         {  help
         :  mcxUsage(stdout, me, usagelines)
         ;  mcxExit(STATUS_FAIL)
      ;  }
         else if (!strcmp(argv[a], "--version"))
         {  app_report_version(me)
         ;  exit(0)
      ;  }
         else if (!strcmp(argv[a], "-imx"))
         {  if (a++ + 1 < argc)
            xf_mx = mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
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

      if (!xf_cl)
         mcxErr(me, "need cluster file")
      ,  mcxExit(1)

   ;  cl =  mclxRead(xf_cl, EXIT_ON_FAIL)
   ;  elcl = mclxTranspose(cl)

   ;  for (i=0;i<N_COLS(elcl);i++)
      {  mclv* vec = elcl->cols+i
      ;  if (vec->n_ivps > 1)
         fprintf(stdout, "%ld\n", (long) vec->vid)
   ;  }

      return 0
;  }

