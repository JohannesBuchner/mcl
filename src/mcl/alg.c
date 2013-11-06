/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "alg.h"
#include "proc.h"
#include "clm.h"
#include "interpret.h"
#include "impala/version.h"
#include "expand.h"
#include "procinit.h"

#include "impala/io.h"
#include "impala/stream.h"

#include "impala/ivp.h"
#include "impala/tab.h"
#include "impala/app.h"
#include "impala/compose.h"
#include "impala/iface.h"

#include "util/ting.h"
#include "util/equate.h"
#include "util/io.h"
#include "util/minmax.h"
#include "util/opt.h"
#include "util/err.h"
#include "util/types.h"
#include "util/tr.h"

#define chb(a,b,c,d,e,f,g) mcxOptCheckBounds("mcl-main", a, b, c, d, e, f, g)

const char* legend
=
   "\n"
   "Legend:\n"
   "  (i)   for informative options that exit after usage\n"
   "  (!)   for important options that you should be aware of\n"
   "Consult the manual page for more information\n"
;

enum
{                          ALG_OPT_OUTPUTFILE = 0
,  ALG_OPT_SHOWVERSION  =  ALG_OPT_OUTPUTFILE + 2
,  ALG_OPT_SHOWHELP
,  ALG_OPT_SHOWSETTINGS
,  ALG_OPT_SHOWCHARTER
,  ALG_OPT_SHOWRAM
,  ALG_OPT_SHOWSCHEMES
,  ALG_OPT_AMOIXA
                        ,  ALG_OPT_APROPOS
,  ALG_OPT_KEEP_OVERLAP =  ALG_OPT_APROPOS + 2
,  ALG_OPT_FORCE_CONNECTED
,  ALG_OPT_CHECK_CONNECTED
,  ALG_OPT_OUTPUT_LIMIT
,  ALG_OPT_APPEND_LOG
,  ALG_OPT_SHOW_LOG
,  ALG_OPT_LINT
,  ALG_OPT_ANALYZE
,  ALG_OPT_SORT
,  ALG_OPT_UNCHECKED
,  ALG_OPT_DIGITS
                        ,  ALG_OPT_BINARY
,  ALG_OPT_ABC          =  ALG_OPT_BINARY + 2
,  ALG_OPT_EXPECT_ABC
,  ALG_OPT_YIELD_ABC
,  ALG_OPT_ABC_TRANSFORM
,  ALG_OPT_ABC_LOGTRANSFORM
,  ALG_OPT_ABC_NEGLOGTRANSFORM
,  ALG_OPT_EXPECT_123
,  ALG_OPT_EXPECT_PACKED
,  ALG_OPT_TAB_EXTEND
,  ALG_OPT_TAB_RESTRICT
,  ALG_OPT_TAB_STRICT
,  ALG_OPT_TAB_USE
,  ALG_OPT_CACHE_MXIN
,  ALG_OPT_CACHE_MXTF
,  ALG_OPT_CACHE_XP
                        ,  ALG_OPT_CACHE_TAB
,  ALG_OPT_AUTOOUT      =  ALG_OPT_CACHE_TAB + 2
,  ALG_OPT_AUTOAPPEND
,  ALG_OPT_AUTOBOUNCENAME
,  ALG_OPT_AUTOBOUNCEFIX
                        ,  ALG_OPT_AUTOPREFIX
,  ALG_OPT_CENTERING    =  ALG_OPT_AUTOPREFIX + 2
,  ALG_OPT_TRANSFORM
,  ALG_OPT_CTRMAXAVG
                        ,  ALG_OPT_DIAGADD
,  ALG_OPT_PREPRUNE     =  ALG_OPT_DIAGADD + 2
,  ALG_OPT_PREINFLATION
}  ;


typedef struct grade
{  int   mark
;  const char* ind
;
}  grade ;

grade gradeDir[] =
{  {  10000, "impossible" }
,  { 100, "yabadabadoo!" }
,  {  98, "perfect" }
,  {  97, "marvelous" }
,  {  96, "superb" }
,  {  95, "sensational" }
,  {  93, "fabulous" }
,  {  92, "ripping" }
,  {  91, "smashing baby!" }
,  {  89, "cracking" }
,  {  88, "top-notch" }
,  {  87, "excellent" }
,  {  86, "splendid" }
,  {  85, "outstanding" }
,  {  84, "really neat" }
,  {  82, "dandy" }
,  {  80, "favourable" }
,  {  79, "not bad at all" }
,  {  78, "groovy" }
,  {  77, "good" }
,  {  76, "all-right" }
,  {  74, "solid" }
,  {  72, "decent" }
,  {  70, "adequate" }
,  {  66, "fair" }
,  {  63, "fairish" }
,  {  60, "satisfactory" }
,  {  55, "acceptable" }
,  {  53, "tolerable" }
,  {  50, "mediocre" }
,  {  49, "so-so" }
,  {  48, "shabby" }
,  {  46, "shoddy" }
,  {  44, "dodgy", }
,  {  43, "poor" }
,  {  40, "bad" }
,  {  35, "deplorable" }
,  {  32, "rotten" }
,  {  30, "lousy" }
,  {  26, "abominable" }
,  {  24, "miserable" }
,  {  23, "dismal" }
,  {  22, "appalling" }
,  {  20, "awful" }
,  {  18, "pathetic" }
,  {  16, "terrible" }
,  {  14, "dreadful" }
,  {  12, "wretched" }
,  {  10, "horrid" }
,  {   8, "ghastly" }
,  {   6, "atrocious" }
,  {  -2, "impossible" }
}  ;


void juryCharter
(  mclProcParam* mpp
)  ;

void  howMuchRam
(  long i
,  mclProcParam* mpp
)  ;

void mclWriteLog
(  FILE* fp
,  mclAlgParam* mlp
,  mclMatrix* cl
)  ;

void mclAlgPrintInfo
(  FILE* fp
,  mclAlgParam* mlp  
,  mclMatrix* cl
)  ;

void mclCenter
(  mclMatrix*        mx
,  double            w_self
,  double            w_maxval
)  ;




/*  options with 'flags & MCX_OPT_INFO' exit after displaying info         */
/*  options with 'flags & MCX_OPT_HASARG' take an argument (who would've guessed) */


mcxOptAnchor mclAlgOptions[] =
{
   {  "-force-connected"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_FORCE_CONNECTED
   ,  "y/n"
   ,  "analyze clustering, make sure it induces cocos"
   }
,  {  "-output-limit"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_OUTPUT_LIMIT
   ,  "y/n"
   ,  "output the limit matrix"
   }
,  {  "-check-connected"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CHECK_CONNECTED
   ,  "y/n"
   ,  "analyze clustering, see whether it induces cocos"
   }
,  {  "-analyze"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_ANALYZE
   ,  "y/n"
   ,  "append performance/characteristics measures"
   }
,  {  "-show-log"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_SHOW_LOG
   ,  "y/n"
   ,  "send log to stdout"
   }
,  {  "--lint"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  ALG_OPT_LINT
   ,  NULL
   ,  "over(t)ly analytical"
   }
,  {  "-append-log"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_APPEND_LOG
   ,  "y/n"
   ,  "append log to clustering"
   }
,  {  "-keep-overlap"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_KEEP_OVERLAP
   ,  "y/n"
   ,  "keep overlap in clustering, should it occur"
   }
,  {  "-sort"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_SORT
   ,  "<mode>"
   ,  "order clustering by one of lex|size|revsize|none"
   }
,  {  "--unchecked"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_UNCHECKED
   ,  NULL
   ,  "do not check input matrix validity(danger sign here)"
   }
,  {  "--abc"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_ABC
   ,  NULL
   ,  "both of --expect-abc and --yield-abc"
   }
,  {  "--expect-abc"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_EXPECT_ABC
   ,  NULL
   ,  "expect <label1> <label2> [value] format"
   }
,  {  "--yield-abc"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_YIELD_ABC
   ,  NULL
   ,  "write clusters as tab-separated labels"
   }
,  {  "--abc-log"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_ABC_LOGTRANSFORM
   ,  NULL
   ,  "log-transform label value"
   }
,  {  "--abc-neg-log"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_ABC_NEGLOGTRANSFORM
   ,  NULL
   ,  "minus log-transform label value"
   }
,  {  "-abc-tf"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_ABC_TRANSFORM
   ,  "tf-spec"
   ,  "transform label values"
   }
,  {  "-tf"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_TRANSFORM
   ,  "tf-spec"
   ,  "transform matrix values"
   }
,  {  "--expect-123"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  ALG_OPT_EXPECT_123
   ,  NULL
   ,  "expect <num1> <num2> [value] format"
   }
,  {  "--expect-packed"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  ALG_OPT_EXPECT_PACKED
   ,  NULL
   ,  "expect packed format"
   }
,  {  "-cache-tab"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CACHE_TAB
   ,  "fname"
   ,  "write tab to file fname"
   }
,  {  "-cache-graph"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CACHE_MXIN
   ,  "fname"
   ,  "write input matrix to file"
   }
,  {  "-cache-graphx"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CACHE_MXTF
   ,  "fname"
   ,  "write transformed matrix to file"
   }
,  {  "-cache-expanded"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CACHE_XP
   ,  "<fname>"
   ,  "file name to write expanded graph to"
   }
,  {  "-use-tab"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_TAB_USE
   ,  "fname"
   ,  "tab file"
   }
,  {  "-strict-tab"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_TAB_STRICT
   ,  "fname"
   ,  "tab file (universe)"
   }
,  {  "-restrict-tab"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_TAB_RESTRICT
   ,  "fname"
   ,  "tab file (world)"
   }
,  {  "-extend-tab"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_TAB_EXTEND
   ,  "fname"
   ,  "tab file (extendable)"
   }
,  {  "--binary"
   ,  MCX_OPT_DEFAULT
   ,  ALG_OPT_BINARY
   ,  NULL
   ,  "write binary output"
   }
,  {  "-digits"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_DIGITS
   ,  "<int>"
   ,  "precision in interchange (intermediate matrices) output"
   }
,  {  "--show-settings"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_SHOWSETTINGS
   ,  NULL
   ,  "\tMi\tDshow some of the default settings"
   }
,  {  "--jury-charter"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_SHOWCHARTER
   ,  NULL
   ,  "\tMi\tDshow the meaning of the jury pruning synopsis"
   }
,  {  "-z"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_SHOWSETTINGS
   ,  NULL
   ,  "\tMi\tDshow some of the default settings (--show-settings alias)"
   }
,  {  "--version"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_SHOWVERSION
   ,  NULL
   ,  "\tMi\tDshow version"
   }
,  {  "--amoixa"
   ,  MCX_OPT_INFO | MCX_OPT_HIDDEN
   ,  ALG_OPT_AMOIXA
   ,  NULL
   ,  ">o<"
   }
,  {  "--apropos"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_APROPOS
   ,  NULL
   ,  "\tMi\tDshow summaries of all options"
   }
,  {  "--k"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_APROPOS
   ,  NULL
   ,  "alias for --apropos"
   }
,  {  "-h"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_SHOWHELP
   ,  NULL
   ,  "\tMi\tDoutput description of most important options"
   }
,  {  "--show-schemes"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_SHOWSCHEMES
   ,  NULL
   ,  "\tMi\tDshow the preset -scheme options"
   }
,  {  "-how-much-ram"
   ,  MCX_OPT_INFO | MCX_OPT_HASARG
   ,  ALG_OPT_SHOWRAM
   ,  "<int>"
   ,  "\tMi\tDshow estimated RAM usage for graphs with <int> nodes"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_OUTPUTFILE
   ,  "<fname>"
   ,  "\tM!\tDwrite output to file <fname>"
   }
,  {  "-aa"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_AUTOAPPEND
   ,  "<suffix>"
   ,  "append <suffix> to mcl-created output file name"
   }
,  {  "-az"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_AUTOBOUNCENAME
   ,  NULL
   ,  "\tMi\tDshow output file name mcl would construct"
   }
,  {  "-ax"
   ,  MCX_OPT_INFO
   ,  ALG_OPT_AUTOBOUNCEFIX
   ,  NULL
   ,  "\tMi\tDshow the suffix mcl constructs from parameters"
   }
,  {  "-ap"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_AUTOPREFIX
   ,  "<prefix>"
   ,  "prepend <prefix> to mcl-created output file name"
   }
,  {  "-c"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CENTERING
   ,  "<num>"
   ,  "<num> in range 1.0-5.0, 0.5-10.0, 0.2-100.0"
   }
,  {  "-cma"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_CTRMAXAVG
   ,  "<pct>"
   ,  "shift loop assignment towards center rather than max"
   }
,  {  "-a"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_DIAGADD
   ,  "<num>"
   ,  "(deprecated) replace diagonal with <num> * I"
   }
,  {  "-pp"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_PREPRUNE
   ,  "<int>"
   ,  "prune input matrix to at most <int> entries per column"
   }
,  {  "-pi"
   ,  MCX_OPT_HASARG
   ,  ALG_OPT_PREINFLATION
   ,  "<num>"
   ,  "preprocess by applying inflation with parameter <num>"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


void showSettings
(  mclAlgParam* mlp
)  ;


void showSetting
(  void
)
   {  fprintf
      (  stdout
      ,
"mcl %s, %s\n"
"Copyright (c) 1999-2005, Stijn van Dongen. mcl comes with NO WARRANTY\n"
"to the extent permitted by law. You may redistribute copies of mcl under\n"
"the terms of the GNU General Public License.\n"
      ,  mclNumTag
      ,  mclDateTag
      )
;  }


const char* mclHelp[]
=
{  "________ mcl verbosity modes"
,  "--show ....... print MCL iterands (small graphs only)"
,  "-v all ....... turn on all -v options"
,  "-progress <i>  if i>0, (try to) print i times `.' during one iteration  [30]"
,  "                 if i<0, print `.' after every |i| cols computed"
,  "                 if i=0, print convergence measure after every iteration"
,  ""
,  "________ on multi-processor systems"
,  "-te <i> ...... number of threads to use for expansion                    [0]"
,  ""
,  "________ mcl info options"
,  "-z ........... shows current settings (takes other arguments into account)"
,  "--show-schemes shows the resource schemes accessible via the -scheme option"
,  "--version .... shows version and license information"
,  ""
,  "________ speed/quality controls"
,  "-P <i> ....... set pruning number to i                            [scheme 2]"
,  "-S <i> ....... set selection number to i                          [scheme 2]"
,  "-R <i> ....... set recover number to i                            [scheme 2]"
,  "-pct <i> ..... mass percentage below which to apply recovery      [scheme 2]"
,  "-scheme <i> .. use a preset scheme for -P/-S/-R/-pct, (i=1,2,3,4,5)      [2]"
,  "                 --show-schemes shows the preset schemes"
,  "                 higher schemes are costlier and possibly more accurate"
,  ""
,  "________ mcl cluster controls"
,  "-I <f> ....... inflation (varying this parameter affects granularity)  [2.0]"
,  "-o <fname> ... direct cluster output to file fname (- for stdout)  [out.mcl]"
,  "                 if omitted, mcl will create a meaningful output name -"
,  "                 try it out, it's convenient. (cf. -az)."
,  ""
,  "This is a selection of the mcl options and their defaults. Try different"
,  "-I values for finding clusterings of different granularity, (e.g. in the"
,  "range 1.2 - 4.0). See 'man mcl' and 'man mclfaq' for more information."
,  "----> mcl <graph.mci> -I 2.0 <---- should get you going."
,  "Usually you should only need the -I option - for large graphs the -scheme"
,  "option as well. mcl --apropos shows a summary listing of all options."
,  ""
,  NULL
}  ;


/*
 * mlp->xfout is set up for us
*/

mclMatrix* postprocess
(  mclAlgParam* mlp
,  mclMatrix* cl
)
   {  double pvals[5]  
   ;  double gvals[8]  
   ;  mclx*  mx = NULL
   ;  const char* me = "__mclAfter__"
   ;  mcxbool reread
      =     mlp->modes
         & (ALG_DO_CHECK_CONNECTED | ALG_DO_FORCE_CONNECTED | ALG_DO_ANALYZE)

                              /* TODO if cached, use cache */
   ;  if (reread)       /* fixme problematic with stream input */
      {  mcxTell("mcl", "re-reading matrix for performance measures")
      ;  mx = mclAlgorithmRead(mlp, reread)     /* could fail! */
   ;  }

      if (mx && (mlp->modes & (ALG_DO_CHECK_CONNECTED | ALG_DO_FORCE_CONNECTED)))
      {  mclMatrix* cm  =  mclcComponents(mx, cl)
      ;  mcxTing* fn2
      ;  mcxIO*   xf2
      ;  char*    pf2

      ;  if (N_COLS(cl) != N_COLS(cm))
         {  mcxTell
            (  me
            ,  "splitting yields an additional %ld clusters at a total of %ld"
            ,  (long) (N_COLS(cm) - N_COLS(cl))
            ,  (long) N_COLS(cm)
            )
         ;  pf2 = (mlp->modes & ALG_DO_FORCE_CONNECTED) ? "orig" : "coco"
         ;  fn2 = mcxTingPrint(NULL, "%s-%s", mlp->xfout->fn->str, pf2)
         ;  xf2 = mcxIOnew(fn2->str, "w")

         ;  if (mlp->modes & ALG_DO_FORCE_CONNECTED)
            {  mclxaWrite(cl, xf2, MCLXIO_VALUE_NONE, RETURN_ON_FAIL)
            ;  mclxFree(&cl)
            ;  cl = cm
            ;  mcxTell(me, "proceeding with split clustering")
         ;  }
            else
            {  mclxaWrite(cm, xf2, MCLXIO_VALUE_NONE, RETURN_ON_FAIL)
            ;  mclxFree(&cm)
            ;  mcxTell(me, "proceeding with original clustering")
         ;  }
         }
         else
            mcxTell
            (  me
            ,  "clustering induces connected components"
            )
         ,  mclxFree(&cm)
   ;  }

      /* write clustering only now */

      if (MCPVB(mlp->mpp, MCPVB_CAT))
      {  mclDumpMatrix(cl, mlp->mpp, "result", "", 0, FALSE)
      ;  mcxTell("mcl", "output is in %s", mlp->mpp->dump_stem->str)
      ;  return cl
   ;  }
      else if (mlp->stream_write_labels)
      {  mclxIOdumper dumper
      ;  if (mcxIOopen(mlp->xfout, RETURN_ON_FAIL) != STATUS_OK)
         {  mcxWarn(me, "cannot open out stream <%s>", mlp->xfout->fn->str)
         ;  mcxWarn(me, "trying to fall back to default <out.mcl>")
         ;  mcxIOnewName(mlp->xfout, "out.mcl")
         ;  mcxIOopen(mlp->xfout, EXIT_ON_FAIL)
      ;  }
         mclxIOdumpSet(&dumper, MCLX_DUMP_RLINES, NULL, NULL, NULL)
      ;  mclxIOdump(cl, mlp->xfout, &dumper, NULL, mlp->tab, RETURN_ON_FAIL)
      ;  mcxTell("mcl", "output is in %s", mlp->xfout->fn->str)
   ;  }
      else
      {  if (mcxIOopen(mlp->xfout, RETURN_ON_FAIL) != STATUS_OK)
         {  mcxWarn(me, "cannot open out stream <%s>", mlp->xfout->fn->str)
         ;  mcxWarn(me, "trying to fall back to default <out.mcl>")
         ;  mcxIOnewName(mlp->xfout, "out.mcl")
         ;  mcxIOopen(mlp->xfout, EXIT_ON_FAIL)
      ;  }
         mclxaWrite(cl, mlp->xfout, MCLXIO_VALUE_NONE, EXIT_ON_FAIL)
   ;  }

      if (mlp->modes & ALG_DO_APPEND_LOG)
      mclWriteLog(mlp->xfout->fp, mlp, cl)

   ;  mcxIOclose(mlp->xfout)

      /* clustering written, now check matrix */

   ;  if (reread && !mx)
      mcxErr(me, "cannot re-read matrix")
   ;  else if (mlp->modes & ALG_DO_ANALYZE)
      {  mcxTing* note = mcxTingEmpty(NULL, 60)
      ;  mcxIOrenew(mlp->xfout, NULL, "a")

      ;  if (mcxIOopen(mlp->xfout, RETURN_ON_FAIL))
         {  mcxWarn(me, "cannot append to file %s", mlp->xfout->fn->str)
         ;  return cl
      ;  }

         if (mlp->is_transformed)
         mcxTingPrintAfter(note, "after-transform\n")

      ;  clmGranularity(cl, gvals)
      ;  clmGranularityPrint (mlp->xfout->fp, note->str, gvals, 1)

      ;  clmPerformance (mx, cl, pvals)
      ;  mcxTingPrint
         (  note
         ,  "target-name=%s\nsource-name=%s\n%s"
         ,  mlp->fnin->str
         ,  mlp->xfout->fn->str
         ,  mlp->is_transformed ? "after-transform\n" : ""
         )
      ;  clmPerformancePrint (mlp->xfout->fp, note->str, pvals, 1)

      ;  if (!mlp->is_transformed && mclAlgorithmTransform(mx, mlp))
         {  mcxTingPrintAfter(note, "after-transform\n")
         ;  clmPerformance (mx, cl, pvals)
         ;  clmPerformancePrint (mlp->xfout->fp, note->str, pvals, 1)
      ;  }

         mcxTell(me, "included performance measures in cluster output")
      ;  mclxFree(&mx)  
      ;  mcxTingFree(&note)
   ;  }

      mcxTell("mcl", "output is in %s", mlp->xfout->fn->str)
   ;  return cl
;  }


mcxstatus mclAlgorithm
(  mclMatrix*  themx
,  mclAlgParam*   mlp
)
   {  mclMatrix      *thecluster       =  NULL
   ;  mclProcParam*  mpp               =  mlp->mpp
   ;  const char*    me                =  "mcl"
   ;  int   o, m, e

   ;  mcxTell(me, "pid %ld\n", (long) getpid())
   ;  thecluster = mclProcess(&themx, mpp)

   ;  if (mlp->modes & ALG_DO_OUTPUT_LIMIT)
      {  mcxTing* fnlm  =  mcxTingPrint(NULL, "%s-%s", mlp->xfout->fn->str, "limit")
      ;  mcxIO*   xflm  =  mcxIOnew(fnlm->str, "w")
      ;  mclxWrite(themx, xflm, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   ;  }

      mclxFree(&themx)  /* this is the last iterand or the one before */

   ;  if (mlp->modes & ALG_DO_SHOW_LOG)
      fprintf(stdout, mpp->massLog->str)

   ;  mclcEnstrict
      (  thecluster
      ,  &o
      ,  &m
      ,  &e
      ,  mlp->modes & ALG_DO_KEEP_OVERLAP ? ENSTRICT_KEEP_OVERLAP : 0
      )

   ;  if (o>0)
      {  if (mlp->modes & ALG_DO_KEEP_OVERLAP)
         mcxWarn("__mclAfter__", "found <%ld> instances of overlap", (long) o)

      ;  else if (!(mlp->modes & ALG_DO_KEEP_OVERLAP))
         mcxWarn("__mclAfter__", "removed <%ld> instances of overlap", (long) o)

      ;  mlp->foundOverlap = TRUE
   ;  }

      if (m>0)
      mcxWarn(me, "added <%ld> garbage entries", (long) m)

   ;  if (N_COLS(thecluster) > 1)
      {  if (mlp->sortMode == 's')
         mclxColumnsRealign(thecluster, mclvSizeCmp)
      ;  else if (mlp->sortMode == 'S')
         mclxColumnsRealign(thecluster, mclvSizeRevCmp)
      ;  else if (mlp->sortMode == 'l')
         mclxColumnsRealign(thecluster, mclvLexCmp)
   ;  }

     /* EO cluster enstriction
      */

      mcxTell
      (  me
      ,  "jury pruning marks (worst %d cases): <%d,%d,%d>, out of 100"
      ,  (int) mclDefaultWindowSizes[mpp->mxp->nj]
      ,  (int) mpp->marks[0]
      ,  (int) mpp->marks[1]
      ,  (int) mpp->marks[2]
      )
   ;  {  int i = 0
      ;  double f = (5*mpp->marks[0] + 2*mpp->marks[1] + mpp->marks[2]) / 8.0
      ;  if (f<0.0)
         f = 0.0
      ;  while (gradeDir[i].mark > f+0.001)
         i++
      ;  mcxTell
         (  me
         ,  "jury pruning synopsis: <%.1f or %s> (cf -scheme, -do log)"
         ,  f
         ,  gradeDir[i].ind
         )
   ;  }

      thecluster = postprocess(mlp, thecluster)

   ;  mcxTell(me, "%ld clusters found", (long) N_COLS(thecluster))
   ;  mclxFree(&thecluster)

   ;  return STATUS_OK
;  }


mcxbool set_bit
(  mclAlgParam*   mlp
,  const char*    opt
,  int            anch_id
,  const char*    clue
)
   {  mcxbool on = FALSE
   ;  long bit = 0
   
   ;  if (!clue || strchr("1yY", clue[0]))
      on = TRUE
   ;  else if (strchr("0nN", (unsigned char) clue[0]))
      on = FALSE
   ;  else
      {  mcxErr("mcl-lib", "option %s expects 1/0/Yes/yes/No/no value", opt)
      ;  return FALSE
   ;  }

      switch(anch_id)
      {  case  ALG_OPT_APPEND_LOG      : bit = ALG_DO_APPEND_LOG     ;  break
      ;  case  ALG_OPT_SHOW_LOG        : bit = ALG_DO_SHOW_LOG       ;  break
      ;  case  ALG_OPT_ANALYZE         : bit = ALG_DO_ANALYZE        ;  break
      ;  case  ALG_OPT_KEEP_OVERLAP    : bit = ALG_DO_KEEP_OVERLAP   ;  break
      ;  case  ALG_OPT_FORCE_CONNECTED : bit = ALG_DO_FORCE_CONNECTED;  break
      ;  case  ALG_OPT_CHECK_CONNECTED : bit = ALG_DO_CHECK_CONNECTED;  break
      ;  case  ALG_OPT_OUTPUT_LIMIT    : bit = ALG_DO_OUTPUT_LIMIT   ;  break

      ;  case  ALG_OPT_LINT
         :  bit =    ALG_DO_APPEND_LOG
                  |  ALG_DO_KEEP_OVERLAP
                  |  ALG_DO_ANALYZE
                  |  ALG_DO_CHECK_CONNECTED
         ;  break
   ;  }

      mlp->modes |= bit
   ;  if (!on)
      mlp->modes ^= bit
   ;  return TRUE
;  }


void make_output_name
(  mclAlgParam* mlp
,  mcxTing* suf
,  const char* mkappend
,  const char* mkprefix
)
   {  mcxTing* name = mcxTingEmpty(NULL, 40)
   ;  mclProcParam* mpp = mlp->mpp
   ;  int s = mpp->mxp->scheme

   ;  mcxTingPrintAfter(suf, "I%.1f", (double) mpp->mainInflation)

   ;  if (s)
      mcxTingPrintAfter(suf, "s%d", s)
   ;  else if (mpp->mxp->my_scheme >= 0)
      mcxTingPrintAfter(suf, "s%d", mpp->mxp->my_scheme)
   ;  else
      mcxTingPrintAfter
      (  suf
      ,  "P%ldQ%2.0fR%ldS%ld"
      ,  (long) mpp->mxp->num_select
      ,  (double) (100 * mpp->mxp->pct)
      ,  (long) mpp->mxp->num_recover
      ,  (long) mpp->mxp->num_select
      )

   ;  if (mpp->initLoopLength)
         mcxTingPrintAfter(suf, "l%d", (int) mpp->initLoopLength)
      ,  mcxTingPrintAfter(suf, "i%.1f", (double) mpp->initInflation)
   ;  if (mlp->pre_center != 1.0)
      mcxTingPrintAfter(suf, "c%.1f", (double) mlp->pre_center)
   ;  if (mlp->pre_inflation > 0.0)
      mcxTingPrintAfter(suf, "pi%.1f", (double) mlp->pre_inflation)
   ;  if (mlp->pre_maxnum)
      mcxTingPrintAfter(suf, "pp%d", (int) mlp->pre_maxnum)

   ;  mcxTingTr(suf, NULL, NULL, ".", "", 0)

   ;  if (mkappend)
      mcxTingPrintAfter(suf, "%s", mkappend)

   ;  if (mkprefix)
      {  char* ph /* placeholder */
      ;  if ((ph = strchr(mkprefix, '=')))
         {  mcxTingPrint(name, "%.*s", ph-mkprefix, mkprefix)
         ;  mcxTingPrintAfter(name, "%s", mlp->fnin->str)
         ;  mcxTingPrintAfter(name, "%s", ph+1)
      ;  }
         else
         mcxTingPrint(name, "%s", mkprefix)
   ;  }
      else
      {  const char* slash = strrchr(mlp->fnin->str, '/')
      ;  if (slash)
         {  mcxTingPrint(name, mlp->fnin->str)
         ;  mcxTingSplice(name, "out.", slash - mlp->fnin->str + 1, 0, 4)
      ;  }
         else
         mcxTingPrint(name, "out.%s", mlp->fnin->str)
   ;  }

      mcxTingPrintAfter(name, ".%s", suf->str)
   ;  mcxTingWrite(mlp->xfout->fn, name->str)

   ;  if (!mpp->dump_stem->len)
      mcxTingPrint(mpp->dump_stem, "%s.%s", mlp->fnin->str, suf->str)

   ;  mcxTingFree(&name)
;  }


mcxstatus mclAlgorithmInit
(  const mcxOption*  opts
,  mcxHash*       myOpts
,  const char*    fname
,  mclAlgParam*   mlp
)
   {  mclProcParam*  mpp  =  mlp->mpp
   ;  const char* mkappend = NULL, *mkprefix = NULL
   ;  mcxTing* suf = mcxTingEmpty(NULL, 20)
   ;  const mcxOption* opt
   ;  int mkbounce = 0
   ;  float f, f_0  =  0.0
   ;  int i_1     =  1
   ;  int i_10    =  10
   ;  int i
   ;  long l

   ;  if (fname)
      mcxTingWrite(mlp->fnin, fname)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = mcxOptFind(opt->anch->tag, myOpts)
      ;  mcxbool  vok   = TRUE        /* value ok */

      ;  mcxTingPrintAfter(mlp->cline, " %s", opt->anch->tag)

      ;  if (opt->val)
         mcxTingPrintAfter(mlp->cline, " %s", opt->val)

      ;  if (!anch)     /* not in myOpts */
         continue

      ;  switch(anch->id)
         {  case ALG_OPT_EXPECT_ABC
         :  mlp->stream_modes |= MCLXIO_STREAM_ABC
         ;  break
         ;

            case ALG_OPT_ABC
         :  mlp->stream_modes |= MCLXIO_STREAM_ABC
         ;  mlp->stream_write_labels = TRUE
         ;  break
         ;

            case ALG_OPT_EXPECT_123
         :  mlp->stream_modes |= MCLXIO_STREAM_123
         ;  break
         ;

            case ALG_OPT_CACHE_XP
         :  mlp->mpp->fname_expanded =  mcxTingNew(opt->val)
         ;  break
         ;

            case ALG_OPT_EXPECT_PACKED
         :  mlp->stream_modes |= MCLXIO_STREAM_PACKED
         ;  break
         ;

            case ALG_OPT_ABC_LOGTRANSFORM
         :  BIT_OFF(mlp->stream_modes, MCLXIO_STREAM_NEGLOGTRANSFORM)
         ;  BIT_ON(mlp->stream_modes, MCLXIO_STREAM_LOGTRANSFORM)
         ;  break
         ;

            case ALG_OPT_ABC_NEGLOGTRANSFORM
         :  BIT_ON(mlp->stream_modes, MCLXIO_STREAM_NEGLOGTRANSFORM)
         ;  BIT_OFF(mlp->stream_modes, MCLXIO_STREAM_LOGTRANSFORM)
         ;  break
         ;

            case ALG_OPT_ABC_TRANSFORM
         :  mlp->stream_transform_spec = mcxTingNew(opt->val)
         ;  break
         ;

            case ALG_OPT_TRANSFORM
         :  mlp->transform_spec  =  mcxTingNew(opt->val)
         ;  break
         ;

            case ALG_OPT_CACHE_TAB
         :  mlp->stream_tab_wname  =  mcxTingNew(opt->val)
         ;  break
         ;

            case ALG_OPT_TAB_RESTRICT
         :  mcxTingWrite(mlp->stream_tab_rname, opt->val)
         ;  mlp->stream_modes |= MCLXIO_STREAM_TAB_RESTRICT
         ;  break
         ;

            case ALG_OPT_TAB_EXTEND
         :  mcxTingWrite(mlp->stream_tab_rname, opt->val)
         ;  mlp->stream_modes |= MCLXIO_STREAM_TAB_EXTEND
         ;  break
         ;

            case ALG_OPT_TAB_STRICT
         :  mcxTingWrite(mlp->stream_tab_rname, opt->val)
         ;  mlp->stream_modes |= MCLXIO_STREAM_TAB_STRICT
         ;  break
         ;

            case ALG_OPT_TAB_USE
         :  mcxTingWrite(mlp->stream_tab_rname, opt->val)
         ;  mlp->stream_write_labels = TRUE
         ;  break
         ;

            case ALG_OPT_YIELD_ABC
         :  mlp->stream_write_labels = TRUE
         ;  break
         ;

            case ALG_OPT_CACHE_MXTF
         :  mlp->cache_mxtf  =  mcxTingNew(opt->val)
         ;  break
         ;

            case ALG_OPT_CACHE_MXIN
         :  mlp->cache_mxin  =  mcxTingNew(opt->val)
         ;  break
         ;

            case ALG_OPT_BINARY
         :  mclxSetBinaryIO()
         ;  break
         ;

            case ALG_OPT_UNCHECKED
         :  {  mcxTing* tmp = mcxTingPrint(NULL, "MCLXIOUNCHECKED=1")
            ;  const char* str = mcxTinguish(tmp)
            ;  putenv(str)
         ;  }
            break
         ;

            case ALG_OPT_SHOWVERSION
         :  showSetting()
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_SHOWRAM
         :  l = atol(opt->val)
         ;  howMuchRam(l, mlp->mpp)
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_AMOIXA
         :  mcxOptApropos
            (  stdout
            ,  "mcl-algorithm"
            ,  NULL
            ,  15
            ,  MCX_OPT_DISPLAY_HIDDEN |   MCX_OPT_DISPLAY_SKIP
            ,  mclAlgOptions
            )
         ;  mcxOptApropos
            (  stdout
            ,  "mcl-process"
            ,  NULL
            ,  15
            ,  MCX_OPT_DISPLAY_HIDDEN |   MCX_OPT_DISPLAY_SKIP
            ,  mclProcOptions
            )
         ;  fputs(legend, stdout)
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_APROPOS
         :  mcxOptApropos
            (  stdout
            ,  "mcl-algorithm"
            ,  NULL
            ,  15
            ,  MCX_OPT_DISPLAY_SKIP
            ,  mclAlgOptions
            )
         ;  mcxOptApropos
            (  stdout
            ,  "mcl-process"
            ,  NULL
            ,  15
            ,  MCX_OPT_DISPLAY_SKIP
            ,  mclProcOptions
            )
         ;  fputs(legend, stdout)
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_SHOWHELP
         :  {  const char** h = mclHelp
            ;  while (*h)
               fprintf(stdout, "%s\n", *h++)
         ;  }
            return ALG_INIT_DONE
         ;

            case ALG_OPT_SHOWCHARTER
         :  juryCharter(mpp)
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_SHOWSETTINGS
         :  showSettings(mlp)
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_SHOWSCHEMES
         :  mclShowSchemes()
         ;  return ALG_INIT_DONE
         ;

            case ALG_OPT_FORCE_CONNECTED
         :  case ALG_OPT_CHECK_CONNECTED
         :  case ALG_OPT_OUTPUT_LIMIT
         :  case ALG_OPT_KEEP_OVERLAP
         :  case ALG_OPT_APPEND_LOG
         :  case ALG_OPT_SHOW_LOG
         :  case ALG_OPT_ANALYZE
         :  case ALG_OPT_LINT
         :
            vok = set_bit(mlp, opt->anch->tag, anch->id, opt->val)
         ;  break
         ;

            case ALG_OPT_AUTOAPPEND
         :  mkappend = opt->val
         ;  break
         ;
            case ALG_OPT_AUTOBOUNCENAME
         :  mkbounce = opts[1].anch ? 2 : 1  /* check whether it's last option */
         ;  break
         ;
            case ALG_OPT_AUTOBOUNCEFIX
         :  mkbounce = 3
         ;  break
         ;
            case ALG_OPT_AUTOPREFIX
         :  mkprefix = opt->val
         ;  break
         ;

            case ALG_OPT_OUTPUTFILE
         :  mcxIOnewName(mlp->xfout, opt->val)
         ;  break
         ;

            case ALG_OPT_CTRMAXAVG
         :  i = atoi(opt->val)
         ;  i = MAX(i, 0);  i = MIN(i, 100)     /* fixme; no warnings */
         ;  mlp->pre_ctrmaxavg = (double) i / 100.0
         ;  break
         ;

            case ALG_OPT_CENTERING
         :  f = atof(opt->val)
         ;  vok = chb(anch->tag, 'f', &f, fltGq, &f_0, NULL, NULL)
         ;  if (f<0.5 || f>5.0)
            mcxWarn
            (  "mcl"
            ,  "warning: conceivable/normal ranges for -c are [0.5, 5.0]"
            )
         ;  if (vok)
            mlp->pre_center = f
         ;  break
         ;

            case ALG_OPT_SORT
         :  if (!strcmp(opt->val, "lex"))
            mlp->sortMode = 'l'
         ;  else if (!strcmp(opt->val, "size"))
            mlp->sortMode = 's'
         ;  else if (!strcmp(opt->val, "revsize"))
            mlp->sortMode = 'S'
         ;  else if (!strcmp(opt->val, "none"))
            mlp->sortMode = 'n'
         ;  break
         ;

            case ALG_OPT_PREINFLATION
         :  f = atof(opt->val)
         ;  if ((vok = chb(anch->tag, 'f', &f, fltGt, &f_0, NULL, NULL)))
            mlp->pre_inflation =  f
         ;  break
         ;

            case ALG_OPT_DIAGADD
         :  f = atof(opt->val)
         ;  if ((vok = chb(anch->tag, 'f', &f, fltGt, &f_0, NULL, NULL)))
               mlp->pre_diag =  f
            ,  mlp->pre_center =  -1.0
         ;  break
         ;

            case ALG_OPT_DIGITS
         :  l = atol(opt->val)
         ;  i = l
         ;  if ((vok = chb(anch->tag,'i', &i, intGq, &i_1, intLq, &i_10)))
               mlp->expandDigits = i
            ,  mpp->printDigits = i
         ;  break
         ;

            case ALG_OPT_PREPRUNE
         :  i = atoi(opt->val)
         ;  if ((vok = chb(anch->tag, 'i', &i, intGq, &i_1, NULL, NULL)))
            mlp->pre_maxnum = i
         ;  break
         ;

         }

         if (vok != TRUE)
         return ALG_INIT_FAIL
   ;  }

      if (!mlp->xfout->fn->len)
      make_output_name(mlp, suf, mkappend, mkprefix)

   ;  if (!mpp->dump_stem->len)
      {  if (!strcmp(mlp->xfout->fn->str, "-"))
         mcxTingPrint(mpp->dump_stem, "%s", mlp->fnin->str)
      ;  else
         mcxTingPrint(mpp->dump_stem, "%s", mlp->xfout->fn->str)
   ;  }

              /* mkbounce == 2: -az was not the last option.
               * Print to stderr too as way of a small alert.
              */
      if (mkbounce)
      {  if (mkbounce == 2)
         fprintf(stderr, "%s\n", mlp->xfout->fn->str)
      ;  if (mkbounce == 3)
         fprintf
         (  stdout
         ,  "%s%s", suf->str
         ,  isatty(fileno(stdout)) ? "\n" : ""
         )
      ;  else
         fprintf
         (  stdout
         ,  "%s%s", mlp->xfout->fn->str
         ,  isatty(fileno(stdout)) ? "\n" : ""
         )
      ;  return ALG_INIT_DONE
   ;  }

                        /* truncate (cat operates in append mode */
      if (MCPVB(mpp, MCPVB_CAT))
      {  mcxIO* tmp = NULL
      ;  mcxTingWrite(mpp->dump_stem, mlp->xfout->fn->str)
      ;  tmp = mcxIOnew(mpp->dump_stem->str, "w")
      ;  mcxIOtestOpen(tmp, RETURN_ON_FAIL)
      ;  mcxIOfree(&tmp)
   ;  }

      if (mlp->stream_tab_rname->len)
      {  mcxIO* xftmp = mcxIOnew(mlp->stream_tab_rname->str, "r")
      ;  mlp->tab = mclTabRead(xftmp, NULL, EXIT_ON_FAIL)
      ;  mlp->mpp->dump_tab = mlp->tab
      ;  mcxIOfree(&xftmp)
   ;  }

      mcxTingFree(&suf)
   ;  return ALG_INIT_OK
;  }


void showSettings
(  mclAlgParam* mlp
)
   {  mclShowSettings(stdout, mlp->mpp, TRUE)
;  }


mclAlgParam* mclAlgParamNew
(  mclProcParam* mpp
,  const char* prog
,  const char* fn
)  
                                                /* mq, valgrind debugging */
   {  mclAlgParam* mlp = (mclAlgParam*)
                               mcxAlloc(sizeof(mclAlgParam), EXIT_ON_FAIL)
   ;  if (!mpp)
      mpp =  mclProcParamNew()

   ;  mlp->mpp             =     mpp

   ;  mlp->xfout           =     mcxIOnew("", "w")

   ;  mlp->expandDigits    =     8

   ;  mlp->pre_center      =     1.0
   ;  mlp->pre_ctrmaxavg   =     0.0
   ;  mlp->pre_diag        =    -1.0

   ;  mlp->pre_inflation   =    -1.0
   ;  mlp->pre_maxnum      =     0

   ;  mlp->modes           =     0

   ;  mlp->stream_modes    =     0
   ;  mlp->stream_tab_wname=     NULL
   ;  mlp->stream_tab_rname=     mcxTingEmpty(NULL, 0)
   ;  mlp->stream_write_labels =     FALSE
   ;  mlp->cache_mxin      =     NULL
   ;  mlp->cache_mxtf      =     NULL
   ;  mlp->is_transformed  =     FALSE  /* when reading back cached graph */
   ;  mlp->tab             =     NULL

   ;  mlp->stream_transform_spec  =     NULL
   ;  mlp->stream_transform       =    NULL

   ;  mlp->transform_spec  =     NULL
   ;  mlp->transform       =    NULL

   ;  mlp->writeMode       =     'a'
   ;  mlp->sortMode        =     'S'
   ;  mlp->cline           =     mcxTingPrintAfter(NULL, "%s %s", prog, fn)
   ;  mlp->fnin            =     mcxTingEmpty(NULL, 10)
   ;  return mlp
;  }


void mclAlgParamFree
(  mclAlgParam** app
)
   {  mclAlgParam *mlp  = *app
   ;  mclProcParamFree(&(mlp->mpp))
   ;  mcxTingFree(&(mlp->cline))
   ;  mcxTingFree(&(mlp->fnin))
   ;  mcxIOfree(&(mlp->xfout))

   ;  mcxTingFree(&(mlp->stream_tab_wname))
   ;  mcxTingFree(&(mlp->stream_tab_rname))
   ;  mcxTingFree(&(mlp->cache_mxin))

   ;  mclTabFree(&(mlp->tab))

   ;  mcxFree(mlp)
   ;  *app = NULL
;  }


void mclAlgPrintInfo
(  FILE* fp
,  mclAlgParam* mlp  
,  mclMatrix* cl
)
   {  fprintf(fp, "version <%s>\n", mclDateTag)
   ;  fprintf(fp, "input file name <%s>\n", mlp->fnin->str)
   ;  if (cl)
      fprintf(fp, "number of nodes <%ld>\n", (long) N_ROWS(cl))
   ;  if (cl)
      fprintf(fp, "number of clusters <%ld>\n", (long) N_COLS(cl))
   ;  fprintf(fp, "command line <%s>\n", mlp->cline->str)
   ;  fprintf(fp, "total time usage <%.2f>\n", (double) mlp->mpp->lap)  
   ;  fprintf(fp, "number of iterations <%d>\n", (int) mlp->mpp->n_ite)  
;  }


const char* juryBabble[]
=
{  ""
,  "The  'synopsis' reported  by  mcl when  it is  done  corresponds with  a"
,  "weighted average of  the reported /jury/ marks. The  average is computed"
,  "as (5*m1+2*m2+m3)/8 and the correspondence is shown in the table above."
,  ""
,  "There are  three jury marks for  the first three rounds  of expansion. A"
,  "jury mark  is the percentage of  kept stochastic mass averaged  over the"
,  "worst X cases (c.q. matrix  columns). The number of cases considered"
,  "can be  set using -nj,  so you  have some control  over the mood  of the"
,  "jury. If the synopsis is low,  just increase the -scheme parameter. Read"
,  "the mcl manual for more information. Do not feel intimidated: -scheme is"
,  "your friend, and the synopsis is there  to remind you of its existence -"
,  "you can forget the rest."
,  ""
,  NULL
}  ;

void juryCharter
(  mclProcParam* mpp
)
   {  grade*   gr    =  gradeDir+1
   ;  const char** g =  juryBabble
   ;  fputc('\n', stdout)
   ;  while (gr->mark >= 0)
         fprintf(stdout, "%3d%20s\n", (int) gr->mark, gr->ind)
      ,  gr++
   ;  while (*g)
      fprintf(stdout, "%s\n", *g++)
   ;  return
;  }


void mclWriteLog
(  FILE* fp
,  mclAlgParam* mlp
,  mclMatrix* cl
)
   {  fputs("\n(mclruninfo\n\n", fp)
   ;  mclAlgPrintInfo(fp, mlp, cl)
   ;  fputc('\n', fp)
   ;  mclProcPrintInfo(fp, mlp->mpp)
   ;  fputc('\n', fp)
   ;  fprintf(mlp->xfout->fp, mlp->mpp->massLog->str)
   ;  fputs(")\n", fp)
;  }


void  howMuchRam
(  long  n
,  mclProcParam* mpp
)
   {  int x    =  MAX(mpp->mxp->num_select, mpp->mxp->num_recover)
   ;  int y    =  MIN(n, x)
   ;  mclIvp ivps[10]
   ;  int l    =  sizeof(ivps) / 10
   ;  double r =  (2.0 * l * y * n) / (1024.0 * 1024.0)
   ;  fprintf
      (  stdout
      ,  "The current settings require at most <%.2fM> RAM for a\n"
         "graph with <%ld> nodes, assuming the average node degree of\n"
         "the input graph does not exceed <%ld>. This (RAM number)\n"
         "will usually but not always be too pessimistic an estimate.\n"
      ,  (double) r
      ,  (long) n
      ,  (long) y
      )
;  }



void mclCenter
(  mclMatrix*        mx
,  double            w_self
,  double            o_ctrmax
)
   {  long cct

   ;  for (cct=0;cct<N_COLS(mx);cct++)
      {  mclVector*  vec      =  mx->cols+cct
      ;  mclIvp*     match    =  NULL
      ;  int         offset   =  -1
      ;  double      maxval   =  mclvMaxValue(vec)

      ;  if (w_self > 0 && vec->n_ivps)
         {  mclvIdxVal(vec, vec->vid, &offset)
         ;  if (offset >= 0)
            {  match = (vec->ivps+offset)
            ;  match->val  =  0.0
         ;  }
            else
            {  mclvInsertIdx (vec, vec->vid, 0.0)
            ;  mclvIdxVal(vec, vec->vid, &offset)
            ;  if (offset < 0)
                  mcxErr("mclCenter", "error: insertion failed ?!?")
               ,  mcxExit(1)
            ;  match    =  (vec->ivps+offset)
         ;  }

            {  double sum =  mclvSum(vec)

            ;  if (sum > 0.0)
               {  double  selfinit
                  =     o_ctrmax * (mclvPowSum(vec, 2) / sum)
                     +  (1 - o_ctrmax) * maxval
               ;  match->val = w_self * selfinit
               ;  if (!match->val)
                  match->val =  1
            ;  }
               else
               match->val =  1
         ;  }
         }
         else if (!vec->n_ivps)
            mcxErr
            ("loop assignment", "adding loop to void column <%ld>", (long) cct)
         ,  mclvInsertIdx(vec, vec->vid, 1.0)
   ;  }
   }

void mclAlgOptionsInit
(  void
)
   {  mcxOptAnchorSortById
      (mclAlgOptions, sizeof(mclAlgOptions)/sizeof(mcxOptAnchor) -1)
;  }


static mclx* test_tab
(  mclx* mx
,  mclAlgParam* mlp
)
   {  mclv* dom
   ;  if (!mlp->tab)
      return mx

   ;  dom = mlp->tab->domain

                        /* DangerSign fixme TODO */
                        /* we are (ab)using the stream interface here */
   ;  if (mlp->stream_modes & MCLXIO_STREAM_TAB_STRICT)
      {  if (!mcldEquate(mx->dom_cols, dom, MCLD_EQ_SUB))
         mcxDie(1, "mcl", "matrix domain does not subsume tab domain")
   ;  }
                        /* DangerSign fixme TODO */
                        /* we are (ab)using the stream interface here */
      else if (mlp->stream_modes & MCLXIO_STREAM_TAB_RESTRICT)
      {  if (!mcldEquate(mx->dom_cols, dom, MCLD_EQ_EQUAL))
         {  mclx* sub = mclxSub(mx, dom, dom)
         ;  mclxFree(&mx)
         ;  mx = sub
      ;  }
      }
      return mx
;  }


static mclx* mclAlgorithmStreamIn
(  mcxIO* xfin
,  mclAlgParam* mlp
,  mcxbool reread
)
   {  mclTab* tabxch = mlp->tab
   ;  mcxbool abc = mlp->stream_modes & MCLXIO_STREAM_ABC
   ;  mclx* mx = NULL

   ;  if
      (  reread
      && !  (  mlp->stream_modes
            &  (  MCLXIO_STREAM_TAB_EXTEND
               |  MCLXIO_STREAM_TAB_STRICT |  MCLXIO_STREAM_TAB_RESTRICT
      )     )  )
      mlp->stream_modes |= MCLXIO_STREAM_TAB_RESTRICT

   ;  mx
      =  mclxIOstreamIn
         (  xfin
         ,  mlp->stream_modes | MCLXIO_STREAM_MIRROR
         ,  mlp->stream_transform
         ,  mclpMergeMax
         ,  &tabxch
         ,  NULL
         ,  EXIT_ON_FAIL
         )

   ;  if (abc)
      {  if (!mlp->tab || mlp->tab != tabxch)
         {  mcxTell("mcl", "new tab created")
         ;  mclTabFree(&(mlp->tab))
      ;  }

         mlp->tab = tabxch
      ;  mlp->mpp->dump_tab = mlp->tab

      ;  if (mlp->tab && mlp->stream_tab_wname)
         {  mcxIO* xftab = mcxIOnew(mlp->stream_tab_wname->str, "w")
         ;  mcxIOopen(xftab, EXIT_ON_FAIL)
         ;  mclTabWrite(tabxch, xftab, NULL, RETURN_ON_FAIL)
         ;  mcxIOfree(&xftab)
      ;  }
      }
      else
      mx = test_tab(mx, mlp)

   ;  return mx
;  }


mclx* mclAlgorithmRead
(  mclAlgParam* mlp
,  mcxbool reread
)
   {  const char* me =  "mclAlgorithmRead"
   ;  mcxIO* xfin    =  mcxIOnew(mlp->fnin->str, "r")
   ;  mcxTing* cache_name = NULL
   ;  mclx* mx       =  NULL

   ;  if (reread && (mcxIOopen(xfin, RETURN_ON_FAIL) || mcxIOstdio(xfin)))
      {  cache_name = mlp->cache_mxin ? mlp->cache_mxin : mlp->cache_mxtf
      ;  if (cache_name)
         {  mcxIOclose(xfin)
         ;  mcxIOrenew(xfin, cache_name->str, NULL)
         ;  mcxTell
            (  me
            ,  "fall-back, trying to read cached graph <%s>"
            ,  cache_name->str
            )
         ;  if (mcxIOopen(xfin, RETURN_ON_FAIL))
            {  mcxTell(me, "fall-back failed")
            ;  mcxIOfree(&xfin)
         ;  }
         }
         else
         mcxIOfree(&xfin)
      ;  if (xfin)
         mlp->stream_modes = 0
   ;  }

      if (!xfin)
      return NULL

   ;  if
      (  (mlp->transform_spec && !mlp->transform)
      && !(mlp->transform = mclpTFparse(NULL, mlp->transform_spec))
      )
      mcxDie(1, "mcl", "errors in tf-spec")

   ;  if
      (  (mlp->stream_transform_spec && !mlp->stream_transform)
      && !(mlp->stream_transform = mclpTFparse(NULL, mlp->stream_transform_spec))
      )
      mcxDie(1, "mcl", "errors in stream tf-spec")

   ;  if (mlp->stream_modes & MCLXIO_STREAM_READ)
      mx = mclAlgorithmStreamIn(xfin, mlp, reread)
   ;  else
      {  mx = mclxReadx(xfin, RETURN_ON_FAIL, MCL_READX_GRAPH)
      ;  if (mx)
         mx = test_tab(mx, mlp)
   ;  }

      if (mx && reread && cache_name == mlp->cache_mxtf)
      mlp->is_transformed = TRUE

   ;  mcxIOfree(&xfin)
   ;  return mx
;  }


int mclAlgorithmTransform
(  mclx* mx  
,  mclAlgParam* mlp
)
   {  int n_ops = 0
   ;  if (mlp->pre_maxnum)
         mclxMakeSparse(mx, 0, mlp->pre_maxnum)
      ,  n_ops++

   ;  if (mlp->transform)
         mclxUnaryList(mx, mlp->transform)
      ,  n_ops++

   ;  if (mlp->pre_inflation > 0)
         mclxInflate(mx, mlp->pre_inflation)
      ,  n_ops++

   ;  if (mlp->pre_diag > 0)
      {  mclMatrix* diagAll   =  mclxConstDiag
                                 (  mclvCopy(NULL, mx->dom_cols)
                                 ,  mlp->pre_diag
                                 )
      ;  mclMatrix*  t  =  mclxAdd(mx, diagAll)
      /* fixme this is too costly for large mtcs */
      ;  mclxFree(&mx)
      ;  mclxFree(&diagAll)
      ;  mx       =  t
   ;  }

      if (mlp->pre_center >= 0)
      mclCenter(mx, mlp->pre_center, mlp->pre_ctrmaxavg)

   ;  mclxMakeStochastic(mx)
   ;  return n_ops
;  }


mcxstatus mclAlgorithmCacheGraph
(  mclx* mx
,  mclAlgParam* mlp
,  char ord
)
   {  mcxTing* name
      =        ord == 'a'
         ?  mlp->cache_mxin
         :     ord == 'b'
         ?  mlp->cache_mxtf
         :  NULL

   ;  if (name)
      {  mcxIO* xfmx = mcxIOnew(name->str, "w")
      ;  if (mcxIOopen(xfmx, RETURN_ON_FAIL))
         return STATUS_FAIL
      ;  if (mclxWrite(mx, xfmx, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL))
         return STATUS_FAIL
      ;  mcxIOclose(xfmx)
      ;  mcxIOfree(&xfmx)
   ;  }
      return STATUS_OK
;  }

