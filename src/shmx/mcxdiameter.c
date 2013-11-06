/*   (C) Copyright 2006, 2007, 2008, 2009 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
 *
 * NOTE
 * this file implements both /diameter/ and /ctty/ (centrality)
 * functionality for mcx.
*/

/* TODO
 * -  Handle components.
 * -  Clean up diameter/ctty organization.
*/


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
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
#include "util/ding.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/compile.h"

#include "impala/matrix.h"
#include "impala/tab.h"
#include "impala/io.h"
#include "impala/stream.h"

#include "clew/clm.h"
#include "gryphon/path.h"

static const char* mediam = "mcx diameter";
static const char* mectty = "mcx ctty";


         /* this aids in finding heuristically likely starting points
          * for long shortest paths, by looking at dead ends
          * in the lattice.
          * experimental and oefully underdocumented.
         */
static dim diameter_rough
(  mclv*       vec
,  mclx*       mx
,  u8*         rough_scratch
,  long*       rough_priority
)
   {  mclv* curr  =  mclvInsertIdx(NULL, vec->vid, 1.0) 
   ;  mclpAR* par =  mclpARensure(NULL, 1024)

   ;  dim d = 0, n_dead_ends = 0, n_dead_ends_res = 0

   ;  memset(rough_scratch, 0, N_COLS(mx))

   ;  rough_scratch[vec->vid] = 1                        /* seen */
   ;  rough_priority[vec->vid] = -1              /* remove from priority list */

   ;  while (1)
      {  mclp* currivp = curr->ivps
      ;  dim t
      ;  mclpARreset(par)
      ;  while (currivp < curr->ivps + curr->n_ivps)
         {  mclv* ls = mx->cols+currivp->idx
         ;  mclp* newivp = ls->ivps
         ;  int hit = 0
         ;  while (newivp < ls->ivps + ls->n_ivps)
            {  u8* tst = rough_scratch+newivp->idx
            ;  if (!*tst || *tst & 2)
               {  if (!*tst)
                  mclpARextend(par, newivp->idx, 1.0)
               ;  *tst = 2
               ;  hit = 1
            ;  }
               newivp++
         ;  }
            if (!hit && rough_priority[currivp->idx] >= 0)
               rough_priority[currivp->idx] += d+1
            ,  n_dead_ends_res++
         ;  else if (!hit)
            n_dead_ends++
/* ,fprintf(stderr, "[%ld->%ld]", (long) currivp->idx, (long) rough_priority[currivp->idx])
*/
;
#if 0
if (currivp->idx == 115 || currivp->idx == 128)
fprintf(stdout, "pivot %d node %d d %d dead %d pri %d\n", (int) vec->vid, (int) currivp->idx, d, (int) (1-hit), (int) rough_priority[currivp->idx])
#endif
         ;  currivp++
      ;  }
         if (!par->n_ivps)
         break
      ;  d++
      ;  mclvFromIvps(curr, par->ivps, par->n_ivps)
      ;  for (t=0;t<curr->n_ivps;t++)
         rough_scratch[curr->ivps[t].idx] = 1
   ;  }

      mclvFree(&curr)
   ;  mclpARfree(&par)
;if(0)fprintf(stdout, "deadends %d / %d\n", (int) n_dead_ends, (int) n_dead_ends_res)
   ;  return d
;  }


typedef struct
{  ofs      node_idx
;  dim      n_paths_lft
;  double   acc_wtd_global
;  double   acc_wtd_local  /* accumulator, weighted */
;  dim      acc_chr        /* accumulator, characteristic */
;  mcxLink* nb_lft         /* list of neighbours, NULL terminated */
;  mcxLink* nb_rgt         /* list of neighbours, NULL terminated */
;
}  SSPnode  ;


enum
{  MY_OPT_ABC    =   MCX_DISP_UNUSED
,  MY_OPT_IMX
,  MY_OPT_TAB
,  MY_OPT_LIST_MAX
,  MY_OPT_LIST_NODES
,  MY_OPT_T
,  MY_OPT_START
,  MY_OPT_END
,  MY_OPT_INCLUDE_ENDS
,  MY_OPT_MOD
,  MY_OPT_ROUGH
}  ;


mcxOptAnchor diameterOptions[] =
{  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "specify input matrix/graph"
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
,  {  "-t"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_T
   ,  "<int>"
   ,  "number of threads to use"
   }
,  {  "-start"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_START
   ,  "<int>"
   ,  "start index (inclusive)"
   }
,  {  "-end"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_END
   ,  "<int>"
   ,  "end index (exclusive)"
   }
,  {  "--rough"
   ,  MCX_OPT_HIDDEN
   ,  MY_OPT_ROUGH
   ,  NULL
   ,  "use direct computation (testing only)"
   }
,  {  "--summary"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LIST_MAX
   ,  NULL
   ,  "return length of longest shortest path"
   }
,  {  "--list"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LIST_NODES
   ,  NULL
   ,  "list eccentricity for all nodes (default)"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


mcxOptAnchor cttyOptions[] =
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
,  {  "-t"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_T
   ,  "<int>"
   ,  "number of threads to use"
   }
,  {  "-start"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_START
   ,  "<int>"
   ,  "start index (inclusive)"
   }
,  {  "-end"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_END
   ,  "<int>"
   ,  "end index (exclusive)"
   }
,  {  "--with-ends"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_INCLUDE_ENDS
   ,  NULL
   ,  "include scores for lattice sources and sinks"
   }
,  {  "--list"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LIST_NODES
   ,  NULL
   ,  "(default) list mode"
   }
,  {  "--mod"
   ,  MCX_OPT_HIDDEN
   ,  MY_OPT_MOD
   ,  NULL
   ,  "add 1 to each sub-score"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;

static  unsigned debug_g   =   -1;

static  mcxIO* xfmx_g      =   (void*) -1;
static  mcxIO* xfabc_g     =   (void*) -1;
static  mcxIO* xftab_g     =   (void*) -1;
static  mclTab* tab_g      =   (void*) -1;
static  dim progress_g     =   -1;
static  mcxbool rough      =   -1;
static  mcxbool list_nodes =   -1;
static  mcxbool mod_up     =   -1;

static mcxbool include_ends_g  =  -1;
static dim start_g         =  -1;
static dim end_g           =  -1;
static dim n_thread_g      =  -1;

static mcxstatus allInit
(  void
)
   {  xfmx_g         =  mcxIOnew("-", "r")
   ;  xfabc_g        =  NULL
   ;  xftab_g        =  NULL
   ;  tab_g          =  NULL
   ;  progress_g     =  0
   ;  rough          =  FALSE
   ;  list_nodes     =  TRUE
   ;  mod_up         =  FALSE
   ;  start_g        =  0
   ;  end_g          =  0
   ;  n_thread_g     =  0
   ;  include_ends_g =  FALSE
   ;  debug_g        =  0
   ;  return STATUS_OK
;  }


static mcxstatus cttyArgHandle
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

         case MY_OPT_INCLUDE_ENDS
      :  include_ends_g = TRUE
      ;  break
      ;

         case MY_OPT_LIST_NODES
      :  break
      ;

         case MY_OPT_T
      :  n_thread_g = (unsigned) atoi(val)
      ;  break
      ;

         case MY_OPT_END
      :  end_g = atoi(val)
      ;  break
      ;

         case MY_OPT_START
      :  start_g = atoi(val)
      ;  break
      ;

         case MY_OPT_MOD
      :  mod_up = TRUE
      ;  break
      ;

         default
      :  mcxExit(1)
      ;
   ;  }
      return STATUS_OK
;  }


static mcxstatus diameterArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  mcxIOnewName(xfmx_g, val)
      ;  break
      ;

         case MY_OPT_LIST_NODES
      :  list_nodes = TRUE
      ;  break
      ;

         case MY_OPT_T
      :  n_thread_g = (unsigned) atoi(val)
      ;  break
      ;

         case MY_OPT_END
      :  end_g = atoi(val)
      ;  break
      ;

         case MY_OPT_START
      :  start_g = atoi(val)
      ;  break
      ;

         case MY_OPT_LIST_MAX
      :  list_nodes = FALSE
      ;  break
      ;

         case MY_OPT_ROUGH
      :  rough = TRUE
      ;  break
      ;

         case MY_OPT_TAB
      :  xftab_g = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_ABC
      :  xfabc_g = mcxIOnew(val, "r")
      ;  break
      ;

         default
      :  mcxExit(1) 
      ;
   ;  }
      return STATUS_OK
;  }


static void rough_it
(  mclx* mx
,  dim* tabulator
,  dim i
,  u8* rough_scratch
,  long* rough_priority
,  dim* pri
)
   {  dim dd = 0, p = i, p1 = 0, p2 = 0, priority = 0, p1p2 = 0, j = 0
      
   ;  for (j=0;j<N_COLS(mx);j++)
      {  if (1)
         {  if (rough_priority[j] >= rough_priority[p1])
               p2 = p1
            ,  p1 = j
         ;  else if (rough_priority[j] >= rough_priority[p2])
            p2 = j
      ;  }
         else
         {  if (!p1 && rough_priority[j] >= 1)
            p1 = j
      ;  }
      }
      p = p1
   ;  priority = rough_priority[p]
   ;  p1p2 = rough_priority[p1] + rough_priority[p2]
   ;  dd = diameter_rough(mx->cols+p, mx, rough_scratch, rough_priority)
;if (0)
fprintf
(  stdout
,  "guard %6d--%-6d %6d %6d NODE %6d ECC %6d PRI\n"
,  (int) p1p2
,  (int) (i * dd)
,  (int) i
,  (int) p
,  (int) dd
,  (int) priority
)
   ;  *pri = priority
   ;  tabulator[p] = dd
;  }


static void ecc_compute
(  dim* tabulator
,  const mclx* mx
,  dim offset
,  dim inc
,  mclv* scratch
)
   {  dim i
   ;  for (i=offset;i<end_g;i+=inc)
      {  dim dd = mclgEcc2(mx->cols+i, mx, scratch)
      ;  tabulator[i] = dd
   ;  }
   }


typedef struct
{  dim      offset
;  dim      inc
;  const    mclx* mx
;  dim*     tabulator
;  mclv*    scratch
;
}  diam_data   ;


static void* diam_thread
(  void* arg
)
   {  diam_data* data = arg
   ;  ecc_compute(data->tabulator, data->mx, data->offset, data->inc, data->scratch)
   ;  return NULL
;  }


static mcxstatus diameterMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)
   {  mclx* mx                =  NULL
   ;  mcxbool canonical       =  FALSE

   ;  u8* rough_scratch       =  NULL
   ;  long* rough_priority    =  NULL
   ;  dim* tabulator          =  NULL

   ;  mclv* ecc_scratch       =  NULL

   ;  double sum = 0.0

   ;  dim thediameter = 0
   ;  dim i

   ;  mclxIOstreamer streamer = { 0 }

   ;  progress_g  =  mcx_progress_g
   ;  debug_g     =  mcx_debug_g

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

      if (!end_g || end_g > N_COLS(mx))
      end_g = N_COLS(mx)

   ;  if (n_thread_g < 1)
      n_thread_g = 1

   ;  rough_scratch  =  calloc(N_COLS(mx), sizeof rough_scratch[0])
   ;  rough_priority =  mcxAlloc(N_COLS(mx) * sizeof rough_priority[0], EXIT_ON_FAIL)
   ;  tabulator      =  calloc(N_COLS(mx), sizeof tabulator[0])
   ;  ecc_scratch    =  mclvCopy(NULL, mx->dom_rows)
                                       /* ^ used as ecc scratch: should have values 1.0 */

   ;  for (i=0;i<N_COLS(mx);i++)
      rough_priority[i] = 0

   ;  canonical = MCLV_IS_CANONICAL(mx->dom_cols)

   ;  if (rough && !mclxGraphCanonical(mx))
      mcxDie(1, mediam, "rough needs canonical domains")

   ;  if (rough)
      {  for (i=0;i<N_COLS(mx);i++)
         {  dim priority = 0
         ;  rough_it(mx, tabulator, i, rough_scratch, rough_priority, &priority)
      ;  }
      }
      else if (n_thread_g == 1)
      ecc_compute(tabulator, mx, start_g, 1, ecc_scratch)
   ;  else
      {  dim t
      ;  mclx* scratchen            /* annoying UNIXen-type plural */
         =  mclxCartesian(mclvCanonical(NULL, n_thread_g, 1.0), mclvClone(mx->dom_rows), 1.0)
      ;  pthread_t *threads_diam
         =  mcxAlloc(n_thread_g * sizeof threads_diam[0], EXIT_ON_FAIL)
      ;  pthread_attr_t  t_attr
      ;  pthread_attr_init(&t_attr)
      ;  for (t=0;t<n_thread_g;t++)
         {  diam_data* data = mcxAlloc(sizeof data[0], EXIT_ON_FAIL)
         ;  data->offset   =  start_g + t
         ;  data->inc      =  n_thread_g
         ;  data->mx       =  mx
         ;  data->scratch  =  scratchen->cols+t
         ;  data->tabulator = tabulator
         ;  if (pthread_create(threads_diam+t, &t_attr, diam_thread, data))
            mcxDie(1, mectty, "error creating thread %d", (int) t)
      ;  }
         for (t=0; t < n_thread_g; t++)
         {  pthread_join(threads_diam[t], NULL)
      ;  }
         mcxFree(threads_diam)
      ;  mclxFree(&scratchen)
   ;  }

      for (i=0;i<N_COLS(mx);i++)    /* report everything so that results can be collected */
      {  dim dd = tabulator[i]
      ;  sum += dd

      ;  if (list_nodes)
         {  long vid = mx->cols[i].vid
         ;  if (tab_g)
            {  const char* label = mclTabGet(tab_g, vid, NULL)
            ;  if (!label) mcxDie(1, mediam, "panic label %ld not found", vid)
            ;  fprintf(stdout, "%s\t%ld\n", label, (long) dd)
         ;  }
            else
            fprintf(stdout, "%ld\t%ld\n", vid, (long) dd)
      ;  }

         if (dd > thediameter)
         thediameter = dd
   ;  }

      if (!list_nodes && N_COLS(mx))
      {  sum /= N_COLS(mx)
      ;  fprintf
         (  stdout
         ,  "%d\t%.3f\n"
         ,  (unsigned) thediameter
         ,  (double) sum
         )
   ;  }

      return 0
;  }


mcxDispHook* mcxDispHookDiameter
(  void
)
   {  static mcxDispHook diameterEntry
   =  {  "diameter"
      ,  "diameter [options]"
      ,  diameterOptions
      ,  sizeof(diameterOptions)/sizeof(mcxOptAnchor) - 1

      ,  diameterArgHandle
      ,  allInit
      ,  diameterMain

      ,  0
      ,  0
      ,  MCX_DISP_MANUAL
      }
   ;  return &diameterEntry
;  }


void clean_up
(  SSPnode*    nodes
,  mclx*       pathmx
)
   {  dim ofs

   ;  for (ofs=0;ofs<N_COLS(pathmx);ofs++)
      {  dim i
      ;  mclv* layer = pathmx->cols+ofs
      ;  for (i=0;i<layer->n_ivps;i++)
         {  SSPnode* nd = nodes+layer->ivps[i].idx
         ;  mcxLink* lk = nd->nb_lft->next
         ;  nd->acc_wtd_local = 0
         ;  nd->n_paths_lft = 0
         ;  while (lk)
            {  mcxLink* lk2 = lk->next
            ;  mcxLinkRemove(lk)
            ;  lk = lk2
         ;  }
            lk = nd->nb_rgt->next
         ;  while (lk)
            {  mcxLink* lk2 = lk->next
            ;  mcxLinkRemove(lk)
            ;  lk = lk2
         ;  }
            nd->nb_lft->next = NULL
         ;  nd->nb_rgt->next = NULL
      ;  }
      }
   }


void add_scores
(  SSPnode*    nodes
,  mclx*       pathmx
)
   {  dim ofs
   ;  double delta = mod_up ? 0.0 : 1.0

   ;  for (ofs=0;ofs<N_COLS(pathmx);ofs++)
      {  dim i
      ;  mclv* layer = pathmx->cols+ofs
      ;  for (i=0;i<layer->n_ivps;i++)
         {  SSPnode* nd = nodes+layer->ivps[i].idx
         ;  if
            (  (include_ends_g || (nd->nb_lft->next && nd->nb_rgt->next))
            && nd->acc_wtd_local >= delta
            )
            nd->acc_wtd_global += (nd->acc_wtd_local - delta)
      ;  }
      }
   }


void compute_scores_up
(  SSPnode*    nodes
,  mclx*       pathmx
)
   {  dim ofs

   ;  for (ofs=1;ofs<N_COLS(pathmx);ofs++)
      {  dim i
      ;  mclv* layer = pathmx->cols+N_COLS(pathmx)-ofs
      ;  for (i=0;i<layer->n_ivps;i++)
         {  SSPnode* nd = nodes+layer->ivps[i].idx
         ;  mcxLink* lk = nd->nb_lft

         ;  while ((lk = lk->next))
            {  SSPnode* nd2 = lk->val
            ;  nd2->acc_wtd_local
               +=  nd->acc_wtd_local * nd2->n_paths_lft * 1.0 / nd->n_paths_lft
         ;  }
         }
      }
   }



void compute_paths_down
(  SSPnode*    nodes
,  mclx*       pathmx
)
   {  dim ofs

   ;  for (ofs=0;ofs<N_COLS(pathmx);ofs++)
      {  dim i
      ;  mclv* layer = pathmx->cols+ofs
;if (debug_g) fprintf(stdout, "%d nodes in layer %d\n", (int) layer->n_ivps, (int) ofs)
      ;  for (i=0;i<layer->n_ivps;i++)
         {  SSPnode* nd = nodes+layer->ivps[i].idx
         ;  mcxLink* lk = nd->nb_rgt

         ;  nd->acc_wtd_local = 1.0

         ;  if (!nd->n_paths_lft)
            nd->n_paths_lft = 1

         ;  while ((lk = lk->next))
            {  SSPnode* nd2 = lk->val
            ;  nd2->n_paths_lft += nd->n_paths_lft
         ;  }
         }
      }
   }


void lattice_print_nodes
(  mclx*    pathmx
,  SSPnode* nodes
)
   {  dim l
   ;  fprintf(stdout, "nodes: lattice has %ld layers\n", (long) N_COLS(pathmx))
   ;  for (l=0;l<N_COLS(pathmx);l++)
      {  dim i
      ;  mclv* layer = pathmx->cols+l
      ;  fprintf(stdout, "size=%ld", (long) layer->n_ivps)
      ;  for (i=0;i<layer->n_ivps;i++)
         {  SSPnode* nd = nodes + layer->ivps[i].idx
         ;  fprintf
            (  stdout
            ,  " %ld-(%ld)/%.2f"
            ,  (long) nd->node_idx
            ,  (long) nd->n_paths_lft
            ,  nd->acc_wtd_local
            )
      ;  }
         fputc('\n', stdout)
   ;  }
;  }



void lattice_print_edges
(  mclx*    pathmx
,  SSPnode* nodes
)
   {  dim l
   ;  fprintf(stdout, "edges: lattice has %ld layers\n", (long) N_COLS(pathmx))

   ;  for (l=0;l<N_COLS(pathmx);l++)
      {  mclv* layer = pathmx->cols+l
      ;  dim i
      ;  fprintf(stdout, "size=%ld", (long) layer->n_ivps)
      ;  for (i=0;i<layer->n_ivps;i++)
         {  SSPnode* nd = nodes + layer->ivps[i].idx
         ;  mcxLink* lk = nd->nb_rgt->next
         ;  while (lk)
            {  SSPnode* nd2 = lk->val
            ;  fprintf(stdout, " (%ld %ld)", (long) nd->node_idx, (long) nd2->node_idx)
            ;  lk = lk->next
         ;  }
         }
         fputc('\n', stdout)
   ;  }
;  }



void mclgSSPmakeLattice
(  mclx* pathmx
,  const mclx* mx
,  SSPnode* nodes
)
   {  dim i
#if 0
   ;  for (i=0;i<N_COLS(pathmx);i++)
      mclvaDump(pathmx->cols+i, stdout, MCLXIO_VALUE_GETENV, " ", 0)
#endif

   ;  for (i=0;i<N_COLS(pathmx);i++)
      {  dim v = 0
      ;  mclv* tails = pathmx->cols+i
      ;  mclv* heads = i < N_COLS(pathmx)-1 ? pathmx->cols+i+1 : NULL
      ;  mclv* relevant = mclvInit(NULL)

      ;  for (v=0;v<tails->n_ivps;v++)
         {  dim j
         ;  ofs lft_idx = tails->ivps[v].idx
         ;  mclv* nb = mclxGetVector(mx, lft_idx, EXIT_ON_FAIL, NULL)
         ;  if (heads)
            mcldMeet(nb, heads, relevant)

         ;  for (j=0;j<relevant->n_ivps;j++)
            {  ofs rgt_idx = relevant->ivps[j].idx
            ;  mcxLinkAfter(nodes[lft_idx].nb_rgt, nodes+rgt_idx)
            ;  mcxLinkAfter(nodes[rgt_idx].nb_lft, nodes+lft_idx)
         ;  }
         }
         mclvFree(&relevant)
   ;  }
   }



mclx* cttyFlood
(  const mclx* mx
,  SSPnode* nodes_unused   cpl__unused
,  ofs root
)
   {  mclv* wave
   ;  mclx* pathmx = mclxAllocClone(mx)
   ;  mclv* scratch = mclvCopy(NULL, mx->dom_cols)

   ;  dim n_wave = 0

   ;  mclvInsertIdx(pathmx->cols+0, root, 1.0)
   ;  mclgUnionvInitNode2(scratch, root)
   ;  wave = mclvClone(pathmx->cols+0)
   ;  n_wave = 1

   ;  while (1)
      {  mclv* wave2 = mclgUnionv2(mx, wave, NULL, SCRATCH_UPDATE, NULL, scratch)
; if (debug_g)
mclvaDump(wave2, stdout, MCLXIO_VALUE_GETENV, " ", MCLVA_DUMP_VID_OFF)
      ;  if (wave2->n_ivps)
         {  mclvFree(&wave)
         ;  wave = wave2
         ;  mclvCopy(pathmx->cols+n_wave, wave2)
         ;  n_wave++
      ;  }
         else
         {  mclvFree(&wave2)
         ;  break
      ;  }
      }

      mclvFree(&wave)
   ;  mclvResize(pathmx->dom_cols, n_wave)
   ;  mclvFree(&scratch)
   ;  return pathmx
;  }


static void ctty_compute
(  const mclx* mx
,  mclv*       ctty
,  dim         offset
,  dim         inc
)  
   {  SSPnode* nodes    =  mcxAlloc(N_COLS(mx) * sizeof nodes[0], EXIT_ON_FAIL)
   ;  mcxLink* src_link =  mcxListSource(N_COLS(mx), 0)
   ;  dim i

   ;  if (offset < start_g)
      mcxDie(1, mectty, "offset error")

   ;  for (i=0;i<N_COLS(mx);i++)
      {  nodes[i].node_idx =  i
      ;  nodes[i].acc_chr  =  0
      ;  nodes[i].acc_wtd_global  =  0.0
      ;  nodes[i].acc_wtd_local = 0.0
      ;  nodes[i].nb_lft   =  mcxLinkSpawn(src_link, NULL)
      ;  nodes[i].nb_rgt   =  mcxLinkSpawn(src_link, NULL)
      ;  nodes[i].n_paths_lft = 0
   ;  }

      for (i=offset;i<end_g;i+=inc)
      {  long vid = mx->cols[i].vid
      ;  mclx* pathmx = cttyFlood(mx, nodes, vid)

      ;  mclgSSPmakeLattice(pathmx, mx, nodes)
      ;  if (debug_g)
            fprintf(stdout, "\nroot %ld\n", vid)
         ,  lattice_print_edges(pathmx, nodes)

      ;  compute_paths_down(nodes, pathmx)

      ;  compute_scores_up(nodes, pathmx)
      ;  add_scores(nodes, pathmx)
      ;  if (debug_g)
         lattice_print_nodes(pathmx, nodes)
      ;  clean_up(nodes, pathmx)
      ;  if (progress_g && !((i+1)% progress_g))
         fputc('.', stderr)
      ;  mclxFree(&pathmx)
   ;  }
      if (progress_g && end_g - start_g >= progress_g)
      fputc('\n', stderr)

   ;  for (i=0;i<N_COLS(mx);i++)
      {  ctty->ivps[i].val += nodes[i].acc_wtd_global
   ;  }

      mcxFree(nodes)
   ;  mcxListFree(&src_link, NULL)
;  }


typedef struct
{  dim      offset
;  dim      inc
;  const    mclx* mx
;  mclv*    ctty
;
}  ctty_data   ;


static void* ctty_thread
(  void* arg
)
   {  ctty_data* data = arg
   ;  ctty_compute(data->mx, data->ctty, data->offset, data->inc)
   ;  return NULL
;  }



static mcxstatus cttyMain
(  int          argc_unused      cpl__unused
,  const char*  argv_unused[]    cpl__unused
)  
   {  mclx* mx, *ctty
   ;  mclxIOstreamer streamer = { 0 }
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
      {  mx = mclxReadx(xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH | MCLX_REQUIRE_CANONICAL)
      ;  if (xftab_g)
         tab_g = mclTabRead(xftab_g, mx->dom_cols, EXIT_ON_FAIL)
   ;  }

      mcxIOfree(&xfmx_g)

   ;  if (n_thread_g < 1)
      n_thread_g = 1

   ;  ctty
      =  mclxCartesian(mclvCanonical(NULL, n_thread_g, 1.0), mclvClone(mx->dom_rows), 0.0)

   ;  debug_g     =  mcx_debug_g
   ;  progress_g  =  mcx_progress_g

   ;  if (!end_g || end_g > N_COLS(mx))
      end_g = N_COLS(mx)

   ;  if (n_thread_g == 1)
      ctty_compute(mx, ctty->cols+0, start_g, 1)
   ;  else
      {  dim t
      ;  pthread_t *threads_ctty
         =  mcxAlloc(n_thread_g * sizeof threads_ctty[0], EXIT_ON_FAIL)
      ;  pthread_attr_t  t_attr
      ;  pthread_attr_init(&t_attr)
      ;  for (t=0;t<n_thread_g;t++)
         {  ctty_data* data = mcxAlloc(sizeof data[0], EXIT_ON_FAIL)
         ;  data->offset   =  start_g + t
         ;  data->inc      =  n_thread_g
         ;  data->mx       =  mx
         ;  data->ctty     =  ctty->cols+t
         ;  if (pthread_create(threads_ctty+t, &t_attr, ctty_thread, data))
            mcxDie(1, mectty, "error creating thread %d", (int) t)
      ;  }
         for (t=0; t < n_thread_g; t++)
         {  pthread_join(threads_ctty[t], NULL)
      ;  }
         mcxFree(threads_ctty)
   ;  }

      if (ctty->cols[0].n_ivps != N_COLS(mx))
      mcxDie(1, mectty, "internal error, tabulator miscount")

                     /* add subtotals to first vector */
   ;  for (i=1;i<N_COLS(ctty);i++)
      {  mclv* vec = ctty->cols+i
      ;  dim j
      ;  if (vec->n_ivps != N_COLS(mx))
         mcxDie(1, mectty, "internal error, tabulator miscount")
      ;  for (j=0;j<vec->n_ivps;j++)
         ctty->cols[0].ivps[j].val += vec->ivps[j].val
   ;  }

                     /* and report first vector, optionally with labels */
      {  mclv* v = ctty->cols+0
      ;  for (i=0;i<v->n_ivps;i++)
         {  double ctt  =  v->ivps[i].val
         ;  long  vid   =  v->ivps[i].idx
         ;  if (tab_g)
            {  const char* label = mclTabGet(tab_g, vid, NULL)
            ;  if (!label) mcxDie(1, mectty, "panic label %ld not found", vid)
            ;  fprintf(stdout, "%s\t%.8g\n", label, ctt)
         ;  }
            else
            fprintf(stdout, "%ld\t%.8g\n",  vid, ctt)
      ;  }
      }

      return 0
;  }


mcxDispHook* mcxDispHookCtty
(  void
)
   {  static mcxDispHook cttyEntry
   =  {  "ctty"
      ,  "ctty [options]"
      ,  cttyOptions
      ,  sizeof(cttyOptions)/sizeof(mcxOptAnchor) - 1

      ,  cttyArgHandle
      ,  allInit
      ,  cttyMain

      ,  0
      ,  0
      ,  MCX_DISP_MANUAL
      }
   ;  return &cttyEntry
;  }




