/* (c) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#include <string.h>
#include <stdio.h>

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/app.h"
#include "mcl/interpret.h"

#include "util/io.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/types.h"

mclMatrix* cmpProjection
(  mclMatrix* mtx
,  mclMatrix* dom
,  mclMatrix* icl
)  ;

const char* usagelines[] =
{  "Usage: mcxmap [options] -imx <in file>"
,  ""
,  "Options:"
,  "  -cmap <fname>"
,  "     Use map file for column indices"
,  "  -cmul <int p>     [1]"
,  "  -cshift <int q>   [0]"
,  "     Column indices j are mapped to p*j + q"
,  ""
,  "  -rmap <fname>"
,  "     Use map file for row indices"
,  "  -rmul <int p>     [1]"
,  "  -rshift <int q>   [0]"
,  "     Row indices i are mapped to p*i + q"
,  ""
,  "  -map <fname>"
,  "     Use map file for row and column indices"
,  "  -mul <int p>"
,  "  -shift <int q>"
,  "     Use mul and shift values for row and column indices"
,  ""
,  "  --invert          invert the map file"
,  "  --invertr         invert the row map file"
,  "  --invertc         invert the column map file"
,  "  -o <out file>"
,  "  -digits <int d>"
,  NULL
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO             *xfin       =  NULL
   ;  mcxIO             *xfout      =  NULL
   ;  mclMatrix  *mx=NULL
   ;  mclx* cmapx = NULL, *rmapx = NULL
   ;  const char* me          =  "mcxmap"
   ;  long        cshift      =  0
   ;  long        rshift      =  0
   ;  long        cmul        =  1
   ;  long        rmul        =  1
   ;  int         a           =  1
   ;  int digits = MCLXIO_VALUE_GETENV
   ;  mcxstatus   status      =  STATUS_OK
   ;  mcxbool     invert      =  FALSE
   ;  mcxbool     invertr     =  FALSE
   ;  mcxbool     invertc     =  FALSE
   ;  mcxIO* xf_map_c = NULL, *xf_map_r = NULL, *xf_map = NULL

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_NO)

   ;  if (argc == 1)
      goto help

   ;  while (a<argc)
      {  if (!strcmp(argv[a], "-h"))
         {  help
         :  mcxUsage(stdout, me, usagelines)
         ;  return status
      ;  }
         else if (!strcmp(argv[a], "--invert"))
         invert = TRUE
      ;  else if (!strcmp(argv[a], "--invertc"))
         invertc = TRUE
      ;  else if (!strcmp(argv[a], "--invertr"))
         invertr = TRUE
      ;  else if (!strcmp(argv[a], "--version"))
         {  app_report_version(me)
         ;  exit(0)
      ;  }
         else if (!strcmp(argv[a], "-mul"))
         {  if (a++ + 1 < argc)
               cmul =  atol(argv[a])
            ,  rmul = cmul
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-shift"))
         {  if (a++ + 1 < argc)
               cshift =  atoi(argv[a])
            ,  rshift =  cshift
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-cmul"))
         {  if (a++ + 1 < argc)
            cmul =  atol(argv[a])
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-cshift"))
         {  if (a++ + 1 < argc)
            cshift =  atoi(argv[a])
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-cmap"))
         {  if (a++ + 1 < argc)
            xf_map_c =  mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-map"))
         {  if (a++ + 1 < argc)
            xf_map =  mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-rmul"))
         {  if (a++ + 1 < argc)
            rmul =  atol(argv[a])
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-rshift"))
         {  if (a++ + 1 < argc)
            rshift =  atol(argv[a])
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-rmap"))
         {  if (a++ + 1 < argc)
            xf_map_r =  mcxIOnew(argv[a], "r")
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-digits"))
         {  if (a++ + 1 < argc)
            digits =  atol(argv[a])
         ;  else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-imx"))
         {  if (a++ + 1 < argc)
            {  xfin  =  mcxIOnew(argv[a], "r")
            ;  mcxIOopen(xfin, EXIT_ON_FAIL)
         ;  }
            else goto arg_missing
      ;  }
         else if (!strcmp(argv[a], "-o"))
         {  if (a++ + 1 < argc)
            {  xfout  =  mcxIOnew(argv[a], "w")
            ;  mcxIOopen(xfout, EXIT_ON_FAIL)
         ;  }
            else goto arg_missing
      ;  }
         else if (0)
         {  arg_missing
         :  mcxTell(me, "flag <%s> needs argument; see help (-h)", argv[argc-1])
         ;  mcxExit(1)
      ;  }
         else
         {  mcxErr(me, "not an option: <%s>", argv[a])
         ;  return 1
      ;  }
         a++
   ;  }

      if (!xfin)
      {  mcxErr(me, "-imx option is required (see -h for builtin help)")
      ;  return 1
   ;  }

      if (!xfout)
      {  xfout  =  mcxIOnew("out.map", "w")
      ;  mcxIOopen(xfout, EXIT_ON_FAIL)
   ;  }

      mx = mclxRead(xfin, EXIT_ON_FAIL)

   ;  if (xf_map)
      {  mcxIOopen(xf_map, EXIT_ON_FAIL)
      ;  cmapx = mclxRead(xf_map, EXIT_ON_FAIL)  
      ;  rmapx = cmapx
   ;  }
      else
      {  if (xf_map_r)
         {  mcxIOopen(xf_map_r, EXIT_ON_FAIL)
         ;  rmapx = mclxRead(xf_map_r, EXIT_ON_FAIL)  
      ;  }
         else if (rshift || rmul > 1)
         {  rmapx
         =  mclxMakeMap
            (  mclvCopy(NULL, mx->dom_rows)
            ,  mclvMap(NULL, rmul, rshift, mx->dom_rows)
            )
      ;  }
         if (xf_map_c)
         {  mcxIOopen(xf_map_c, EXIT_ON_FAIL)
         ;  cmapx = mclxRead(xf_map_c, EXIT_ON_FAIL)  
      ;  }
         else if (cshift || cmul > 1)
         {  cmapx
         =  mclxMakeMap
            (  mclvCopy(NULL, mx->dom_cols)
            ,  mclvMap(NULL, cmul, cshift, mx->dom_cols)
            )
      ;  }
      }

      if (invert && cmapx && cmapx == rmapx)
      {  mclx* cmapxi = mclxTranspose(cmapx)
      ;  mclxFree(&cmapx)
      ;  cmapx = rmapx = cmapxi
   ;  }
      else
      {  if ((invert || invertr) && rmapx)
         {  mclx* rmapxi = mclxTranspose(rmapx)
         ;  mclxFree(&rmapx)
         ;  rmapx = rmapxi
      ;  }
         if ((invert || invertc) && cmapx)
         {  mclx* cmapxi = mclxTranspose(cmapx)
         ;  mclxFree(&cmapx)
         ;  cmapx = cmapxi
      ;  }
      }

   ;  status = STATUS_FAIL

   ;  do
      {  if (cmapx && mclxMapCols(mx, cmapx))
         break
      ;  if (rmapx && mclxMapRows(mx, rmapx))
         break
      ;  status = STATUS_OK
   ;  }
      while (0)

   ;  if (status)
      {  mcxErr(me, "error, nothing written")
      ;  return 1
   ;  }

      mclxWrite(mx, xfout, digits, EXIT_ON_FAIL)
   ;  return 0
;  }


