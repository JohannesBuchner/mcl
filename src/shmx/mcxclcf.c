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

#include "mcx.h"

#include "util/types.h"
#include "util/io.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/compile.h"

#include "impala/io.h"
#include "impala/matrix.h"
#include "impala/tab.h"
#include "impala/stream.h"

#include "gryphon/path.h"


enum
{  MY_OPT_ABC = MCX_DISP_UNUSED
,  MY_OPT_IMX
,  MY_OPT_TAB
,  MY_OPT_TEST_LOOPS
,  MY_OPT_LIST_NODES
}  ;


mcxOptAnchor clcfOptions[] =
{  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "specify input matrix"
   }
,  {  "-abc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ABC
   ,  "<fname>"
   ,  "specify input using label pairs"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "specify tab file to be used with matrix input"
   }
,  {  "--list"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LIST_NODES
   ,  NULL
   ,  "list eccentricity for all nodes"
   }
,  {  "-test-loops"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_TEST_LOOPS
   ,  "1/0"
   ,  "test loop compensation code"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


static  dim progress_g  = 0;
static  mcxbool list_g = FALSE;
static  mcxbool test_loop_g = FALSE;
static  mcxIO* xfmx_g = (void*) -1;
static  mcxIO* xfabc_g = (void*) -1;
static  mcxIO* xftab_g = (void*) -1;
static  mclTab* tab_g = (void*) -1;


static mcxstatus clcfInit
(  void
)
   {  progress_g  =  0
   ;  list_g      =  FALSE
   ;  test_loop_g =  FALSE
   ;  xfmx_g      =  mcxIOnew("-", "r")
   ;  xfabc_g     =  NULL
   ;  xftab_g     =  NULL
   ;  tab_g       =  NULL
   ;  return STATUS_OK
;  }


static mcxstatus clcfArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  mcxIOnewName(xfmx_g, val)
      ;  break
      ;

         case MY_OPT_ABC
      :  xfabc_g = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_TAB
      :  xftab_g = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_LIST_NODES
      :  list_g = TRUE
      ;  break
      ;

         case MY_OPT_TEST_LOOPS
      :  test_loop_g = atoi(val) ? TRUE : FALSE
      ;  break
      ;

         default
      :  mcxExit(1) 
      ;
      }
   ;  return STATUS_OK
;  }


static mcxstatus clcfMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)
   {  mclx* mx
   ;  mclv* has_loops
   ;  mclxIOstreamer streamer = { 0 }

   ;  double clcf = 0.0, ccmax = 0.0
   ;  dim i

   ;  if (xfabc_g)
      {  if (xftab_g)
            tab_g = mclTabRead(xftab_g, NULL, EXIT_ON_FAIL)
         ,  streamer.tab_sym_in = tab_g
      ;  mx
      =  mclxIOstreamIn
         (  xfabc_g
         ,     MCLXIO_STREAM_ABC
            |  MCLXIO_STREAM_MIRROR
            |  MCLXIO_STREAM_SYMMETRIC
            |  (tab_g ? MCLXIO_STREAM_GTAB_RESTRICT : 0)
         ,  NULL
         ,  mclpMergeMax
         ,  &streamer
         ,  EXIT_ON_FAIL
         )
      ;  tab_g = streamer.tab_sym_out
   ;  }
      else
      {  mx = mclxReadx(xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH)
      ;  if (xftab_g)
         tab_g = mclTabRead(xftab_g, mx->dom_cols, EXIT_ON_FAIL)
   ;  }

      has_loops   =  mclxColNums(mx, mclvHasLoop, MCL_VECTOR_SPARSE)
   ;  progress_g  =  mcx_progress_g

   ;  if (!test_loop_g)
      mclxAdjustLoops(mx, mclxLoopCBremove, NULL)

   ;  for (i=0;i<N_COLS(mx);i++)
      {  double cc = mclgCLCF(mx, mx->cols+i, test_loop_g ? has_loops : NULL)
      ;  long vid = mx->cols[i].vid
      ;  clcf += cc
      ;  if (list_g)
         {  if (tab_g)
            {  const char* label = mclTabGet(tab_g, vid, NULL)
            ;  if (!label) mcxDie(1, "clcf", "panic label %ld not found", vid)
            ;  fprintf(stdout, "%s\t%.4f\n", label, cc)
         ;  }
            else
            fprintf(stdout, "%ld\t%.4f\n", vid, cc)
      ;  }
         if (progress_g && cc > ccmax)
            fprintf(stderr, "new max vec %u clcf %.4f\n", (unsigned) i, cc)
         ,  ccmax = cc
      ;  if (progress_g && !((i+1) % progress_g))
         fprintf
         (  stderr
         ,  "%u average %.3f\n"
         ,  (unsigned) (i+1)
         ,  (double) (clcf/(i+1))
         )
   ;  }

      if (N_COLS(mx))
      clcf /= N_COLS(mx)

   ;  if (!list_g)
      fprintf(stdout, "%.3f\n", clcf)

   ;  return 0
;  }


mcxDispHook* mcxDispHookClcf
(  void
)
   {  static mcxDispHook clcfEntry
   =  {  "clcf"
      ,  "clcf [options]"
      ,  clcfOptions
      ,  sizeof(clcfOptions)/sizeof(mcxOptAnchor) - 1

      ,  clcfArgHandle
      ,  clcfInit
      ,  clcfMain

      ,  0
      ,  0
      ,  MCX_DISP_MANUAL
      }
   ;  return &clcfEntry
;  }


