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
#include "impala/scan.h"
#include "impala/io.h"
#include "impala/app.h"
#include "impala/iface.h"
#include "mcl/interpret.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/err.h"
#include "util/types.h"
#include "util/opt.h"
#include "util/minmax.h"


const char* syntax = "Usage: clmclose -imx <matrix> [options]";

enum
{  MY_OPT_MXFILE
,  MY_OPT_DOMAIN
,  MY_OPT_OUTPUTFILE
,  MY_OPT_CC
,  MY_OPT_VERSION
,  MY_OPT_HELP
,  MY_OPT_APROPOS
}  ;

mcxOptAnchor options[] =
{  {  "--version"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "output version information, exit"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUTFILE
   ,  "<fname>"
   ,  "output cluster file"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_MXFILE
   ,  "<fname>"
   ,  "input matrix file, presumably dumped mcl iterand or dag"
   }
,  {  "-dom"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DOMAIN
   ,  "<fname>"
   ,  "input domain file"
   }
,  {  "-cc"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_CC
   ,  NULL
   ,  "retrieve connected components"
   }
,  {  "--apropos"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "this info"
   }
,  {  "-h"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_HELP
   ,  NULL
   ,  "this info"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO *xfmx    =  NULL, *xfdom= NULL, *xfout = NULL
   ;  mclMatrix *dom =  NULL, *dom2 = NULL
   ;  mclMatrix *mx  =  NULL
   ;  const char* me =  "clmclose"

#define MODE_CC 1
   ;  mcxbits mode = 0

   ;  mcxstatus parseStatus = STATUS_OK
   ;  mcxOption* opts, *opt

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)

   ;  if
      (!(opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)))
      exit(0)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 20, MCX_OPT_DISPLAY_SKIP, options)
         ;  return 0
         ;

            case MY_OPT_MXFILE
         :  xfmx = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_OUTPUTFILE
         :  xfout = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  break
         ;

            case MY_OPT_DOMAIN
         :  xfdom= mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_CC
         :  mode |= MODE_CC
         ;  break
         ;

         }
      }

      if (!xfout)
      xfout = mcxIOnew("out.clm", "w")

   ;  mcxOptFree(&opts)

   ;  if (!mode)
      mcxDie(0, me, "suggestion to use mode (-cc)")

   ;  if (!xfmx)
      mcxDie(1, me, "-imx argument required")

   ;  mx = mclxReadx(xfmx, EXIT_ON_FAIL, MCL_READX_GRAPH)
   ;  dom =    xfdom
            ?  mclxRead(xfdom, EXIT_ON_FAIL)
            :  NULL

   /* fixme: adapt domains? */
   ;  if (dom && !mcldEquate(dom->dom_rows, mx->dom_cols, MCLD_EQ_EQUAL))
      mcxDie(1, me, "domains not equal")

   ;  if (mode & MODE_CC)
         dom2 = mclcComponents(mx, dom)
      ,  mclxaWrite(dom2, xfout, MCLXIO_VALUE_NONE, RETURN_ON_FAIL)

   ;  mcxIOfree(&xfmx)
   ;  mcxIOfree(&xfout)

   ;  mclxFree(&mx)
   ;  mclxFree(&dom2)
   ;  mclxFree(&dom)
   ;  return 0
;  }



