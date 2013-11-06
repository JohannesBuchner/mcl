/* (c) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
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
   {  mclv* base =mclvCanonical(NULL, 20000, 1.0) 
   ;  mclv* small = mclvCanonical(NULL, 200, 1.0/(200))

   ;  int mode = argc > 1 ? atoi(argv[1]) : 0
   ;  int small_delta = 1
   ;  int j

   ;  if (argc == 1)
      {  fprintf(stdout,
"  0  default\n"
"  1  use mclvUpdateMeet, not mclvAdd\n"
"  2  sparsify small over Z6\n"
"  4  sparsify base to even numbers\n"
"  8  set small to size 3000\n"
)     ;  return 0
   ;  }

      if (mode & 8)
      small = mclvCanonical(small, 3000, 1.0/3000)

   ;  if (mode & 2)
      {  small_delta = 4
      ;  for (j=0;j<small->n_ivps;j++)
         small->ivps[j].idx *= 4
   ;  }

      if (mode & 4)
      for (j=0;j<base->n_ivps;j++)
      base->ivps[j].idx *= 2

   ;  while (MCLV_MAXID(small) < MCLV_MAXID(base))
      {  if (mode & 1)
         mclvUpdateMeet(base, small, fltAdd)
      ;  else
         mclvAdd(base, small, base)

      ;  for (j=0;j<small->n_ivps;j++)
         small->ivps[j].idx += small_delta
   ;  }
      for (j=small->n_ivps-10;j<small->n_ivps+9;j++)
      fprintf(stdout, "%ld %.9f\n", (long) base->ivps[j].idx, (double) base->ivps[j].val)

   ;  return 0
;  }


const char* usagelines[] =
{  NULL
}  ;



