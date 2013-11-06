/* (c) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include "impala/matrix.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/app.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"


const char* usagelines[] =
{  "usage: "
,  "   mcxconvert <fname in> <fname out>     (convert)"
,  "   mcxconvert <fname>                    (report current format)"
,  NULL
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mclMatrix*        mx
   ;  const char*       me = "mcxconvert"
   ;  long n_cols = 0, n_rows = 0

   ;  if (argc == 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--apropos"))
      {  mcxUsage(stdout, me, usagelines)
      ;  mcxExit(0)
   ;  }
      else if (!strcmp(argv[1], "--version"))
      {  app_report_version(me)
      ;  exit(0)
   ;  }
      else if (argc == 2)
      {  mcxIO* xf = mcxIOnew(argv[1], "r")
      ;  const char* fmt
      ;  int format

      ;  mcxIOopen(xf, EXIT_ON_FAIL)
      ;  if (mclxReadDimensions(xf, &n_cols, &n_rows))
         mcxDie(1, me, "reading %s failed", argv[1])

      ;  format = mclxIOformat(xf)
      ;  fmt = format == 'b' ? "binary" : format == 'a' ? "interchange" : "?"
      ;  fprintf
         (  stdout
         ,  "%s format,  row x col dimensions are %ld x %ld\n"
         ,  fmt
         ,  n_rows
         ,  n_cols
         )
   ;  }
      else
      {  mcxIO *xfin = mcxIOnew(argv[1], "r")
      ;  mcxIO *xfout = mcxIOnew(argv[2], "w")
      ;  int format
      ;  mx = mclxRead(xfin, EXIT_ON_FAIL)

      ;  format = mclxIOformat(xfin)
      ;  mcxIOopen(xfout, EXIT_ON_FAIL)
      ;  if (format == 'a')
         mclxbWrite(mx, xfout, EXIT_ON_FAIL)
      ;  else
         mclxaWrite(mx, xfout, MCLXIO_VALUE_GETENV, EXIT_ON_FAIL)
   ;  }
      return 0
;  }


