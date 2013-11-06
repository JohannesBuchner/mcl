/*   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#include "impala/io.h"
#include "impala/matrix.h"
#include "impala/tab.h"
#include "impala/stream.h"
#include "impala/ivp.h"
#include "impala/compose.h"
#include "impala/vector.h"
#include "impala/app.h"
#include "impala/iface.h"

#include "util/types.h"
#include "util/ding.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/equate.h"
#include "util/rand.h"
#include "util/opt.h"

#include "mcl/proc.h"
#include "mcl/procinit.h"
#include "mcl/alg.h"

#include "clew/clm.h"
#include "clew/cat.h"

const char* usagelines[];

const char* me = "mcxminusmeet";

void usage
(  const char**
)  ;


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxIO* xf = mcxIOnew(argv[1], "r")
   ;  dim j
   ;  mclxCat st
   ;  mclxCatInit(&st)
   ;  if (mclxCatRead(xf, &st, 10, NULL, NULL, atoi(argv[2])))
      mcxDie(1, "test", "nope")
   ;  for (j=0;j<st.n_level;j++)
      fprintf(stdout, "cols %d rows %d\n", (int) N_COLS(st.level[j].mx), (int) N_ROWS(st.level[j].mx))
   ;  return 0
;  }


const char* usagelines[] =
{  NULL
}  ;


