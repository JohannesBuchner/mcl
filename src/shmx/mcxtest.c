/*   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#include <string.h>
#include <stdlib.h>

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/ivp.h"
#include "impala/pval.h"
#include "impala/scan.h"
#include "impala/io.h"
#include "impala/app.h"

#include "util/ting.h"
#include "util/err.h"
#include "util/types.h"


const char* usagelines[];

const char* me = "mcxtest";

void usage
(  const char**
)  ;


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mclx* mxv, *mxd
   ;  mclv* vec, *dom
   ;  mcxIO* xfv, *xfd
   ;  mclvScore vscore
   ;  double cov, covmax
   ;  int iv, id

   ;  if (argc != 5)
      mcxDie(1, me, "need 4 args: vecmx, dommx, vec idx, vec idx")

   ;  xfv = mcxIOnew(argv[1], "r")
   ;  xfd = mcxIOnew(argv[2], "r")

   ;  iv = atoi(argv[3]);
   ;  id = atoi(argv[4]);

   ;  mxv = mclxRead(xfv, EXIT_ON_FAIL)
   ;  mxd = mclxRead(xfd, EXIT_ON_FAIL)

   ;  vec = mxv->cols+iv
   ;  dom = mxd->cols+id

   ;  mclvSubScan
      (  vec
      ,  dom
      ,  &vscore
      )

   ;  mclvScoreCoverage
      (  &vscore
      ,  &cov
      ,  &covmax
      )  ;

   ;  fprintf(stdout, "cov    = %f\n", cov)
   ;  fprintf(stdout, "maxcov = %f\n", covmax)
   ;  return 0
;  }


const char* usagelines[] =
{  NULL
}  ;


