/* (c) Copyright 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "impala/io.h"
#include "impala/stream.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/tab.h"
#include "impala/iface.h"
#include "impala/app.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/minmax.h"

const char* me = "mcxdump";
const char* sep_lead_g = "\t";
const char* sep_row_g = "\t";
const char* sep_val_g = ":";

const char* syntax = "Usage: mcxdump -imx <fname> [-dump <fname>] [options]";


enum
{  MY_OPT_IMX
,  MY_OPT_OUTPUT
,  MY_OPT_TAB
,  MY_OPT_TABC
,  MY_OPT_TABR
,  MY_OPT_LAZY_TAB
,  MY_OPT_NO_VALUES
,  MY_OPT_NO_LOOPS
,  MY_OPT_FORCE_LOOPS
,  MY_OPT_TRANSPOSE
,  MY_OPT_DUMP_PAIRS
,  MY_OPT_DUMP_LINES
,  MY_OPT_DUMP_RLINES
,  MY_OPT_DUMP_TABC
,  MY_OPT_DUMP_TABR
,  MY_OPT_STREAM_PACKED
,  MY_OPT_SEP_LEAD
,  MY_OPT_SEP_FIELD
,  MY_OPT_SEP_VAL
,  MY_OPT_HELP
,  MY_OPT_APROPOS
,  MY_OPT_VERSION
}  ;

mcxOptAnchor options[] =
{
   {  "-h"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_HELP
   ,  NULL
   ,  "print this help"
   }
,  {  "--version"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "print version information"
   }
,  {  "--apropos"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "print this help"
   }
,  {  "--lazy-tab"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LAZY_TAB
   ,  NULL
   ,  "tab file(s) may mismatch matrix domain(s)"
   }
,  {  "--stream-packed"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_STREAM_PACKED
   ,  NULL
   ,  "stream matrix in packed format"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "read matrix from file <fname>"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "read tab file from <fname> for all identifiers"
   }
,  {  "-tabc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TABC
   ,  "<fname>"
   ,  "read tab file from <fname> for column domain identifiers"
   }
,  {  "-tabr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TABR
   ,  "<fname>"
   ,  "read tab file from <fname> for row domain identifiers"
   }
,  {  "--no-values"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_NO_VALUES
   ,  NULL
   ,  "do not emit values"
   }
,  {  "--no-loops"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_NO_LOOPS
   ,  NULL
   ,  "do not include self in listing"
   }
,  {  "--force-loops"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_FORCE_LOOPS
   ,  NULL
   ,  "force self in listing"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUT
   ,  "<fname>"
   ,  "output to file <fname> (- for STDOUT)"
   }
,  {  "-dump"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUT
   ,  "<fname>"
   ,  "alias for -o"
   }
,  {  "--dump-tabr"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_TABR
   ,  NULL
   ,  "dump tab file on row domain"
   }
,  {  "--dump-tabc"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_TABC
   ,  NULL
   ,  "dump tab file on column domain"
   }
,  {  "--dump-pairs"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_PAIRS
   ,  NULL
   ,  "dump a single column/row matrix pair per output line"
   }
,  {  "--transpose"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TRANSPOSE
   ,  NULL
   ,  "work with the transposed matrix"
   }
,  {  "--dump-lines"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_LINES
   ,  NULL
   ,  "join all row entries on a single line"
   }
,  {  "--dump-rlines"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_RLINES
   ,  NULL
   ,  "as --dump-lines, do not emit the leading column identifier"
   }
,  {  "-sep-lead"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SEP_LEAD
   ,  "<string>"
   ,  "use <string> to separate col from row list (default tab)"
   }
,  {  "-sep-field"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SEP_FIELD
   ,  "<string>"
   ,  "use <string> to separate row indices (default tab)"
   }
,  {  "-sep-value"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SEP_VAL
   ,  "<string>"
   ,  "use <string> as node/value separator (default colon)"
   }
,  {  NULL, 0, 0, NULL, NULL  }
}  ;

int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO* xf_tab     =  NULL
   ;  mcxIO* xf_tabr    =  NULL
   ;  mcxIO* xf_tabc    =  NULL
   ;  mcxIO* xf_mx      =  NULL
   ;  mcxIO* xfout    =  NULL
   ;  const char*  fndump  =  "-"
   ;  mclTab* tabr      =  NULL
   ;  mclTab* tabc      =  NULL
   ;  mclx* mx          =  NULL
   ;  mcxbool transpose =  FALSE
   ;  mcxbool lazy_tab  =  FALSE
   ;  mcxbool dump_tabc =  FALSE
   ;  mcxbool dump_tabr =  FALSE
   ;  mcxbool stream_packed =  FALSE

   ;  mcxbits modes     =  MCLX_DUMP_VALUES

   ;  mcxbits mode_dump =  MCLX_DUMP_PAIRS
   ;  mcxbits mode_loop =  MCLX_DUMP_LOOP_ASIS

   ;  mclxIOdumper dumper
   ;  mcxOption* opts, *opt
   ;  mcxstatus parseStatus = STATUS_OK
   
   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)
   ;  opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)

   ;  if (!opts)
      exit(0)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 0, 0, options)
         ;  return 0
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  return 0
         ;

            case MY_OPT_TAB
         :  xf_tab = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_TABC
         :  xf_tabc = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_TABR
         :  xf_tabr = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_OUTPUT
         :  fndump = opt->val
         ;  break
         ;

            case MY_OPT_SEP_LEAD
         :  sep_lead_g = opt->val
         ;  break
         ;

            case MY_OPT_SEP_FIELD
         :  sep_row_g = opt->val
         ;  break
         ;

            case MY_OPT_SEP_VAL
         :  sep_val_g = opt->val
         ;  break
         ;

            case MY_OPT_LAZY_TAB
         :  lazy_tab = TRUE
         ;  break
         ;

            case MY_OPT_IMX
         :  xf_mx = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_NO_VALUES
         :  BIT_OFF(modes, MCLX_DUMP_VALUES)
         ;  break
         ;

            case MY_OPT_DUMP_RLINES
         :  mode_dump = MCLX_DUMP_RLINES
         ;  break
         ;

            case MY_OPT_DUMP_LINES
         :  mode_dump = MCLX_DUMP_LINES
         ;  break
         ;

            case MY_OPT_NO_LOOPS
         :  mode_loop = MCLX_DUMP_LOOP_NONE
         ;  break
         ;

            case MY_OPT_FORCE_LOOPS
         :  mode_loop = MCLX_DUMP_LOOP_FORCE
         ;  break
         ;

            case MY_OPT_DUMP_TABC
         :  dump_tabc = TRUE
         ;  break
         ;

            case MY_OPT_STREAM_PACKED
         :  stream_packed = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_TABR
         :  dump_tabr = TRUE
         ;  break
         ;

            case MY_OPT_TRANSPOSE
         :  transpose = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_PAIRS
         :  mode_dump = MCLX_DUMP_PAIRS
         ;  break
      ;  }
      }


   ;  modes |= mode_loop | mode_dump


   ;  if (!xf_mx)
      mcxDie(1, me, "-imx argument required")

   ;  xfout = mcxIOnew(fndump, "w")
   ;  mcxIOopen(xfout, EXIT_ON_FAIL)

   ;  mcxIOopen(xf_mx, EXIT_ON_FAIL)


                        /* fixme: restructure code to include bit below */

   ;  if (xf_tab && (dump_tabc || dump_tabr))
      {  mclv* dom_cols = mclvInit(NULL)
      ;  mclv* dom_rows = mclvInit(NULL)
      ;  mclv* dom = dump_tabc ? dom_cols : dom_rows

      ;  if (!(tabc =  mclTabRead(xf_tab, NULL, RETURN_ON_FAIL)))
         mcxDie(1, me, "error reading tab file")

      ;  if (mclxReadDomains(xf_mx, dom_cols, dom_rows))
         mcxDie(1, me, "error reading matrix file")
      ;  mcxIOclose(xf_mx)

                                       /* fixme check status */
      ;  mclTabWrite(tabc, xfout, dom, RETURN_ON_FAIL) 

      ;  mcxIOclose(xfout)
      ;  return 0
   ;  }


      if (xf_tab && !lazy_tab)
      mx = mclxReadx(xf_mx, EXIT_ON_FAIL, MCL_READX_GRAPH)
   ;  else
      mx = mclxRead(xf_mx, EXIT_ON_FAIL)

   ;  mcxIOfree(&xf_mx)

   ;  if (transpose)
      {  mclx* tp = mclxTranspose(mx)
      ;  mclxFree(&mx)
      ;  mx = tp
   ;  }

      if (stream_packed)
      {  return mclxIOstreamOut(mx, xfout, EXIT_ON_FAIL)
   ;  }

      if (xf_tab)
      {  mcxIOopen(xf_tab, EXIT_ON_FAIL)
      ;  if
         (  !
            (  tabr
            =  mclTabRead
               (xf_tab, lazy_tab ? NULL : mx->dom_rows, RETURN_ON_FAIL)
            )
         )
         mcxDie(1, me, "consider using --lazy-tab option")
      ;  tabc = tabr
      ;  mcxIOfree(&xf_tab)
   ;  }
      else
      {  if (xf_tabr)
         {  mcxIOopen(xf_tabr, EXIT_ON_FAIL)
         ;  if
            (  !  
               (  tabr
               =  mclTabRead
                  (xf_tabr, lazy_tab ? NULL : mx->dom_rows, RETURN_ON_FAIL)
               )
            )
            mcxDie(1, me, "consider using --lazy-tab option")
         ;  mcxIOfree(&xf_tabr)
      ;  }
         if (xf_tabc)
         {  mcxIOopen(xf_tabc, EXIT_ON_FAIL)
         ;  if
            (  !  
               (  tabc
               =  mclTabRead
                  (xf_tabc, lazy_tab ? NULL : mx->dom_cols, RETURN_ON_FAIL)
               )
            )
            mcxDie(1, me, "consider using --lazy-tab option")
         ;  mcxIOfree(&xf_tabc)
      ;  }
      }

      mclxIOdumpSet(&dumper, modes, sep_lead_g, sep_row_g, sep_val_g)

   ;  if (mclxIOdump(mx, xfout, &dumper, tabc, tabr, RETURN_ON_FAIL))
      mcxDie(1, me, "something suboptimal")

   ;  mcxIOfree(&xfout)
   ;  return 0
;  }


