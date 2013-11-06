/*      Copyright (C) 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO
 * compute when inner loop can be broken; all of xproj has been accounted for.
 *    (sum meetsize)
 * ? adapt or warn on mismatched domains.
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
,  "-o <fname>    write to fname"
,  "-l            write leading legend"
,  NULL
}  ;

const char* me = "clmmate";

int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO* xfx, *xfy, *xftwins = mcxIOnew("-", "w")
   ;  mclx* mx, *my, *meet, *teem, *myt
   ;  int a = 1, x, y
   ;  mcxbool legend = FALSE

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
         else if (!strcmp(argv[a], "-l"))
         legend = TRUE
      ;  else if (!strcmp(argv[a], "-o"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  mcxIOnewName(xftwins, argv[a])
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

      mcxIOopen(xftwins, EXIT_ON_FAIL)

   ;  if (a+2 != argc)
         mcxUsage(stdout, me, usagelines)
      ,  mcxExit(1)

   ;  xfx =  mcxIOnew(argv[a], "r")
   ;  mx  =  mclxRead(xfx, EXIT_ON_FAIL)
   ;  mcxIOclose(xfx)
   ;  xfy =  mcxIOnew(argv[a+1], "r")
   ;  my  =  mclxRead(xfy, EXIT_ON_FAIL)
   ;  myt =  mclxTranspose(my)

   ;  if (!MCLD_EQUAL(mx->dom_rows, my->dom_rows))
      mcxDie(1, me, "domains are not equal")

   ;  meet=  mclxCompose(myt, mx, 0)
   ;  teem=  mclxTranspose(meet)

   ;  if (legend)
      fprintf
      (  xftwins->fp
      ,  "%-10s %6s %6s %6s %6s %6s %6s %6s %6s %6s\n"
      ,  "overlap"
      ,  "x-idx"
      ,  "y-idx"
      ,  "meet"
      ,  "xdiff"
      ,  "ydiff"
      ,  "x-size"
      ,  "y-size"
      ,  "x-proj"
      ,  "y-proj"
      )

   ;  for (x=0;x<N_COLS(meet);x++)
      {  mclv* xvec = meet->cols+x
      ;  long X = xvec->vid
      ;  long xproj = mclvSum(xvec)
      ;  long xsize = mx->cols[x].n_ivps

      ;  for (y=0;y<N_COLS(teem);y++)
         {  mclv* yvec = teem->cols+y
         ;  long Y = yvec->vid
         ;  long yproj = mclvSum(yvec)
         ;  long ysize = my->cols[y].n_ivps
         ;  double twinfac
         ;  long meetsize
         ;  mclp* ivp = mclvGetIvp(yvec, X, NULL)
         ;  if (!ivp)
            continue

         /*
          * meet size, left diff, right diff, right size.
         */

         ;  meetsize = ivp->val

         ;  if (!xsize && !ysize)         /* paranoia */
            continue

         ;  twinfac = 2 * meetsize / ( (double) (xsize + ysize) )

         ;  if (xftwins)
            fprintf
            (  xftwins->fp
            ,  "%-10.3f %6ld %6ld %6ld %6ld %6ld %6ld %6ld %6ld %6ld\n"
            ,  twinfac
            ,  X
            ,  Y
            ,  meetsize
            ,  xsize - meetsize
            ,  ysize - meetsize
            ,  xsize
            ,  ysize
            ,  xproj
            ,  yproj
            )
      ;  }
      }
      return 0
;  }



