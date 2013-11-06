/*   (C) Copyright 2006, 2007, 2008 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO erdos:
 *    in daemon mode create cookie ..
 *    -  (perhaps) [create new array excluding i with seen[aow[i]] = 0]
 *    -  ??? use dedicated matrix structure (halves memory requirement)
 *       -> no; rather think dijkstra and void*
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
#include "util/heap.h"
#include "util/ding.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/equate.h"
#include "util/rand.h"
#include "util/opt.h"
#include "util/compile.h"

#include "impala/matrix.h"
#include "impala/io.h"
#include "impala/tab.h"
#include "impala/app.h"

#include "clew/clm.h"
#include "gryphon/path.h"


/* fixme:  link together should not start with x or y.
 * be careful what happens when x <-> y
 * and x <-> y <-> z and even the four-node path ->
 * middle is adjacent to x or y.
 * [uh, wtf does that fixme even mean?]
*/


enum
{  MY_OPT_IMX    =   MCX_DISP_UNUSED
,  MY_OPT_TAB
}  ;



mcxOptAnchor erdosOptions[] =
{  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "specify input matrix"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "specify tab file"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


static unsigned debug_g =  -1;

static mcxIO* xfmx_g    =  (void*) -1;
static mcxIO* xftab_g   =  (void*) -1;

static mclTab* tab_g    =  (void*) -1;
static mcxHash* hsh_g   =  (void*) -1;


static mcxstatus allInit
(  void
)
   {  xfmx_g         =  mcxIOnew("-", "r")
   ;  xftab_g        =  NULL
   ;  tab_g          =  NULL
   ;  hsh_g          =  NULL
   ;  debug_g        =  0
   ;  return STATUS_OK
;  }


static mcxstatus erdosArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  mcxIOnewName(xfmx_g, val)
      ;  break
      ;

         case MY_OPT_TAB
      :  xftab_g = mcxIOnew(val, "r")
      ;  break
      ;

         default
      :  mcxExit(1)
      ;
   ;  }
      return STATUS_OK
;  }


void label_not_found
(  const mcxTing* t
)
   {  fprintf(stdout, "-- LABEL NOT FOUND (%s)\n", t->str)
;  }


mcxstatus check_bounds
(  const mclx* mx
,  long idx
)
   {  if (idx < 0 || (dim) idx >= N_COLS(mx))
      {  fprintf(stdout, "-- ARGUMENT OUT OF BOUNDS (%ld)\n", idx)
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }



static void erdos_link_together
(  mclx* mx
,  mclv* tails
,  mclv* heads
)
   {  dim v = 0
   ;  mclv* relevant = mclvInit(NULL)
   ;  for (v=0;v<tails->n_ivps;v++)
      {  long t = tails->ivps[v].idx
      ;  dim j
      ;  mclv* nb = mclxGetVector(mx, t, EXIT_ON_FAIL, NULL)
      ;  mcldMeet(nb, heads, relevant)
      ;  for (j=0;j<relevant->n_ivps;j++)
         {  if (tab_g)
            {  long u = relevant->ivps[j].idx
            ;  const char* sx = mclTabGet(tab_g, (long) t, NULL)
            ;  const char* sy = mclTabGet(tab_g, (long) u, NULL)
            ;  if (!sx)
               sx = "NAx"
            ;  if (!sy)
               sy = "NAy"
            ;  fprintf(stdout, "(%s, %s)", sx, sy)
         ;  }
            else
            fprintf(stdout, "(%ld, %ld)", (long) t, (long) relevant->ivps[j].idx)
      ;  }
         if (!relevant->n_ivps)
         fprintf(stdout, "odd, %d has no friends\n", (int) t)
      ;  else
         fprintf(stdout, "\n");
   ;  }
      fprintf(stdout, ".\n");
   ;  mclvFree(&relevant)
;  }



void handle_clcf
(  mclx*    mx
,  mcxTing* sa
)
   {  mcxKV* kv = tab_g ? mcxHashSearch(sa, hsh_g, MCX_DATUM_FIND) : NULL
   ;  long idx = -1
   ;  if (tab_g && !kv)
      {  label_not_found(sa)
      ;  return
   ;  }
      else if (kv)
      idx = VOID_TO_ULONG kv->val
   ;  else
      idx = atoi(sa->str)

   ;  if (check_bounds(mx, idx))
      return

   ;  {  double clcf = mclgCLCF(mx, mx->cols+idx, NULL)
      ;  fprintf(stdout, "%.3f\n", clcf)
   ;  }
   }


void handle_look
(  mclx*    mx
,  mcxTing* sa
)
   {  mcxKV* kv = tab_g ? mcxHashSearch(sa, hsh_g, MCX_DATUM_FIND) : NULL
   ;  long idx = -1
   ;  if (tab_g && !kv)
      {  label_not_found(sa)
      ;  return
   ;  }
      else if (kv)
      idx = VOID_TO_ULONG kv->val
   ;  else
      idx = atoi(sa->str)

   ;  if (check_bounds(mx, idx))
      return

   ;  {  mclv* vec = mx->cols+idx
      ;  dim t
      ;  for (t=0;t<vec->n_ivps;t++)
         {  const char* s = tab_g ? mclTabGet(tab_g, (long) vec->ivps[t].idx, NULL) : NULL
         ;  if (s)
            fprintf(stdout, "   %s\n", s)
         ;  else
            fprintf(stdout, "%12ld\n", (long) vec->ivps[t].idx)
      ;  }
      }
   }


void handle_hairy
(  mclx*    mx
,  mcxTing* sa
)
   {  int num = atoi(sa->str)
   ;  dim t
   ;  mcxHeap* hp 

   ;  if (num <= 0)
      num = 100
   ;  if ((dim) num > N_COLS(mx))
      num = N_COLS(mx)

   ;  hp =  mcxHeapNew
            (  NULL
            ,  num
            ,  sizeof(mclp)
            ,  mclpValCmp
            ,  MCX_MIN_HEAP
            )
   ;  for (t=0;t<N_COLS(mx);t++)
      {  mclp  ivp
      ;  ivp.idx = mx->cols[t].vid
      ;  ivp.val = mx->cols[t].n_ivps
      ;  mcxHeapInsert(hp, &ivp)
   ;  }

      qsort(hp->base, hp->n_inserted, hp->elemSize, hp->cmp)
   /* warning this destroys the heap structure */

   ;  for (t=0; t<hp->n_inserted;t++)
      {  char* p = (char*) hp->base + (t*hp->elemSize)
      ;  mclp* ivp = (void*) p
      ;  const char* s = tab_g ? mclTabGet(tab_g, (long) ivp->idx, NULL) : NULL
      ;  if (s)
         fprintf(stdout, "%20s : %6.0f\n", s, (double) ivp->val)
      ;  else
         fprintf(stdout, "%20ld : %6.0f\n", (long) ivp->idx, (double) ivp->val)
   ;  }

      mcxHeapFree(&hp)
;  }


void handle_query
(  mclx*    mx
,  mcxTing* sa
,  mcxTing* sb
)
   {  if (!strcmp(sa->str, "?hairy"))
      handle_hairy(mx, sb)
   ;  else if (!strcmp(sa->str, "?look"))
      handle_look(mx, sb)
   ;  else if (!strcmp(sa->str, "?clcf"))
      handle_clcf(mx, sb)
   ;  else
      fprintf(stdout, "-- UNKNOWN QUERY [hairy#1 look#1 clcf#1]\n")
;  }


static mcxstatus erdosMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)  
   {  mcxIO* xq = mcxIOnew("-", "r")
   ;  mcxTing* line = mcxTingEmpty(NULL, 100)
   ;  mcxTing* sa = mcxTingEmpty(NULL, 100)
   ;  mcxTing* sb = mcxTingEmpty(NULL, 100)
   ;  mclx* mx

   ;  SSPxy* sspo

   ;  tab_g    =  xftab_g ? mclTabRead(xftab_g, NULL, EXIT_ON_FAIL) : NULL
   ;  hsh_g    =  tab_g ? mclTabHash(tab_g) : NULL
   ;  debug_g  =  mcx_debug_g

   ;  mcxLogLevel =
      MCX_LOG_AGGR | MCX_LOG_MODULE | MCX_LOG_IO | MCX_LOG_GAUGE | MCX_LOG_WARN
   ;  mclx_app_init(stderr)

   ;  mx  = mclxReadx(xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH | MCLX_REQUIRE_CANONICAL)

   ;  mclxAdjustLoops(mx, mclxLoopCBremove, NULL)

   ;  fprintf(stdout, "REMOVING HAIRY NODES\n")
   ;  if (0)
      {  mclv* sel = mclgMakeSparse(mx, 0, 14)
      ;  fprintf
         (  stdout
         ,  "REMOVED %lu nodes from the graph\n"
         ,  (ulong) (N_COLS(mx) - sel->n_ivps)
         )
      ;  mclvFree(&sel)
   ;  }

      sspo = mclgSSPxyNew(mx)

   ;  mcxIOopen(xq, EXIT_ON_FAIL)

   ;  while (1)
      {  int a = -1, b = -2, ns = 0
      ;  mcxbool query = FALSE
      ;  if (tab_g)
         fprintf(stdout, "READY (type two labels)\n")
      ;  else
         fprintf(stdout, "READY (type two graph indices)\n")
      ;  if
         (  STATUS_OK != mcxIOreadLine(xq, line, MCX_READLINE_CHOMP)
            || !strcmp(line->str, ".")
         )
         break

      ;  query = (u8) line->str[0] == '?'

      ;  if (query && (line->len == 1 || isspace((unsigned char) line->str[1])))
         {  fprintf(stdout, "-->\n")
         ;  fprintf(stdout, "?hairy <num>\n")
         ;  fprintf(stdout, "?look <node>\n")
         ;  fprintf(stdout, "?clcf <node>\n")
         ;  fprintf(stdout, "<--\n")
         ;  continue
      ;  }

         mcxTingEnsure(sa, line->len)
      ;  mcxTingEnsure(sb, line->len)

      ;  ns = sscanf(line->str, "%s %s", sa->str, sb->str)
      ;  if (ns != 2)
         {  fprintf(stdout, "-- EXPECT two arguments\n")
         ;  continue
      ;  }

         ;  sa->len = strlen(sa->str)
         ;  sb->len = strlen(sb->str)

      ;  if (query)
         {  handle_query(mx, sa, sb)
         ;  continue                      /* fixme improve flow */
      ;  }
         else if (tab_g)
         {  mcxKV* kv

         ;  if ((kv = mcxHashSearch(sa, hsh_g, MCX_DATUM_FIND)))
            a = VOID_TO_ULONG kv->val     /* fixme (> 2G labels) */
         ;  else
            {  label_not_found(sa)
            ;  continue
         ;  }

            if ((kv = mcxHashSearch(sb, hsh_g, MCX_DATUM_FIND)))
            b = VOID_TO_ULONG kv->val     /* fixme (> 2G labels) */
         ;  else
            {  label_not_found(sb)
            ;  continue
         ;  }
         }
         else
         {  a = atoi(sa->str)
         ;  b = atoi(sb->str)
      ;  }

         if (check_bounds(mx, a))
         continue
      ;  if (check_bounds(mx, b))
         continue

      ;  if (a == b)
         {  fprintf(stdout, "-- ARGUMENTS IDENTICAL (%d,%d)\n", a, b)
         ;  continue
      ;  }

         {  if (!mclgSSPxyQuery(sspo, a, b))
            {  dim t
            ;  fprintf
               (stdout, "===> START (paths of length %d)\n", (int) sspo->length)

            ;  if (sspo->length == 1)
               {  if (tab_g)
                  fprintf(stdout, "(%s, %s)\n.\n", sa->str, sb->str)
               ;  else
                  fprintf(stdout, "(%ld, %ld)\n.\n", (long) a, (long) b)
            ;  }
               else
               for (t=0; t< N_COLS(sspo->pathmx)-1; t++)
               erdos_link_together(mx, sspo->pathmx->cols+t, sspo->pathmx->cols+t+1)

            ;  fprintf(stdout, "<=== END (paths of length %d)\n", (int) sspo->length)
            ;  fprintf(stdout, "PATH LENGTH   %d\n", (int) sspo->length)
            ;  fprintf(stdout, "CONSIDERED    %d\n", (int) sspo->n_considered)
            ;  fprintf(stdout, "PARTICIPANTS  %d\n", (int) sspo->n_involved)
            ;  fprintf(stdout, "---\n")
         ;  }
            else
            fprintf(stderr, "NO PATH\n")

         ;  mclgSSPxyReset(sspo)
      ;  }
      }

      mclgSSPxyFree(&sspo)

   ;  mcxIOfree(&xftab_g)
   ;  mcxIOfree(&xfmx_g)
   ;  mcxIOfree(&xq)
   ;  mcxTingFree(&sa)
   ;  mcxTingFree(&sb)
   ;  mcxTingFree(&line)
   ;  if (N_COLS(mx) < 1<<17)
      {  mclxFree(&mx)
      ;  mclTabFree(&tab_g)
      ;  mcxHashFree(&hsh_g, NULL, NULL)
   ;  }
      return 0
;  }


static mcxDispHook erdosEntry
=  {  "erdos"
   ,  "erdos [options]"
   ,  erdosOptions
   ,  sizeof(erdosOptions)/sizeof(mcxOptAnchor) - 1

   ,  erdosArgHandle
   ,  allInit
   ,  erdosMain

   ,  0
   ,  0
   ,  MCX_DISP_DEFAULT
   }
;


mcxDispHook* mcxDispHookErdos
(  void
)
   {  return &erdosEntry
;  }


