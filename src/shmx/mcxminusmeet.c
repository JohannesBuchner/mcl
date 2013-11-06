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

const char* usagelines[];

const char* me = "mcxminusmeet";

void usage
(  const char**
)  ;


#define  MMM_NEW        1
#define  MMM_RIGHT      2
#define  MMM_CHECK      4
#define  MMM_BINARY     8
#define  MMM_DUMPMX     16
#define  MMM_PROGRESS   32
#define  MMM_NOADD      64

const char* usagelines[] =
{  "mcxminusmeet <modes> <matrix> <low> <high>"
,  "  modes:   1  new target rather than self"
,  "           2  right rather than left"
,  "           4  perform all checks"
,  "           8  use binary"
,  "          16  dump final matrix (should be identical)"
,  "          32  print progress"
,  "          64  refrain from modifying input matrix"
,  NULL
}  ;


void do_mx
(  mclx* mx
,  mcxbits modes
)
   {  dim t, u, n_tst = 0
   ;  mclv* cache   = mclvInit(NULL)
   ;  mclv* meet    = mclvInit(NULL)
   ;  mclv* join    = mclvInit(NULL)
   ;  mclv* diff    = mclvInit(NULL)
   ;  mcxbool self  = !(modes & MMM_NEW)

   ;  for (t=0;t<N_COLS(mx);t++)
      {  mclv* dst

      ;  if (modes & MMM_PROGRESS)
         fputc('.', stderr)

      ;  for (u=1;u<N_COLS(mx);u++)
         {  dim target = self ? (modes & MMM_RIGHT ? u : t) : -1
         ;  if (self)
            mclvCopy(cache, mx->cols+target)

         ;  dst = self ? mx->cols+target : diff

         ;  if (modes & MMM_BINARY)
            mclvBinary(mx->cols+t, mx->cols+u, dst, fltLaNR)
         ;  else
            mcldMinus(mx->cols+t, mx->cols+u, dst)

         ;  if (self)
               mclvCopy(diff, dst)
            ,  mclvCopy(dst, cache)

         ;  dst = self ? mx->cols+target : meet

         ;  if (modes & MMM_BINARY)
            mclvBinary(mx->cols+t, mx->cols+u, dst, fltLaR)
         ;  else
            mcldMeet(mx->cols+t, mx->cols+u, dst)

         ;  if (self)
               mclvCopy(meet, dst)
            ,  mclvCopy(dst, cache)

         ;  mcldMerge(diff, meet, join)

         ;  if (modes & MMM_CHECK)
            {  mclv* dediff = mclvClone(mx->cols+t)
            ;  mclv* demeet = mclvClone(mx->cols+t)
               
            ;  dim nd = mclvUpdateMeet(dediff, diff, fltSubtract)
            ;  dim nm = mclvUpdateMeet(demeet, meet, fltSubtract)

            ;  if
               (  diff->n_ivps + meet->n_ivps != mx->cols[t].n_ivps
               || !mcldEquate(join, mx->cols+t, MCLD_EQT_EQUAL)
               || diff->n_ivps != nd
               || meet->n_ivps != nm
               )
               {  mclvaDump(mx->cols+t, stdout, 0, 0, FALSE)
               ;  mclvaDump(mx->cols+u, stdout, 0, 0, FALSE)
               ;  mclvaDump(meet, stdout, 0, 0, FALSE)
               ;  mclvaDump(diff, stdout, 0, 0, FALSE)
               ;  mcxDie(1, me, "rats")
            ;  }

               mclvFree(&dediff)
            ;  mclvFree(&demeet)
         ;  }

            n_tst++
      ;  }
      }

      if (modes & MMM_PROGRESS)
      fputc('\n', stderr)

   ;  fprintf
      (  stdout
      ,  "%d %s tests in %s%s %s mode\n"
      ,  (int) n_tst
      ,     modes & MMM_CHECK
         ?  "successful"
         :  "unchecked"
      ,  self ? "overwrite" : "create"
      ,  self ? ( modes & MMM_RIGHT ? "-right" : "-left" ) : ""
      ,     modes & MMM_BINARY
         ?  "generic"
         :  "update"
      )
  ;   fprintf
      (  stdout
      ,  "%10lu%10lu%10lu%10lu%10lu%10lu\n"
      ,  (ulong) nu_meet_can
      ,  (ulong) nu_meet_zip
      ,  (ulong) nu_meet_sl
      ,  (ulong) nu_diff_can
      ,  (ulong) nu_diff_zip
      ,  (ulong) nu_diff_sl
      )
;  }


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxIO* xf
   ;  mclx* mx
   ;  mcxbits modes = 0
   ;  dim lo, hi
   ;  dim gap
   ;  dim i

   ;  mcxLogLevel =
      MCX_LOG_AGGR | MCX_LOG_MODULE | MCX_LOG_IO | MCX_LOG_GAUGE | MCX_LOG_WARN
   ;  mclx_app_init(stdout)

   ;  if (argc < 5)
         mcxUsage(stdout, me, usagelines)
      ,  mcxExit(0)

   ;  modes =  atoi(argv[1])
   ;  xf    =  mcxIOnew(argv[2], "r")
   ;  lo    =  atoi(argv[3])
   ;  hi    =  atoi(argv[4])

   ;  mx  = mclxRead(xf, EXIT_ON_FAIL)

   ;  mclgMakeSparse(mx, lo, hi)

   ;  gap = argc > 5 ? atoi(argv[5]) : log(N_COLS(mx))

   ;  if (!(modes & (MMM_DUMPMX | MMM_NOADD)))
      for (i=gap;i<N_COLS(mx);i+=gap)
      mclvCopy(mx->cols+i, mx->dom_rows)

   ;  do_mx(mx, modes)

   ;  if (modes & MMM_DUMPMX)
      {  mcxIO* xo = mcxIOnew("out.mmm", "w")
      ;  mclxWrite(mx, xo, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   ;  }

      return 0
;  }



