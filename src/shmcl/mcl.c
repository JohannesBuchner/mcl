/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/*  **************************************************************************
 * *
 **            Implementation notes (a few).
 *
 *    MCL front-end. Lots of options, many more have come and gone.
 *    Most options are inessential (and you dont see em here anyway).
 *
 *    The design is this:
 *
 * o     the routine mclProcess is provided by the library.
 *    It takes a stochastic matrix as input, and starts clustering using the
 *    supplied inflation parameter(s) and not much more.  Additionally, the
 *    library provides extensive support for different pruning strategies, and
 *    for tracking those (verbosity and progress options).  mclProcess belongs
 *    to the 'lib' or 'library' part.
 *
 * o     mclAlgorithm provides some (not many) handles for
 *    manipulating the input graph, it provides some handles for output and
 *    re-chunking the mcl process (--expand-only and --inflate-first), and it
 *    provides handles (one?) for postprocessing like removing overlap.
 *    mclAlgorithm belongs to the 'main' part.
 *
 *    We do library initialization first, because matrix reading is part of
 *    algorithm initialization. If the two switch order it is painful if errors
 *    are only detected after reading in the matrix (which takes time if it is
 *    huge).
 *
 *    There are some subtleties in command line parsing, as the lib and main
 *    interfaces are unified for the parsing part, but after that each does the
 *    interpretation of the arguments it accepts all by itself. Additionally,
 *    we check whether any info flag appears at the position of the file
 *    argument.
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

#include "impala/ivp.h"
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/io.h"

#include "util/types.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/equate.h"
#include "util/opt.h"

#include "mcl/proc.h"
#include "mcl/procinit.h"
#include "mcl/alg.h"
#include "mcl/clm.h"


int main
(  int               argc
,  char* const       argv[]
)
   {  mclMatrix      *themx            =  NULL

   ;  mclProcParam*  mpp               =  mclProcParamNew()
   ;  char* prog                       =  argc > 0 ? argv[0] : ""
   ;  char* input                      =  argc > 1 ? argv[1] : ""
   ;  mclAlgParam*   map               =  mclAlgParamNew(mpp, prog, input)
   ;  mcxstatus      libStatus         =  STATUS_OK
   ;  mcxstatus      mainStatus        =  STATUS_OK
   ;  mcxstatus      parseStatus       =  STATUS_OK
   ;  int            prefix            =  2

   ;  mcxHash        *procOpts, *algOpts, *mergedOpts
   ;  mcxOption      *opts
   ;  mcxIO          *xfin

   ;  signal(SIGALRM, mclSigCatch)

   ;  mclProcOptionsInit()
   ;  mclAlgOptionsInit()


   ;  procOpts    =  mcxOptHash(mclProcOptions, NULL)
   ;  algOpts     =  mcxOptHash(mclAlgOptions, NULL)
   ;  mergedOpts  =  mcxHashMerge(procOpts, algOpts, NULL, NULL)

   ;  if (argc < 2)
      {  mcxTell
         (  "mcl"
         ,  "usage: mcl <-|file name> [options],"
            " do 'mcl -h' or 'man mcl' for help"
         )
      ;  mcxExit(0)
   ;  }

     /* 
      *  one big fat ugly hack. If there is only one argument, it need not be a
      *  file name, but it can be an info-flag.  This is the only case we
      *  cover. If someone wants to see the effect of '-I 3.0 -z', she needs to
      *  add a dummy file name like '-' or so, e.g. 'mcl -I 3.0 -z' is illegal.
     */
      if (argc >= 2 && *(argv[1]) == '-' && *(argv[1]+1) != '\0')
      {  mcxKV* kv = mcxHashSearch(argv[1], algOpts, MCX_DATUM_FIND)  
      ;  mcxOptAnchor* anch = kv ? (mcxOptAnchor*) kv->val : NULL
      ;  if (anch && (anch->flags & MCX_OPT_INFO))
         prefix = 1
   ;  }

      xfin = mcxIOnew(argv[1], "r")

   ;  opts =  mcxHOptParse
              (mergedOpts, (char**) argv, argc, prefix, 0, &parseStatus)

   ;  if (parseStatus != MCX_OPT_STATUS_OK)
      {  mcxErr("mcl", "error while parsing options")
      ;  mcxTell("mcl", "do 'mcl -h' or 'man mcl'")
      ;  return 1
   ;  }

      libStatus   =  mclProcessInit(opts, procOpts, mpp)

   ;  if (libStatus == STATUS_FAIL)
         mcxErr("mcl", "initialization failed")
      ,  mcxTell("mcl", "do 'mcl -h' or 'man mcl'")
      ,  mcxExit(1)

   ;  mainStatus  =  mclAlgorithmInit(opts, algOpts, argv[1], map)

   ;  mcxOptFree(&opts)
   ;  mcxOptHashFree(&algOpts)
   ;  mcxOptHashFree(&procOpts)
   ;  mcxOptHashFree(&mergedOpts)

   ;  if (mainStatus == ALG_INIT_DONE)
      return 0
   ;  else if (mainStatus == ALG_INIT_FAIL)
         mcxErr("mcl", "initialization failed")
      ,  mcxTell("mcl", "do 'mcl -h' or 'man mcl'")
      ,  mcxExit(1)

   ;  themx = mclxReadx(xfin, EXIT_ON_FAIL, MCL_READX_GRAPH)
   ;  if (!mcldEquate(themx->dom_cols, themx->dom_rows, MCLD_EQ_EQUAL))
         mcxErr("mcl", "domains differ!")
      ,  mcxExit(1)
   ;  if (!N_COLS(themx))
         mcxErr("mcl", "Attempting to cluster the void")
      /* ,  mcxExit(1) */

   ;  mcxIOfree(&xfin)

   ;  mclSetProgress(N_COLS(themx), mpp)

   ;  mainStatus  =  mclAlgorithm(themx, map)
   ;  if (mainStatus == STATUS_FAIL)
         mcxErr("mcl", "failed")
      ,  mcxExit(1)

   ;  mclAlgParamFree(&map)
   ;  return 0
;  }


