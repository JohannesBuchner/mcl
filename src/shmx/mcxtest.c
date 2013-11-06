/*   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#include <string.h>
#include <stdlib.h>
#include <pthread.h>

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
   {  mcxIO* xf = mcxIOnew(argv[1], "r")
   ;  mclx* mx
   ;  mclv* vec1, *vec2

   ;  mcxIOopen(xf, EXIT_ON_FAIL)
   ;  mx = mclxRead(xf, EXIT_ON_FAIL)

   ;  vec1 = mclvCopy(NULL, mx->cols+0)
   ;  vec2 = mclvCopy(NULL, mx->cols+1)

   ;  mclvCopy(vec1, mx->cols+2)
   ;  mclvCopy(vec2, mx->cols+3)

   ;  mcldMinus(vec1, vec2, vec1)
   ;  mcldMinus(vec2, vec2, vec2)
   ;  mcldMinus(vec1, vec2, vec1)
   ;  mcldMinus(vec1, vec1, vec1)

   ;  mclvFree(&vec1)
   ;  mclvFree(&vec2)
   ;  mclxFree(&mx)
   ;  mcxIOfree(&xf)
   ;  return 0
;  }


const char* usagelines[] =
{  NULL
}  ;



