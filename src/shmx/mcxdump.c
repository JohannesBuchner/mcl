/*   Copyright (C) 2003, 2004, 2005 Stijn van Dongen
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
const char* sep_g = "\t";
const char* sep_value_g = ":";

const char* syntax = "Usage: mcxdump -imx <fname> [-dump <fname>] [options]";

enum
{  MY_OPT_IMX
,  MY_OPT_OUTPUT
,  MY_OPT_TAB
,  MY_OPT_TABC
,  MY_OPT_TABR
,  MY_OPT_LAZY_TAB
,  MY_OPT_NO_VALUES
,  MY_OPT_TRANSPOSE
,  MY_OPT_DUMP_PAIRS
,  MY_OPT_DUMP_LINES
,  MY_OPT_DUMP_RLINES
,  MY_OPT_SEP_VALUE
,  MY_OPT_SEP
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
,  {  "-sep-value"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SEP_VALUE
   ,  "<string>"
   ,  "use <string> as node/value separator"
   }
,  {  "-sep"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SEP
   ,  "<string>"
   ,  "use <string> as field separator"
   }
,  {  NULL, 0, 0, NULL, NULL  }
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO* xf_tab  =  NULL
   ;  mcxIO* xf_tabr =  NULL
   ;  mcxIO* xf_tabc =  NULL
   ;  mcxIO* xf_mx   =  NULL
   ;  mcxIO* xf_dump =  NULL
   ;  const char*  fndump  =  "-"
   ;  mclTab* tabr   =  NULL
   ;  mclTab* tabc   =  NULL
   ;  mclx* mx       =  NULL
   ;  mcxbool pr_values = TRUE
   ;  mcxbool transpose = FALSE
   ;  mcxbool lazy_tab = FALSE
   ;  mcxmode mode_dump =  'p'      /* pairs, lines, rlines (no col tag) */
   ;  mcxstatus parseStatus = STATUS_OK

   ;  mcxOption* opts, *opt
   
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

            case MY_OPT_SEP
         :  sep_g = opt->val
         ;  break
         ;

            case MY_OPT_SEP_VALUE
         :  sep_value_g = opt->val
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
         :  pr_values = FALSE
         ;  break
         ;

            case MY_OPT_DUMP_RLINES
         :  mode_dump = 'r'
         ;  break
         ;

            case MY_OPT_TRANSPOSE
         :  transpose = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_LINES
         :  mode_dump = 'l'
         ;  break
         ;

            case MY_OPT_DUMP_PAIRS
         :  mode_dump = 'p'
         ;  break
      ;  }
      }

   ;  if (!xf_mx)
      mcxDie(1, me, "-imx argument required")

   ;  xf_dump = mcxIOnew(fndump, "w")
   ;  mcxIOopen(xf_dump, EXIT_ON_FAIL)

   ;  mcxIOopen(xf_mx, EXIT_ON_FAIL)
   ;  mx = mclxRead(xf_mx, EXIT_ON_FAIL)
   ;  if (xf_tab && !mcldEquate(mx->dom_rows, mx->dom_cols, MCLD_EQ_EQUAL))
      mcxDie(1, me, "domains are not identical")
   ;  mcxIOfree(&xf_mx)

   ;  if (transpose)
      {  mclx* tp = mclxTranspose(mx)
      ;  mclxFree(&mx)
      ;  mx = tp
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

      if (mode_dump == 'p')
      {  long i, j, labelc_o = -1
      ;  char* labelc = "", *labelr = ""

      ;  for (i=0;i<N_COLS(mx);i++)
         {  mclv* vec = mx->cols+i
         ;  long labelr_o = -1

         ;  if (tabc)
            labelc = mclTabGet(tabc, vec->vid, &labelc_o)

         ;  for (j=0;j<vec->n_ivps;j++)
            {  if (tabr)
               labelr = mclTabGet(tabr, vec->ivps[j].idx, &labelr_o)

            ;  if (tabc)
               fputs(labelc, xf_dump->fp)
            ;  else
               fprintf(xf_dump->fp, "%ld", (long) vec->vid)

            ;  fputs(sep_g, xf_dump->fp)

            ;  if (tabr)
               fputs(labelr, xf_dump->fp)
            ;  else
               fprintf(xf_dump->fp, "%ld", (long) vec->ivps[j].idx)

            ;  if (pr_values)
               fprintf(xf_dump->fp, "%s%f", sep_g, vec->ivps[j].val)
            ;  fputc('\n', xf_dump->fp)
         ;  }
         }
      }
      else if (mode_dump == 'l' || mode_dump == 'r')
      {  long i, j, labelc_o = -1
      ;  char* labelc = "", *labelr = ""

      ;  for (i=0;i<N_COLS(mx);i++)
         {  mclv* vec = mx->cols+i
         ;  long labelr_o = -1

         ;  if (tabc)
            labelc = mclTabGet(tabc, vec->vid, &labelc_o)

         ;  if (mode_dump == 'l')
            {  if (tabc)
               fputs(labelc, xf_dump->fp)
            ;  else
               fprintf(xf_dump->fp, "%ld", (long) vec->vid)
         ;  }

            for (j=0;j<vec->n_ivps;j++)
            {  const char* sep = mode_dump == 'r' && !j ? "" : sep_g

            ;  if (tabr)
               labelr = mclTabGet(tabr, vec->ivps[j].idx, &labelr_o)

            ;  fputs(sep, xf_dump->fp)

            ;  if (tabr)
               fputs(labelr, xf_dump->fp)
            ;  else
               fprintf(xf_dump->fp, "%ld", (long) vec->ivps[j].idx)

            ;  if (pr_values)
               fprintf(xf_dump->fp, "%s%f", sep_value_g, vec->ivps[j].val)
         ;  }
            if (mode_dump == 'l' || vec->n_ivps)   /* sth was printed */
            fputc('\n', xf_dump->fp)
      ;  }
      }

      mcxIOfree(&xf_dump)
   ;  return 0
;  }


