/*   (C) Copyright 2003, 2004, 2005, 2006, 2007, 2008 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* TODO
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

#include "clew/cat.h"

const char* me = "mcxdump";
const char* sep_lead_g = "\t";
const char* sep_row_g = "\t";
const char* sep_val_g = ":";
const char* sep_cat_g = "===";

const char* syntax = "Usage: mcxdump -imx <fname> [-o <fname>] [options]";


enum
{  MY_OPT_IMX
,  MY_OPT_CAT
,  MY_OPT_TREECAT
,  MY_OPT_OUTPUT
,  MY_OPT_TAB
,  MY_OPT_TABC
,  MY_OPT_TABR
,  MY_OPT_LAZY_TAB
,  MY_OPT_NO_VALUES
,  MY_OPT_NO_LOOPS
,  MY_OPT_SORT
,  MY_OPT_FORCE_LOOPS
,  MY_OPT_CAT_LIMIT
,  MY_OPT_SPLIT_STEM
,  MY_OPT_DUMP_MATRIX
,  MY_OPT_TRANSPOSE
,  MY_OPT_DUMP_PAIRS
,  MY_OPT_DUMP_UPPER
,  MY_OPT_DUMP_UPPERI
,  MY_OPT_DUMP_LOWER
,  MY_OPT_DUMP_LOWERI
,  MY_OPT_DUMP_LINES
,  MY_OPT_DUMP_RLINES
,  MY_OPT_DUMP_NEWICK
,  MY_OPT_NEWICK_MODE
,  MY_OPT_DUMP_TABLE
,  MY_OPT_TABLE_NFIELDS
,  MY_OPT_TABLE_NLINES
,  MY_OPT_DUMP_KEYS
,  MY_OPT_DUMP_NOLEAD
,  MY_OPT_DIGITS
,  MY_OPT_WRITE_TABC
,  MY_OPT_WRITE_TABR
,  MY_OPT_WRITE_TABR_SHADOW
,  MY_OPT_DUMP_RDOM
,  MY_OPT_DUMP_CDOM
,  MY_OPT_SKEL
,  MY_OPT_SEP_LEAD
,  MY_OPT_SEP_FIELD
,  MY_OPT_SEP_VAL
,  MY_OPT_SEP_CAT
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
,  {  "-imx-cat"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CAT
   ,  "<fname>"
   ,  "dump multiple matrices encoded in cat file"
   }
,  {  "-imx-tree"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TREECAT
   ,  "<fname>"
   ,  "stackify and dump multiple matrices encoded in cone file"
   }
,  {  "--skeleton"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SKEL
   ,  NULL
   ,  "create empty matrix, honour domains"
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
,  {  "-sort"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SORT
   ,  "size-{ascending,descending}"
   ,  "sort mode"
   }
,  {  "--write-matrix"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_MATRIX
   ,  NULL
   ,  "dump mcl matrix"
   }
,  {  "-split-stem"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SPLIT_STEM
   ,  "<file-name-stem>"
   ,  "split multiple matrices over different files"
   }
,  {  "-cat-max"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CAT_LIMIT
   ,  "<num>"
   ,  "only do the first <num> files"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUT
   ,  "<fname>"
   ,  "output to file <fname> (- for STDOUT)"
   }
,  {  "-digits"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DIGITS
   ,  "<int>"
   ,  "precision to use in interchange format"
   }
,  {  "--write-tabr"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_WRITE_TABR
   ,  NULL
   ,  "write tab file on row domain"
   }
,  {  "--write-tabc"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_WRITE_TABC
   ,  NULL
   ,  "write tab file on column domain"
   }
,  {  "--write-tabr-shadow"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_WRITE_TABR_SHADOW
   ,  NULL
   ,  "write shadow tab file on row domain"
   }
,  {  "--dump-domr"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_RDOM
   ,  NULL
   ,  "dump the row domain"
   }
,  {  "--dump-domc"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_CDOM
   ,  NULL
   ,  "dump the col domain"
   }
,  {  "--dump-pairs"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_PAIRS
   ,  NULL
   ,  "dump a single column/row matrix pair per output line"
   }
,  {  "--dump-upper"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_UPPER
   ,  NULL
   ,  "dump upper part of the matrix excluding diagonal"
   }
,  {  "--dump-upperi"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_UPPERI
   ,  NULL
   ,  "dump upper part of the matrix including diagonal"
   }
,  {  "--dump-lower"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_LOWER
   ,  NULL
   ,  "dump lower part of the matrix excluding diagonal"
   }
,  {  "--dump-loweri"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_LOWERI
   ,  NULL
   ,  "dump lower part of the matrix including diagonal"
   }
,  {  "--newick"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_NEWICK
   ,  NULL
   ,  "write newick string"
   }
,  {  "-newick"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_NEWICK_MODE
   ,  "[NBIS]+"
   ,  "no Number, no Branch length, no Indent, no Singleton parentheses"
   }
,  {  "--dump-table"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_TABLE
   ,  NULL
   ,  "dump complete matrix including zeroes"
   }
,  {  "-table-nlines"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TABLE_NLINES
   ,  "<num>"
   ,  "limit table dump to first <num> lines"
   }
,  {  "-table-nfields"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TABLE_NFIELDS
   ,  "<num>"
   ,  "limit table dump to first <num> fields"
   }
,  {  "--table-keys"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_KEYS
   ,  NULL
   ,  "dump keys too (with --dump-table)"
   }
,  {  "--dump-lead-off"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_NOLEAD
   ,  NULL
   ,  "dump lead node (with --dump-table)"
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
,  {  "-sep-cat"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SEP_CAT
   ,  "<string>"
   ,  "use <string> to separate cat matrix dumps (cf -imx-cat)"
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
   ;  mcxIO* xf_mx      =  mcxIOnew("-", "r")
   ;  mcxIO* xfout    =  NULL
   ;  const char*  fndump  =  "-"
   ;  mclTab* tabr      =  NULL
   ;  mclTab* tabc      =  NULL
   ;  mcxbool transpose =  FALSE
   ;  mcxbool lazy_tab  =  FALSE
   ;  mcxbool write_tabc =  FALSE
   ;  mcxbool write_tabr =  FALSE
   ;  mcxbool cat       =  FALSE
   ;  mcxbool tree      =  FALSE
   ;  mcxbool skel      =  FALSE
   ;  mcxbool newick    =  FALSE
   ;  mcxbits newick_bits = 0
   ;  mcxbits cat_bits  =  0
   ;  dim catmax        =  1
   ;  dim n_max         =  0
   ;  dim table_nlines  =  0
   ;  dim table_nfields =  0
   ;  int split_idx     =  1
   ;  int split_inc     =  1
   ;  const char* split_stem =  NULL
   ;  const char* sort_mode = NULL
   ;  mcxTing* line     =  mcxTingEmpty(NULL, 10)

   ;  mcxbits modes     =  MCLX_DUMP_VALUES

   ;  mcxbits mode_dump =  MCLX_DUMP_PAIRS
   ;  mcxbits mode_part =  0
   ;  mcxbits mode_loop =  MCLX_DUMP_LOOP_ASIS
   ;  mcxbits mode_matrix = 0
   ;  int digits        =  MCLXIO_VALUE_GETENV

   ;  mcxOption* opts, *opt
   ;  mcxstatus parseStatus = STATUS_OK

   ;  mcxLogLevel =
      MCX_LOG_AGGR | MCX_LOG_MODULE | MCX_LOG_IO | MCX_LOG_GAUGE | MCX_LOG_WARN
   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)
   ;  mclx_app_init(stderr)
   
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

            case MY_OPT_SEP_CAT
         :  sep_cat_g = opt->val
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

            case MY_OPT_DUMP_KEYS
         :  BIT_ON(modes, MCLX_DUMP_KEYS)
         ;  break
         ;

            case MY_OPT_NO_VALUES
         :  BIT_OFF(modes, MCLX_DUMP_VALUES)
         ;  break
         ;

            case MY_OPT_DUMP_RLINES
         :  mode_dump = MCLX_DUMP_LINES
         ;  BIT_ON(modes, MCLX_DUMP_NOLEAD)
         ;  break
         ;

            case MY_OPT_DUMP_LINES
         :  mode_dump = MCLX_DUMP_LINES
         ;  break
         ;

            case MY_OPT_SORT
         :  sort_mode = opt->val
         ;  break
         ;

            case MY_OPT_NO_LOOPS
         :  mode_loop = MCLX_DUMP_LOOP_NONE
         ;  break
         ;

            case MY_OPT_CAT_LIMIT
         :  n_max = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_SPLIT_STEM
         :  split_stem = opt->val
         ;  sep_cat_g = NULL
         ;  break
         ;

            case MY_OPT_FORCE_LOOPS
         :  mode_loop = MCLX_DUMP_LOOP_FORCE
         ;  break
         ;

            case MY_OPT_SKEL
         :  skel = TRUE
         ;  break
         ;

            case MY_OPT_WRITE_TABC
         :  write_tabc = TRUE
         ;  break
         ;

            case MY_OPT_DIGITS
         :  digits = strtol(opt->val, NULL, 10)
         ;  break
         ;

            case MY_OPT_WRITE_TABR
         :  write_tabr = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_RDOM
         :  transpose = TRUE
         ;  skel = TRUE
         ;  mode_dump = MCLX_DUMP_LINES
         ;  break
         ;

            case MY_OPT_DUMP_CDOM
         :  skel = TRUE
         ;  mode_dump = MCLX_DUMP_LINES
         ;  break
         ;

            case MY_OPT_IMX
         :  mcxIOnewName(xf_mx, opt->val)
         ;  break
         ;

            case MY_OPT_TREECAT
         :  mcxIOnewName(xf_mx, opt->val)
         ;  tree = TRUE
         ;  cat_bits |= MCLX_PRODUCE_DOMSTACK
         ;  break
         ;

            case MY_OPT_CAT
         :  mcxIOnewName(xf_mx, opt->val)
         ;  cat = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_MATRIX
         :  mode_matrix |= MCLX_DUMP_MATRIX
         ;  break
         ;

            case MY_OPT_TRANSPOSE
         :  transpose = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_UPPER
         :  mode_part = MCLX_DUMP_PART_UPPER
         ;  break
         ;

            case MY_OPT_DUMP_UPPERI
         :  mode_part = MCLX_DUMP_PART_UPPERI
         ;  break
         ;

            case MY_OPT_DUMP_LOWER
         :  mode_part = MCLX_DUMP_PART_LOWER
         ;  break
         ;

            case MY_OPT_DUMP_LOWERI
         :  mode_part = MCLX_DUMP_PART_LOWERI
         ;  break
         ;

            case MY_OPT_DUMP_NOLEAD
         :  BIT_ON(modes, MCLX_DUMP_NOLEAD)
         ;  break
         ;

            case MY_OPT_NEWICK_MODE
         :  if (strchr(opt->val, 'N'))
            newick_bits |= (MCLX_NEWICK_NONL | MCLX_NEWICK_NOINDENT)
         ;  if (strchr(opt->val, 'I'))
            newick_bits |= MCLX_NEWICK_NOINDENT
         ;  if (strchr(opt->val, 'B'))
            newick_bits |= MCLX_NEWICK_NONUM
         ;  if (strchr(opt->val, 'S'))
            newick_bits |= MCLX_NEWICK_NOPTHS
         ;  newick = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_NEWICK
         :  newick = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_TABLE
         :  mode_dump = MCLX_DUMP_TABLE
         ;  break
         ;

            case MY_OPT_TABLE_NFIELDS
         :  table_nfields = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_TABLE_NLINES
         :  table_nlines = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_DUMP_PAIRS
         :  mode_dump = MCLX_DUMP_PAIRS
         ;  break
      ;  }
      }

   ;  if (skel)
      cat_bits |= MCLX_READ_SKELETON

   ;  modes |= mode_loop | mode_dump | mode_part | mode_matrix

   ;  xfout = mcxIOnew(fndump, "w")
   ;  mcxIOopen(xfout, EXIT_ON_FAIL)

   ;  mcxIOopen(xf_mx, EXIT_ON_FAIL)

   ;  if (cat || tree)
      catmax = n_max ? n_max : 0

   ;  if ((write_tabc || write_tabr) && !xf_tab)
      mcxDie(1, me, "need tab file with these options")

   ;  if (xf_tab && mcxIOopen(xf_tab, RETURN_ON_FAIL))
      mcxDie(1, me, "no tab")

   ;  else
      {  if (xf_tabr && mcxIOopen(xf_tabr, RETURN_ON_FAIL))
         mcxDie(1, me, "no tabr")
      ;  if (xf_tabc && mcxIOopen(xf_tabc, RETURN_ON_FAIL))
         mcxDie(1, me, "no tabc")
   ;  }

                        /* fixme: restructure code to include bit below */

      if (write_tabc || write_tabr)
      {  mclv* dom_cols = mclvInit(NULL)
      ;  mclv* dom_rows = mclvInit(NULL)
      ;  mclv* dom = write_tabc ? dom_cols : dom_rows

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

      if (newick)
      {  mcxTing* tree
      ;  mclxCat  cat

      ;  if (xf_tab && !(tabr =  mclTabRead(xf_tab, NULL, RETURN_ON_FAIL)))
         mcxDie(1, me, "error reading tab file")

      ;  if
         (  mclxCatRead
            (  xf_mx
            ,  &cat
            ,  0
            ,  NULL
            ,  tabr ? tabr->domain : NULL
            ,  MCLX_CATREAD_CLUSTERTREE | MCLX_ENSURE_ROOT
            )
         )
         mcxDie(1, me, "failure reading file")
      ;  tree = mclxCatNewick(&cat, tabr, newick_bits)
      ;  fwrite(tree->str, 1, tree->len, xfout->fp)
      ;  fputc('\n', xfout->fp)
      ;  mcxIOclose(xfout)
      ;  return 0
   ;  }

      while (1)
      {  mclxIOdumper dumper
      ;  mclxCat    cat
      ;  dim i

      ;  if (xf_tab && !lazy_tab)
         cat_bits |= MCLX_REQUIRE_GRAPH

      ;  if (mclxCatRead(xf_mx, &cat, catmax, NULL, NULL, cat_bits))
         break

      ;  for (i=0;i<cat.n_level;i++)
         {  mclx* mx = cat.level[i].mx
         ;  if (sort_mode)
            {  if (!strcmp(sort_mode, "size-ascending"))
               mclxColumnsRealign(mx, mclvSizeCmp)
            ;  else if (!strcmp(sort_mode, "size-descending"))
               mclxColumnsRealign(mx, mclvSizeRevCmp)
            ;  else
               mcxErr(me, "unknown sort mode <%s>", sort_mode)
            ;  if (catmax != 1)
               mcxErr(me, "-sort option and cat mode may fail or corrupt")
         ;  }

            if (xf_tab && !tabr)
            {  if (!(  tabr = mclTabRead
                       (xf_tab, lazy_tab ? NULL : mx->dom_rows, RETURN_ON_FAIL)
                  ) )
               mcxDie(1, me, "consider using --lazy-tab option")
            ;  tabc = tabr
            ;  mcxIOclose(xf_tab)
         ;  }
            else
            {  if (!tabr && xf_tabr)
               {  if (!(tabr =  mclTabRead
                        (xf_tabr, lazy_tab ? NULL : mx->dom_rows, RETURN_ON_FAIL)
                     ) )
                  mcxDie(1, me, "consider using --lazy-tab option")
               ;  mcxIOclose(xf_tabr)
            ;  }
               if (!tabc && xf_tabc)
               {  if (!( tabc = mclTabRead
                        (xf_tabc, lazy_tab ? NULL : mx->dom_cols, RETURN_ON_FAIL)
                     ) )
                  mcxDie(1, me, "consider using --lazy-tab option")
               ;  mcxIOclose(xf_tabc)
            ;  }
            }

         ;  if (transpose)
            {  mclx* tp = mclxTranspose(mx)
            ;  mclxFree(&mx)
            ;  mx = tp
            ;  if (tabc || tabr)
               {  mclTab* tabt = tabc
               ;  tabc = tabr
               ;  tabr = tabt
            ;  }
            }

            mclxIOdumpSet(&dumper, modes, sep_lead_g, sep_row_g, sep_val_g)
         ;  dumper.table_nlines  = table_nlines
         ;  dumper.table_nfields = table_nfields

         ;  if (split_stem)
            {  mcxTing* ting = mcxTingPrint(NULL, "%s.%03d", split_stem, split_idx)
            ;  mcxIOclose(xfout)
            ;  mcxIOrenew(xfout, ting->str, "w")
            ;  split_idx += split_inc
         ;  }

            if
            (  mclxIOdump
               (  mx
               ,  xfout
               ,  &dumper
               ,  tabc
               ,  tabr
               ,  digits
               ,  RETURN_ON_FAIL
             ) )
            mcxDie(1, me, "something suboptimal")

         ;  mclxFree(&mx)

         ;  if (sep_cat_g && i+1 < cat.n_level)
            fprintf(xfout->fp, "%s\n", sep_cat_g)
      ;  }
         break
   ;  }

      mcxIOfree(&xf_mx)
   ;  mcxIOfree(&xfout)
   ;  mcxIOfree(&xf_tab)
   ;  mcxIOfree(&xf_tabr)
   ;  mcxIOfree(&xf_tabc)
   ;  mcxTingFree(&line)
   ;  return 0
;  }


