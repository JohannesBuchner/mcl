/*   (C) Copyright 2006, 2007, 2008, 2009 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO erdos:
 *    more matrix transformation options.
 *    -  enable random deletion of edges (to test whether directionality corrupts).
 *    -  enable look + distance
 *
 *    -  in non-interactive mode fail rather than resume.
 *    -  output edge info with mclgCeilNB
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
#include "impala/stream.h"
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
{  MY_OPT_ABC    =   MCX_DISP_UNUSED
,  MY_OPT_IMX
,  MY_OPT_TAB
,  MY_OPT_QUERY
,  MY_OPT_CEILNB
}  ;



mcxOptAnchor erdosOptions[] =
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
   ,  "specify tab file"
   }
,  {  "-query"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_QUERY
   ,  "<fname>"
   ,  "get queries from stream <fname>"
   }
,  {  "-ceil-nb"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CEILNB
   ,  "<num>"
   ,  "shrink neighbourhoods to at most <int> heaviest edges"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


static mclxIOstreamer streamer_g = { 0 };

static const char* me   =  "mcx erdos";
static unsigned debug_g =  -1;
static ulong    ceil_g  =  -1;

static  mcxIO* xfabc_g  =  (void*) -1;
static mcxIO* xfmx_g    =  (void*) -1;
static mcxIO* xftab_g   =  (void*) -1;
static mcxIO* xq_g      =  (void*) -1;

static mclTab* tab_g    =  (void*) -1;
static mcxHash* hsh_g   =  (void*) -1;


static mcxstatus allInit
(  void
)
   {  xfmx_g         =  NULL
   ;  xftab_g        =  NULL
   ;  tab_g          =  NULL
   ;  hsh_g          =  NULL
   ;  xq_g           =  mcxIOnew("-", "r")
   ;  xfabc_g        =  NULL
   ;  debug_g        =  0
   ;  ceil_g         =  0
   ;  return STATUS_OK
;  }


static mcxstatus erdosArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  xfmx_g = mcxIOnew(val, "r")
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

         case MY_OPT_CEILNB
      :  if (mcxStrToul(val, &ceil_g, NULL))
         mcxDie(1, me, "failed to convert <%s> to a number", val)
      ;  break
      ;

         case MY_OPT_QUERY
      :  mcxIOnewName(xq_g, val)
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
   {  fprintf(stdout, "(error label-not-found (%s))\n", t->str)
;  }


mcxstatus check_bounds
(  const mclx* mx
,  long idx
)
   {  if (idx < 0 || (dim) idx >= N_COLS(mx))
      {  fprintf(stdout, "(error argument-out-of-bounds (%ld))\n", idx)
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
   ;  fprintf(stdout, "(");
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
            ;  fprintf(stdout, " (%s %s)", sx, sy)
         ;  }
            else
            fprintf(stdout, " (%ld %ld)", (long) t, (long) relevant->ivps[j].idx)
      ;  }
         if (!relevant->n_ivps)
         mcxErr(me, "odd, %d has no friends\n", (int) t)
   ;  }
      fprintf(stdout, " )\n");
   ;  mclvFree(&relevant)
;  }



void handle_ceil
(  mclx*    mx
,  mcxTing* sa
)
   {  long ceil = 0
   
   ;  if (mcxStrTol(sa->str, &ceil, NULL) || ceil < 0)
      fprintf(stdout,  "(error number-no-good)\n")
   ;  else
      {  dim n_hub, n_in, n_out
      ;  mclv* sel = mclgCeilNB(mx, ceil, &n_hub, &n_in, &n_out)
      ;  fprintf
         (  stdout
         ,  "(ceil (nodes %lu) (edges %lu/%lu))\n"
         ,  (ulong) n_hub
         ,  (ulong) n_in
         ,  (ulong) n_out
         )
      ;  mclvFree(&sel)
   ;  }
   }



void handle_clcf
(  mclx*    mx
,  mcxTing* sa
)
   {  mcxKV* kv = tab_g ? mcxHashSearch(sa, hsh_g, MCX_DATUM_FIND) : NULL
   ;  long idx = -1
   ;  mcxstatus status = STATUS_OK

   ;  if (tab_g && !kv)
      {  label_not_found(sa)
      ;  return
   ;  }
      else if (kv)
      idx = VOID_TO_ULONG kv->val
   ;  else
      status = mcxStrTol(sa->str, &idx, NULL)

   ;  if (status || check_bounds(mx, idx))
      return

   ;  {  double clcf = mclgCLCF(mx, mx->cols+idx, NULL)
      ;  fprintf(stdout, "%.3f\n", clcf)
   ;  }
   }


void handle_list
(  mclx*    mx
,  mcxTing* sa
)
   {  mcxKV* kv = tab_g ? mcxHashSearch(sa, hsh_g, MCX_DATUM_FIND) : NULL
   ;  mcxstatus status  = STATUS_OK
   ;  long idx = -1
   ;  if (tab_g && !kv)
      {  label_not_found(sa)
      ;  return
   ;  }
      else if (kv)
      idx = VOID_TO_ULONG kv->val
   ;  else
      status = mcxStrTol(sa->str, &idx, NULL)

   ;  if (status || check_bounds(mx, idx))
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


void handle_top
(  mclx*    mx
,  mcxTing* sa
)
   {  long num = -1
   ;  dim t
   ;  mcxHeap* hp 

   ;  if (mcxStrTol(sa->str, &num, NULL) || num < 0)
      {  fprintf(stdout,  "(error number-no-good)\n")
      ;  return
   ;  }

      if (!num || (dim) num > N_COLS(mx))
      num = N_COLS(mx)

                     /* Could use mclxColSizes instead */
   ;  hp =  mcxHeapNew
            (  NULL
            ,  num
            ,  sizeof(mclp)
            ,  mclpValRevCmp
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


mclx* handle_query
(  mclx*    mx
,  mcxIO*   xfmx
,  mcxTing* sa
,  mcxTing* sb
)
   {  if (!strcmp(sa->str, ":top"))
      handle_top(mx, sb)
   ;  else if (!strcmp(sa->str, ":list"))
      handle_list(mx, sb)
   ;  else if (!strcmp(sa->str, ":ceil"))
      handle_ceil(mx, sb)
   ;  else if (!strcmp(sa->str, ":reread"))
      {  mclxFree(&mx)
      ;  if (xfabc_g)
         {  streamer_g.tab_sym_in = tab_g
         ;  
            mx
         =  mclxIOstreamIn
            (  xfabc_g
            ,     MCLXIO_STREAM_ABC
               |  MCLXIO_STREAM_MIRROR
               |  MCLXIO_STREAM_SYMMETRIC
               |  MCLXIO_STREAM_GTAB_RESTRICT         /* docme/fixme need to check for tab_g ? */
            ,  NULL
            ,  mclpMergeMax
            ,  &streamer_g    /* has tab, if present */
            ,  EXIT_ON_FAIL
            )
         ;  mcxIOclose(xfabc_g)
      ;  }
         else
         {  mx
            =  mclxReadx
               (xfmx, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH | MCLX_REQUIRE_CANONICAL)
         ;  mcxIOclose(xfmx)
      ;  }
         mclxAdjustLoops(mx, mclxLoopCBremove, NULL)
   ;  }
      else if (!strcmp(sa->str, ":clcf"))
      handle_clcf(mx, sb)
   ;  else
      fprintf(stdout, "(error unknown-query (:ceil#1 :clcf#1 :list#1 :reread :top#1))\n")
   ;  return mx
;  }


mclx* process_queries
(  mcxIO* xq
,  mclx* mx
,  mcxIO* xfmx
,  mclTab* tab
)
   {  mcxTing* line = mcxTingEmpty(NULL, 100)
   ;  mcxTing* sa = mcxTingEmpty(NULL, 100)
   ;  mcxTing* sb = mcxTingEmpty(NULL, 100)
   ;  SSPxy* sspo = mclgSSPxyNew(mx)     /* fixme: use that mx is a member of sspo */

   ;  mcxIOopen(xq, EXIT_ON_FAIL)

   ;  while (1)
      {  long a = -1, b = -2, ns = 0
      ;  mcxbool query = FALSE
      ;  if (isatty(fileno(xq->fp)))
         fprintf
         (  stdout
         ,  "\n(ready (expect two %s or : directive))\n"
         ,  tab ? "labels" : "graph indices"
         )
      ;  if
         (  STATUS_OK != mcxIOreadLine(xq, line, MCX_READLINE_CHOMP)
            || !strcmp(line->str, ".")
         )
         break

      ;  query = (u8) line->str[0] == ':'

      ;  if (query && (line->len == 1 || isspace((unsigned char) line->str[1])))
         {  fprintf(stdout, "-->\n")
         ;  fprintf(stdout, ":ceil <num>\n")
         ;  fprintf(stdout, ":top <num>\n")
         ;  fprintf(stdout, ":list <node>\n")
         ;  fprintf(stdout, ":clcf <node>\n")
         ;  fprintf(stdout, ":reread>\n")
         ;  fprintf(stdout, "<--\n")
         ;  continue
      ;  }

         mcxTingEnsure(sa, line->len)
      ;  mcxTingEnsure(sb, line->len)

      ;  ns = sscanf(line->str, "%s %s", sa->str, sb->str)
      ;  if (ns == 2)
            sa->len = strlen(sa->str)
         ,  sb->len = strlen(sb->str)
      ;  else
            sa->len = strlen(sa->str)
         ,  sb->len = 0
         ,  sb->str[0] = '\0'

      ;  if (!query && ns != 2)
         {  if (line->len)
            fprintf(stdout, "(error expect two nodes or : directive)\n")
         ;  continue
      ;  }

         if (query)
         {  mx = handle_query(mx, xfmx, sa, sb)
         ;  sspo->mx = mx
         ;  fprintf(stdout, "%s\n\n", line->str)
         ;  continue                      /* fixme improve flow */
      ;  }
         else if (tab)
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
         else if (mcxStrTol(sa->str, &a, NULL) || mcxStrTol(sb->str, &b, NULL))
         {  fprintf(stdout,  "(error failed-reading-number)\n")
         ;  continue
      ;  }

         if (check_bounds(mx, a))
         continue
      ;  if (check_bounds(mx, b))
         continue

      ;  fprintf
         (  stdout
         ,  "\n(lattice\n"
            "   (anchors %s %s)\n"
         ,  sa->str
         ,  sb->str
         )

      ;  if (a == b)
         {  fprintf
            (  stdout
            ,  "  (path-length 0)\n"
               "(data\n"
            )
      ;  }
         else
         {  mcxstatus thestat = mclgSSPxyQuery(sspo, a, b)
         ;  dim t

         ;  if (thestat)
            fprintf(stdout,  "   (path-length -2)\n(data\n")
         ;  else if (sspo->length < 0)       /* not in same component */
            fprintf(stdout,  "   (path-length -1)\n(data\n")
         ;  else
            {  fprintf
               (  stdout
               ,  "   (path-length %d)\n"
                  "(data\n"
               ,  (int) sspo->length
               )

            ;  if (sspo->length == 1)
               {  if (tab)
                  fprintf(stdout, "((%s %s))\n", sa->str, sb->str)
               ;  else
                  fprintf(stdout, "((%ld %ld))\n", (long) a, (long) b)
            ;  }
               else
               for (t=0; t< N_COLS(sspo->pathmx)-1; t++)
               erdos_link_together(mx, sspo->pathmx->cols+t, sspo->pathmx->cols+t+1)

            ;  fputs(")\n", stdout)
            ;  fprintf(stdout, "   (anchors %s %s)\n", sa->str, sb->str)
            ;  fprintf(stdout, "   (considered %d)\n", (int) sspo->n_considered)
            ;  fprintf(stdout, "   (participants %d)\n", (int) sspo->n_involved)
            ;  fprintf(stdout, "   (path-length %d)\n", (int) sspo->length)
         ;  }
         }

         fprintf(stdout, ")\n\n")
      ;  mclgSSPxyReset(sspo)
   ;  }
      mcxTingFree(&sa)
   ;  mcxTingFree(&sb)
   ;  mcxTingFree(&line)
   ;  mclgSSPxyFree(&sspo)
   ;  return mx
;  }


static mcxstatus erdosMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)  
   {  mclx* mx
   ;  dim n_hub, n_in, n_out
   ;  mclv* sel

   ;  debug_g  =  mcx_debug_g

   ;  mcxLogLevel =
      MCX_LOG_AGGR | MCX_LOG_MODULE | MCX_LOG_IO | MCX_LOG_GAUGE | MCX_LOG_WARN
   ;  mclx_app_init(stderr)

   ;  if (xfabc_g)
      {  if (xftab_g)
            tab_g = mclTabRead(xftab_g, NULL, EXIT_ON_FAIL)
         ,  streamer_g.tab_sym_in = tab_g
      ;  mx
      =  mclxIOstreamIn
         (  xfabc_g
         ,     MCLXIO_STREAM_ABC
            |  MCLXIO_STREAM_MIRROR
            |  MCLXIO_STREAM_SYMMETRIC
            |  (tab_g ? MCLXIO_STREAM_GTAB_RESTRICT : 0)
         ,  NULL
         ,  mclpMergeMax
         ,  &streamer_g
         ,  EXIT_ON_FAIL
         )
      ;  tab_g = streamer_g.tab_sym_out

      ;  if (!streamer_g.tab_sym_in)
         streamer_g.tab_sym_in = tab_g    /* needed when rereading */

      ;  mcxIOclose(xfabc_g)
   ;  }
      else if (xfmx_g)
      {  mx =  mclxReadx
               (xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH | MCLX_REQUIRE_CANONICAL)
      ;  if (xftab_g)
         tab_g = mclTabRead(xftab_g, mx->dom_cols, EXIT_ON_FAIL)
      ;  mcxIOclose(xfmx_g)
   ;  }
      else
      mcxDie(1, me, "-imx <fname> or -abc <fname> required")

   ;  hsh_g = tab_g ? mclTabHash(tab_g) : NULL
   ;  mclxAdjustLoops(mx, mclxLoopCBremove, NULL)

   ;  if (ceil_g)
      {  sel = mclgCeilNB(mx, ceil_g, &n_hub, &n_in, &n_out)
      ;  mclvFree(&sel)
   ;  }
      
      if (xq_g)
      mx = process_queries(xq_g, mx, xfmx_g, tab_g)

   ;  mcxIOfree(&xftab_g)
   ;  mcxIOfree(&xfmx_g)
   ;  mcxIOfree(&xq_g)
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


