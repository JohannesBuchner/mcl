/*   (C) Copyright 2006, 2007 Stijn van Dongen
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#include "mcx.h"

#include "impala/matrix.h"
#include "impala/tab.h"
#include "impala/io.h"
#include "impala/stream.h"

#include "util/types.h"
#include "util/ding.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/opt.h"

#include "clew/clm.h"
#include "gryphon/path.h"


dim diameter_rough
(  mclv*       vec
,  mclx*       mx
,  u8*         scratch
,  long*       scratch_priority
)
   {  mclv* curr  =  mclvInsertIdx(NULL, vec->vid, 1.0) 
   ;  mclpAR* par =  mclpARensure(NULL, 1024)

   ;  dim d = 0, n_dead_ends = 0, n_dead_ends_res = 0

   ;  memset(scratch, 0, N_COLS(mx))

   ;  scratch[vec->vid] = 1                        /* seen */
   ;  scratch_priority[vec->vid] = -1              /* remove from priority list */

   ;  while (1)
      {  mclp* currivp = curr->ivps
      ;  dim t
      ;  mclpARreset(par)
      ;  while (currivp < curr->ivps + curr->n_ivps)
         {  mclv* ls = mx->cols+currivp->idx
         ;  mclp* newivp = ls->ivps
         ;  int hit = 0
         ;  while (newivp < ls->ivps + ls->n_ivps)
            {  u8* tst = scratch+newivp->idx
            ;  if (!*tst || *tst & 2)
               {  if (!*tst)
                  mclpARextend(par, newivp->idx, 1.0)
               ;  *tst = 2
               ;  hit = 1
            ;  }
               newivp++
         ;  }
            if (!hit && scratch_priority[currivp->idx] >= 0)
               scratch_priority[currivp->idx] += d+1
            ,  n_dead_ends_res++
         ;  else if (!hit)
            n_dead_ends++
/* ,fprintf(stderr, "[%ld->%ld]", (long) currivp->idx, (long) scratch_priority[currivp->idx])
*/
;
#if 0
if (currivp->idx == 115 || currivp->idx == 128)
fprintf(stdout, "pivot %d node %d d %d dead %d pri %d\n", (int) vec->vid, (int) currivp->idx, d, (int) (1-hit), (int) scratch_priority[currivp->idx])
#endif
         ;  currivp++
      ;  }
         if (!par->n_ivps)
         break
      ;  d++
      ;  mclvFromIvps(curr, par->ivps, par->n_ivps)
      ;  for (t=0;t<curr->n_ivps;t++)
         scratch[curr->ivps[t].idx] = 1
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
,  MY_OPT_LIST_NODES
,  MY_OPT_X
,  MY_OPT_Y
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
,  {  "--rough"
   ,  MCX_OPT_HIDDEN
   ,  MY_OPT_ROUGH
   ,  NULL
   ,  "use direct computation (testing only)"
   }
,  {  "--list"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LIST_NODES
   ,  NULL
   ,  "list eccentricity for all nodes"
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
,  {  "-x"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_X
   ,  "<int>"
   ,  "start index"
   }
,  {  "-y"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_Y
   ,  "<int>"
   ,  "end index"
   }
,  {  "--with-ends"
   ,  MCX_OPT_DEFAULT
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
static  mclTab* tab_g      =   (void*) -1;
static  dim progress_g     =   -1;
static  mcxbool rough      =   -1;
static  mcxbool list_nodes =   -1;
static  mcxbool mod_up     =   -1;

static mcxbool include_ends_g  =  -1;
static ofs start_g      =  -1;
static ofs end_g        =  -1;

static mcxstatus allInit
(  void
)
   {  xfmx_g         =  mcxIOnew("-", "r")
   ;  xfabc_g        =  NULL
   ;  tab_g          =  NULL
   ;  progress_g     =  0
   ;  rough          =  FALSE
   ;  list_nodes     =  FALSE
   ;  mod_up         =  FALSE
   ;  start_g        =  0
   ;  end_g          =  0
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

         case MY_OPT_INCLUDE_ENDS
      :  include_ends_g = TRUE
      ;  break
      ;

         case MY_OPT_LIST_NODES
      :  break
      ;

         case MY_OPT_Y
      :  end_g = atoi(val)
      ;  break
      ;

         case MY_OPT_X
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

         case MY_OPT_ROUGH
      :  rough = TRUE
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


static mcxstatus diameterMain
(  int                  argc
,  const char*          argv[]
)
   {  mclx* mx                =  NULL
   ;  u8* scratch             =  NULL
   ;  long* scratch_priority  =  NULL
   ;  mcxbool canonical       =  FALSE

   ;  double sum = 0.0

   ;  dim d = 0
   ;  dim i

   ;  dim last[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, }
   ;  dim l        =  0
   ;  double last10 =  0.0

   ;  progress_g  =  mcx_progress_g
   ;  debug_g     =  mcx_debug_g

   ;  if (xfabc_g)
      mx
      =  mclxIOstreamIn
         (  xfabc_g
         ,  MCLXIO_STREAM_ABC | MCLXIO_STREAM_MIRROR
         ,  NULL
         ,  mclpMergeMax
         ,  &tab_g
         ,  NULL
         ,  EXIT_ON_FAIL
         )
   ;  else
      mx = mclxReadx(xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH)

   ;  canonical = MCLV_IS_CANONICAL(mx->dom_cols)
   ;  scratch   =  calloc(N_COLS(mx), sizeof scratch[0])

   ;  scratch_priority = malloc(N_COLS(mx) * sizeof scratch_priority[0])
   ;  for (i=0;i<N_COLS(mx);i++)
      scratch_priority[i] = 0

   ;  if (rough && !mclxGraphCanonical(mx))
      mcxDie(1, "mcxdiameter", "rough needs canonical domains")

   ;  for (i=0;i<N_COLS(mx);i++)
      {  dim dd = 0, p = i, p1 = 0, p2 = 0, priority = 0, p1p2 = 0
         
      ;  if (rough)
         {  dim j = 0
         ;  for (j=0;j<N_COLS(mx);j++)
            {  if (1)
               {  if (scratch_priority[j] >= scratch_priority[p1])
                     p2 = p1
                  ,  p1 = j
               ;  else if (scratch_priority[j] >= scratch_priority[p2])
                  p2 = j
            ;  }
               else
               {  if (!p1 && scratch_priority[j] >= 1)
                  p1 = j
            ;  }
            }
            p = p1
         ;  priority = scratch_priority[p]
         ;  p1p2 = scratch_priority[p1] + scratch_priority[p2]
         ;  dd = diameter_rough(mx->cols+p, mx, scratch, scratch_priority)
;if (0)
fprintf
(  stdout
,  "guard %6d--%-6d %6d %6d NODE %6d ECC %6d PRI\n"
,  p1p2
,  (int) (i * dd)
,  (int) i
,  (int) p
,  (int) dd
,  (int) priority
)
      ;  }
         else
         dd = mclgEcc(mx->cols+p, mx)

      ;  l = l % 10
      ;  last[l++] = dd
      ;  if (progress_g && !((i+1)%10))
         {  dim j
         ;  double sum10 = 0.0
         ;  for (j=0;j<10;j++)
            sum10 += last[j]
         ;  fputc( sum10 > last10 ? '+' : '-', stderr)
         ;  last10 = sum10
      ;  }


#if 0
;fprintf(stderr, " %d,%d", (int) dd, (int) priority)
#elif 0
;fprintf(stderr, " %d", (int) dd)
#endif
      ;  sum += dd

      ;  if (list_nodes)
         {  long vid = mx->cols[p].vid
         ;  if (tab_g)
            {  const char* label = mclTabGet(tab_g, vid, NULL)
            ;  if (!label) mcxDie(1, "diameter", "panic label %ld not found", vid)
            ;  fprintf(stdout, "%s\t%ld\n", label, (long) dd)
         ;  }
            else
            fprintf(stdout, "%ld\t%ld\n", vid, (long) dd)
      ;  }

         if (progress_g && dd > d)
         fprintf
         (  stderr
         ,  "ite %u pri %lu new max vec %u ecc %u\n"
         ,  (unsigned) i
         ,  (ulong) priority
         ,  (unsigned) mx->cols[p].vid
         ,  (unsigned) dd
         )

      ;  if (dd > d)
         d = dd

      ;  if (progress_g && !((i+1)% progress_g))
         fprintf
         (  stderr
         ,  "%u average %.3f\n"
         ,  (unsigned) (i+1)
         ,  (double) (sum/(i+1))
         )
   ;  }

      sum /= N_COLS(mx)
   ;  if (!list_nodes)
      fprintf
      (  stdout
      ,  "%d\t%.3f\n"
      ,  (unsigned) d
      ,  (double) sum
      )
   ;  return 0
;  }


static mcxDispHook diameterEntry
=  {  "diameter"
   ,  "diameter [options]"
   ,  diameterOptions
   ,  sizeof(diameterOptions)/sizeof(mcxOptAnchor) - 1

   ,  diameterArgHandle
   ,  allInit
   ,  diameterMain

   ,  0
   ,  0
   ,  MCX_DISP_DEFAULT
   }
;


mcxDispHook* mcxDispHookDiameter
(  void
)
   {  return &diameterEntry
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
,  mclx* mx
,  SSPnode* nodes
)
   {  dim i, i_layer = 0
#if 0
   ;  for (i=0;i<N_COLS(pathmx);i++)
      mclvaDump2(pathmx->cols+i, stdout, MCLXIO_VALUE_GETENV, " ", 0)
#endif

   ;  for (i=0;i<N_COLS(pathmx);i++)
      {  dim v = 0
      ;  mclv* tails = pathmx->cols+i
      ;  mclv* heads = i < N_COLS(pathmx)-1 ? pathmx->cols+i+1 : NULL
      ;  mclv* relevant = mclvInit(NULL)

      ;  for (v=0;v<tails->n_ivps;v++)
         {  dim j
         ;  ofs lft_idx = tails->ivps[v].idx
;if(0)fprintf(stderr, "%ld\n", lft_idx)
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
(  mclx* mx
,  SSPnode* nodes
,  ofs root
)
   {  mclv* wave
   ;  mclx* pathmx = mclxAllocClone(mx)

   ;  dim n_wave = 0

   ;  mclvInsertIdx(pathmx->cols+0, root, 1.0)
   ;  mclgUnionvInitNode(mx, root)
   ;  wave = mclvClone(pathmx->cols+0)
   ;  n_wave = 1

   ;  while (1)
      {  dim i
      ;  mclv* wave2 = mclgUnionv(mx, wave, NULL, SCRATCH_UPDATE, NULL)
; if (debug_g)
mclvaDump2(wave2, stdout, MCLXIO_VALUE_GETENV, " ", MCLVA_DUMP_VID_NO)
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
   ;  mclgUnionvReset(mx)
   ;  return pathmx
;  }


   /* TODO: different counting modes:
   */

static mcxstatus cttyMain
(  int          argc
,  const char*  argv[]
)  
   {  mclx* mx
   ;  SSPnode* nodes
   ;  mcxLink* src_link

   ;  if (xfabc_g)
      mx
      =  mclxIOstreamIn
         (  xfabc_g
         ,  MCLXIO_STREAM_ABC | MCLXIO_STREAM_MIRROR
         ,  NULL
         ,  mclpMergeMax
         ,  &tab_g
         ,  NULL
         ,  EXIT_ON_FAIL
         )
   ;  else
      mx = mclxReadx(xfmx_g, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH | MCLX_REQUIRE_CANONICAL)

   ;  nodes    =  mcxAlloc(N_COLS(mx) * sizeof nodes[0], EXIT_ON_FAIL)
   ;  src_link =  mcxListSource(N_COLS(mx), 0)

   ;  debug_g  =  mcx_debug_g
   ;  progress_g =mcx_progress_g

   ;  dim i
   ;  if (!end_g)
      end_g = N_COLS(mx)

   ;  for (i=0;i<N_COLS(mx);i++)
      {  nodes[i].node_idx =  i
      ;  nodes[i].acc_chr  =  0
      ;  nodes[i].acc_wtd_global  =  0.0
      ;  nodes[i].acc_wtd_local = 0.0
      ;  nodes[i].nb_lft   =  mcxLinkSpawn(src_link, NULL)
      ;  nodes[i].nb_rgt   =  mcxLinkSpawn(src_link, NULL)
      ;  nodes[i].n_paths_lft = 0
   ;  }

      for (i=start_g;i<end_g;i++)
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
      {  long vid = mx->cols[i].vid
      ;  double ctt = nodes[i].acc_wtd_global
      ;  if (tab_g)
         {  const char* label = mclTabGet(tab_g, vid, NULL)
         ;  if (!label) mcxDie(1, "ctty", "panic label %ld not found", vid)
         ;  fprintf(stdout, "%s\t%.2f\n", label, ctt)
      ;  }
         else
         fprintf(stdout, "%ld\t%.2f\n",  vid, ctt)
   ;  }

      mcxFree(nodes)
   ;  mcxListFree(&src_link, NULL)
   ;  mclxFree(&mx)
   ;  mcxIOfree(&xfmx_g)
   ;  return 0
;  }


static mcxDispHook cttyEntry
=  {  "ctty"
   ,  "ctty [options]"
   ,  cttyOptions
   ,  sizeof(cttyOptions)/sizeof(mcxOptAnchor) - 1

   ,  cttyArgHandle
   ,  allInit
   ,  cttyMain

   ,  0
   ,  0
   ,  MCX_DISP_DEFAULT
   }
;


mcxDispHook* mcxDispHookCtty
(  void
)
   {  return &cttyEntry
;  }




