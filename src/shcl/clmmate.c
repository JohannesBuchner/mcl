/*      Copyright (C) 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO
 * adapt or warn on mismatched domains.
*/


#include <string.h>
#include <stdio.h>

#include "report.h"

#include "impala/matrix.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/compose.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "taurus/ilist.h"
#include "taurus/la.h"
#include "mcl/clm.h"

#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"


const char* usagelines[] =
{  "clmmate [options] clfile1 clfile2"
,  ""
,  "Options:"
,  "-twins <fname>   for each cluster in clfile1, write best match in clfile2"
,  "                 (creates line based output suitable for sorting)"
,  NULL
}  ;

const char* me = "clmmate";

int main
(  int                  argc
,  const char*          argv[]
)
   {  int digits = 8
   ;  mcxIO* xfx, *xfy, *xftwins = NULL
   ;  mclx* mx, *my, *meet, *teem, *mxt
   ;  int a = 1, x, y
   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_NO)

   ;  if (argc == 1)
      goto help

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
         else if (!strcmp(argv[a], "-twins"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  xftwins = mcxIOnew(argv[a], "w")
         ;  mcxIOopen(xftwins, EXIT_ON_FAIL)
      ;  }
         else if (!strcmp(argv[a], "-digits"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  digits = atoi(argv[a])
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
         else
         break
      ;  a++
   ;  }

      if (a+2 != argc)
         mcxUsage(stdout, me, usagelines)
      ,  mcxExit(1)

   ;  xfx =  mcxIOnew(argv[a], "r")
   ;  mx  =  mclxRead(xfx, EXIT_ON_FAIL)
   ;  mcxIOclose(xfx)
   ;  xfy =  mcxIOnew(argv[a+1], "r")
   ;  my  =  mclxRead(xfy, EXIT_ON_FAIL)
   ;  mxt =  mclxTranspose(mx)

   ;  meet=  mclxCompose(mxt, my, 0)
   ;  teem=  mclxTranspose(meet)

   ;  for (x=0;x<N_COLS(meet);x++)
      {  mclv* xvec = meet->cols+x
      ;  long X = xvec->vid
      ;  long xsize = mclvSum(xvec)

      ;  printf
         (  "cl %ld, %ld entries, file %s; iterating over file %s\n"
         ,  X, xsize, xfy->fn->str, xfx->fn->str
         )
      ;  printf
         (  " %10s: %9s %9s %9s %9s\n"
         ,  "id", "meet", "ldif", "rdif", "rsize"
         )

      ;  for (y=0;y<N_COLS(teem);y++)
         {  mclv* yvec = teem->cols+y
         ;  long Y = yvec->vid
         ;  long ysize = mclvSum(yvec)
         ;  double twinfac
         ;  long meetsize
         ;  mclp* ivp = mclvGetIvp(yvec, X, NULL)
         ;  if (!ivp)
            continue

         /*
          * meet size, left diff, right diff, right size.
         */

         ;  meetsize = ivp->val
         ;  printf
            (  " %10ld: %9ld %9ld %9ld %9ld\n"
            ,  Y
            ,  meetsize
            ,  xsize - meetsize
            ,  ysize - meetsize
            ,  ysize
            )

         ;  if (!xsize && !ysize)
            continue
         ;  twinfac = 2 * meetsize / ( (double) (xsize + ysize) )

         ;  if (xftwins)
            fprintf
            (  xftwins->fp
            ,  "%-10.3f %6ld,%-6ld %6ld %6ld %6ld\n"
            ,  twinfac
            ,  X
            ,  Y
            ,  meetsize
            ,  xsize - meetsize
            ,  ysize - meetsize
            )
      ;  }
      }
      return 0
;  }



