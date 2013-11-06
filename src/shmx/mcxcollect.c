/*   (C) Copyright 2006, 2007, 2008 Stijn van Dongen
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

#include "impala/io.h"
#include "impala/matrix.h"
#include "impala/tab.h"
#include "impala/stream.h"

#include "util/types.h"
#include "util/io.h"
#include "util/err.h"
#include "util/opt.h"

#include "gryphon/path.h"


enum
{  MY_OPT_TAB = MCX_DISP_UNUSED
,  MY_OPT_SUMMARY
}  ;


mcxOptAnchor collectOptions[] =
{  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "specify tab file to map collect indices to labels, if appropriate"
   }
,  {  "--summary"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SUMMARY
   ,  NULL
   ,  "as final step output maximum, median, minimum, and average"
   }
,  {  NULL, 0, MCX_DISP_UNUSED, NULL, NULL }
}  ;

static   mcxIO* xftab_g       =   (void*) -1;
static   mcxmode  summary_g   =   -1;

static mcxstatus collectInit
(  void
)
   {  xftab_g        =  NULL
   ;  summary_g      =  0
   ;  return STATUS_OK
;  }


static mcxstatus collectArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_TAB
      :  xftab_g = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_SUMMARY
      :  summary_g = 1
      ;  break
      ;

         default
      :  mcxExit(1) 
      ;
      }
   ;  return STATUS_OK
;  }


typedef struct
{  const char* label
;  double      val
;
}  aggr        ;


int aggr_cmp(const void* a1, const void* a2)
   {  const aggr* ag1 = a1, *ag2 = a2
   ;  return ag1->val > ag2->val ? 1 : ag1->val < ag2->val ? -1 : 0
;  }


static dim do_a_file
(  aggr** collectpp
,  const char* fname
,  dim collect_n
)
   {  mcxIO* xf = mcxIOnew(fname, "r")
   ;  mcxTing* buf = mcxTingEmpty(NULL, 100)
   ;  mcxTing* lbl = mcxTingEmpty(NULL, 100)
   ;  mcxstatus status = STATUS_OK
   ;  aggr* collect = *collectpp
   ;  dim ct = 0, collect_alloc = 0

   ;  if (!collect_n)
         collect_alloc = 100
      ,  collect = mcxNAlloc(collect_alloc, sizeof collect[0], NULL, EXIT_ON_FAIL)

   ;  mcxIOopen(xf, EXIT_ON_FAIL)

   ;  while (STATUS_OK == (status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)))
      {  double val
      ;  mcxTingEnsure(lbl, buf->len)
      ;  if (2 != sscanf(buf->str, "%s%lg", lbl->str, &val))
         mcxDie(1, "mcx collect", "error at line %d file %s", (int) xf->lc, fname)
      ;  lbl->len = strlen(lbl->str)
      ;  if (!collect_n)
         {  if (ct >= collect_alloc)
            {  dim collect_realloc = collect_alloc * 1.44
            ;  collect = mcxNRealloc(collect, collect_realloc, collect_alloc, sizeof collect[0], NULL, EXIT_ON_FAIL)
            ;  collect_alloc = collect_realloc
         ;  }
            collect[ct].label = mcxTingStr(lbl)
         ;  collect[ct].val = val
      ;  }
         else
         {  if (ct >= collect_n)
            mcxDie(1, "mcx collect", "additional lines in file %s", fname)
         ;  if (strcmp(collect[ct].label, lbl->str))
            mcxDie
            (  1
            ,  "mcx collect"
            ,  "label conflict %s/%s at line %d in file %s"
            ,  collect[ct].label
            ,  lbl->str
            ,  (int) xf->lc, fname
            )
         ;  collect[ct].val += val 
      ;  }
         ct++
   ;  }

      if (collect_n)
      {  if (ct != collect_n)
         mcxDie(1, "mcx collect", "not enough lines in file %s", fname)
   ;  }
      else
      {  if (!ct)
         mcxDie(1, "mcx collect", "empty file(s)")
      ;  *collectpp = collect
   ;  }

      mcxIOfree(&xf)
   ;  return ct
;  }


static mcxstatus collectMain
(  int                  argc
,  const char*          argv[]
)
   {  aggr* collect = NULL
   ;  int a
   ;  dim i, collect_n = 0
   ;  mclTab* tab = NULL
   ;  double avg = 0.0
  /*  mcxHash* map = NULL */

   ;  if (xftab_g)
         tab = mclTabRead(xftab_g, NULL, EXIT_ON_FAIL)
            /* map not used; perhaps someday we want to map labels to indexes?
             * in that case, we could also simply reverse the tab when reading ..
      ,  map = mclTabHash(tab)
            */

   ;  if (argc)
      collect_n = do_a_file(&collect, argv[0], 0)

   ;  if (tab && collect_n != N_TAB(tab))
      mcxErr("mcx collect", "tab has differing size, continuing anyway")

   ;  for (a=1;a<argc;a++)
      do_a_file(&collect, argv[a], collect_n)

   /* fimxe: dispatch on binary_g */

   ;  for (i=0;i<collect_n;i++)
      {  const char* lb = collect[i].label
      ;  if (tab)
         {  unsigned u = atoi(lb)
         ;  lb = mclTabGet(tab, u, NULL)
         ;  if (TAB_IS_NA(tab, lb))
            mcxDie(1, "mcx collect", "no label found for index %ld - abort", (long) u)
      ;  }
         if (summary_g)
         avg += collect[i].val
      ;  else
         fprintf(stdout, "%s\t%g\n", lb, collect[i].val)
   ;  }
      if (summary_g && collect_n)
      {  dim middle1 = (collect_n-1)/2, middle2 = collect_n/2
      ;  qsort(collect, collect_n, sizeof collect[0], aggr_cmp)
      ;  avg /= collect_n
      ;  fprintf
         (  stdout
         ,  "%g %g %g %g\n"
         ,  collect[0].val
         ,  (collect[middle1].val + collect[middle2].val) / 2
         ,  collect[collect_n-1].val
         ,  avg
         )
   ;  }
      return STATUS_OK
;  }


static mcxDispHook collectEntry
=  {  "collect"
   ,  "collect [files]"
   ,  collectOptions
   ,  sizeof(collectOptions)/sizeof(mcxOptAnchor) - 1

   ,  collectArgHandle
   ,  collectInit
   ,  collectMain

   ,  1
   ,  -1
   ,  MCX_DISP_DEFAULT
   }
;


mcxDispHook* mcxDispHookCollect
(  void
)
   {  return &collectEntry
;  }


