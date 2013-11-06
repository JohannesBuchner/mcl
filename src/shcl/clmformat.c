/*      Copyright (C) 2003, 2004, 2005 Stijn van Dongen
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

#include "report.h"
#include "clmformat.zmm.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/tab.h"
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/scan.h"
#include "impala/app.h"

#include "mcl/interpret.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/minmax.h"

/*
 *    how about using key-val encoding: {{sz}{3}{nb}{2}{cv}{2-3}} ?
 *    (some extra processing time; negligible ?)
*/

/*
 * TODO:
 *
 *    are sparse domains supported?
 *    account for overlap (nightmares).
 *    chunked indexes.
 *    --split option is obsolete
 *
 *    index file:
 *    possibly some sample nodes from each cluster. (ideally these would
 *    be as disparate as possible).
 *    Append index.html to fmt base, so foo-index.html or foo/index.html
 *       futurefeature: implement disparacy maximizing algorithm.
 *       ff: user-time reselection of random nodes.
 *       ff: cluster/graph/node context, on the fly popup options.
 *
 *    / Are single element clusters done right ?
 *
 *    It might be nice to show node/node edge weights, but that will
 *    require an extra layer of design.
 *    Vision: current focus on cluster/node context.
 *       cluster/graph/node context
 *       cluster1/cluster2/node context require interface design first. 
*/

/* Checklist for cluster/graph properties.
 *    defective clustering (overlap, missing nodes, empty clusters)
 *    sparse cluster (column) domain
 *    sparse node domain (cluster rows, graph rows/cols)
 *    graph node domain subsumes cluster node domain.
 *    cluster node domain subsumes graph node domain.
 *    graph/cluster node domains induce tri-sphere.
*/


const char* me = "clmformat";
const char* syntax = "Usage: clmformat <options>";

enum
{  MY_OPT_ICL        =  0
,  MY_OPT_IMX
,  MY_OPT_TAB

,  MY_OPT_PI         =  MY_OPT_TAB + 2

,  MY_OPT_ADAPT
,  MY_OPT_SUBGRAPH
,  MY_OPT_LAZY

,  MY_OPT_ZMM        =  MY_OPT_LAZY + 2
,  MY_OPT_FMT

,  MY_OPT_DIR        =  MY_OPT_FMT + 2
,  MY_OPT_INFIX
,  MY_OPT_FANCY
,  MY_OPT_LUMP_COUNT

,  MY_OPT_DUMP       =  MY_OPT_LUMP_COUNT + 2
,  MY_OPT_DUMP_NOARG
,  MY_OPT_DUMP_PAIRS
,  MY_OPT_DUMP_MEASURES
,  MY_OPT_DNS

,  MY_OPT_NSM        =  MY_OPT_DNS + 2
,  MY_OPT_CCM

,  MY_OPT_VERSION    =  MY_OPT_CCM + 2
,  MY_OPT_HELP
}  ;


mcxOptAnchor options[] =
{  {  "--version"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "output version information, exit"
   }
,  {  "-icl"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_ICL
   ,  "<fname>"
   ,  "read clustering matrix from file"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "read graph matrix from file"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "read tab file"
   }
,  {  "-dir"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DIR
   ,  "<dname>"
   ,  "use directory as base entry point, split output"
   }
,  {  "-zmm"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ZMM
   ,  "<fname>"
   ,  "use zoem definitions from this file"
   }
,  {  "-fmt"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_FMT
   ,  "<fname>"
   ,  "write logical output to this file"
   }
,  {  "-infix"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_INFIX
   ,  "<str>"
   ,  "insert <str> after base name or base directory"
   }
,  {  "-lump-count"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_LUMP_COUNT
   ,  "<n>"
   ,  "batch size threshold for split behaviour"
   }
,  {  "-nsm"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_NSM
   ,  "<fname>"
   ,  "write node stickiness matrix to file"
   }
,  {  "-ccm"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CCM
   ,  "<fname>"
   ,  "write cluster cohesion matrix to file"
   }
,  {  "--adapt"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_ADAPT
   ,  NULL
   ,  "allow non-matching domains and defective clusterings"
   }
,  {  "--subgraph"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SUBGRAPH
   ,  NULL
   ,  "take subgraph (when domains don't match)"
   }
,  {  "--lazy-tab"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LAZY
   ,  NULL
   ,  "allow mismatched tab-file"
   }
,  {  "-pi"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_PI
   ,  "<f>"
   ,  "apply inflation to input graph"
   }
,  {  "-dump"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DUMP
   ,  "<fname>"
   ,  "dump line-based output, default one cluster per line"
   }
,  {  "--dump"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_NOARG
   ,  NULL
   ,  "dump line-based output, default one cluster per line"
   }
,  {  "--fancy"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_FANCY
   ,  NULL
   ,  "use with -dump/--dump if you want html/txt files as well"
   }
,  {  "--dump-pairs"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_PAIRS
   ,  NULL
   ,  "write cluster/element pair per single line"
   }
,  {  "--dump-measures"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP_MEASURES
   ,  NULL
   ,  "include performance measures in dump output"
   }
,  {  "-dump-node-sep"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DNS
   ,  "<str>"
   ,  "the separator for node indices or node labels"
   }
,  {  "-h"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_HELP
   ,  NULL
   ,  "this info"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


const char* fmt_label_open = "" ;
const char* fmt_label_close = "" ;

const char* dump_node_sep = "\t";
mcxbool dump_pairs   =  FALSE;

static FILE* zfp     =  NULL;
static mcxIO* xf_azm =  NULL;
static mcxbool split =  FALSE;
static const char* fname_defs = "clmformat.zmm";


/*
,  "Node stickiness matrix: columns range over all nodes in the graph."
,  "  Given a node c, the entry in row zero shows the mass fraction of edges"
,  "  that have c as tail and for which the head is in the same cluster as c,"
,  "  relative to the mass of all edges that have c as tail."
,  "  The entry in row one lists the number of edges originating from c."
,  "Cluster cohesion matrix: columns range over all clusters."
,  "  Given a cluster C, the entry in row zero shows the average of the node"
,  "  stickiness (defined above) for all nodes c in C."
,  "  The entry in row one lists the number of nodes in c."
,  "Residue projection matrix:"
,  "  Contains projection value of missing nodes onto each of the clusterings."
*/


void print_el_scores
(  mcxTing* scr
,  mclvScore* score
)
   {  double mass = score->sum_i + score->sum_o
   ;  double selfval = mass ? score->sum_i / mass : 0.0
   ;  double cov, covmax

   ;  mclvScoreCoverage(score, &cov, &covmax)

   ;  mcxTingPrint
      (  scr
      ,  "{%ld-%ld}{%.2f}"
      ,  (long) score->n_meet
      ,  (long) score->n_vdif
      ,  selfval
      )
   ;  mcxTingPrintAfter(scr, "{%.3f-%.3f}", cov, covmax)

   ;  if (!score->n_meet)
      mcxTingPrintAfter(scr, "{na-na}")
   ;  else
      mcxTingPrintAfter(scr, "{%.3f-%.3f}", score->max_i, score->min_i)

   ;  if (!score->n_vdif)     /* fixme check todo isthis ok ? */
      mcxTingPrintAfter(scr, "{na-na}")
   ;  else
      mcxTingPrintAfter(scr, "{%.3f-%.3f}", score->max_o, score->min_o)
;  }


void freenames
(  char** fnames
)
   {  char** baseptr = fnames
   ;  if (!fnames)
      return
   ;  while (*fnames)
      {  mcxFree(*fnames)
      ;  fnames++
   ;  }
      mcxFree(baseptr)
;  }


/* todo:
 * options for printing cluster index, cluster confidence,
 * nrof elements ...
 * format for nodes: %i:%t <%i t="%t"> etc
*/
void dodump
(  mcxIO*   xf_dump
,  mclx*    cl
,  mclTab*  tab      /* NULL allowed */
,  mclx*    clvals   /* NULL allowed */
)
   {  int i, j
   ;  FILE* fp = xf_dump->fp
   ;  for (i=0;i<N_COLS(cl);i++)
      {  mclv* clus = cl->cols+i
      ;  if (clvals)
         {  mclv* clvv = clvals->cols+i
         ;  fprintf
            (  fp
            ,  "%ld%s%ld%s%.2f%s%.2f%s%.2f"
            ,  (long) clus->vid
            ,  dump_node_sep
            ,  (long) clus->n_ivps
            ,  dump_node_sep
            ,  (double) clvv->ivps[0].val
            ,  dump_node_sep
            ,  (double) clvv->ivps[1].val
            ,  dump_node_sep
            ,  (double) clvv->ivps[2].val
            )
         ;  if (dump_pairs)
            fputs("\n", fp)
      ;  }
         for (j=0;j<clus->n_ivps;j++)
         {  long elid = clus->ivps[j].idx
         ;  if (!dump_pairs && (j || clvals))
            fputs(dump_node_sep, fp)

         ;  if (tab)
            {  char* lbl = mclTabGet(tab, elid, NULL)

            ;  if (dump_pairs)
               fprintf(fp, "%ld%s%s\n", (long) clus->vid, dump_node_sep, lbl)
            ;  else
               fprintf(fp, "%s", lbl)
         ;  }
            else if (dump_pairs)
            fprintf(fp, "%ld%s%ld\n", (long) clus->vid, dump_node_sep, (long) elid)
         ;  else
            fprintf(fp, "%ld", (long) elid)
      ;  }
         if (clus->n_ivps && (!dump_pairs || clvals))
         fprintf(fp, "\n")
   ;  }
   }


char** mknames
(  mclMatrix* cl
,  const char* base
,  int batchsize
)
   {  int i
   ;  char** fnames = mcxAlloc((N_COLS(cl)+1) * sizeof(char*), EXIT_ON_FAIL)
   ;  mcxTing* txt = mcxTingEmpty(NULL, 80)
   ;  mcxTing* bch = mcxTingEmpty(NULL, 80)
   ;  mcxTing* res = NULL
   ;  mcxTing* emp = mcxTingNew("")
   ;  int carry = 0

   ;  if (!mcldIsCanonical(cl->dom_cols))
      mcxDie(1, me, "Currently I need sequentially numbered clusters")

   ;  for (i=0;i<N_COLS(cl);i++)
      {  int clid = cl->cols[i].vid

      ;  if (batchsize)
         {  if (!carry)
            {  int j
            ;  for (j=i;j<N_COLS(cl) && carry<batchsize;j++)
               carry+= cl->cols[j].n_ivps
            ;  carry = 0
            ;  mcxTingWrite(bch, base)
            ;  mcxTingPrintAfter
               (  bch
               ,  "%s%ld"
               ,  bch->len ? "." : ""
               ,  (long) clid
               )
            ;  if (j-i > 1)
               mcxTingPrintAfter
               (  bch
               ,  "-%ld"
               ,  (long) (j-1)
               )
         ;  }
            carry += cl->cols[i].n_ivps

         ;  fnames[i] = mcxTingStr(bch)
         ;  if (carry > batchsize)
            carry = 0
      ;  }
      }

      fnames[N_COLS(cl)] = NULL

   ;  mcxTingFree(&txt)
   ;  mcxTingFree(&bch)
   ;  mcxTingFree(&res)
   ;  mcxTingFree(&emp)
   ;  return fnames
;  }


/*    The entries in cl are set to the self-projection value.
 *
*/

void settoself
(  mclMatrix* cl
,  mclMatrix* el_on_cl
)
   {  int i, j
   ;  for (i=0;i<N_COLS(cl);i++)
      {  mclv* clvec = cl->cols+i
      ;  long  clid  = clvec->vid
      ;  mclv* elclvec = NULL
      ;  mclp* valivp = NULL
      ;  for (j=0;j<clvec->n_ivps;j++)
         {  long elid = clvec->ivps[j].idx
         ;  elclvec = mclxGetVector(el_on_cl, elid, EXIT_ON_FAIL, elclvec)
         ;  valivp = mclvGetIvp(elclvec, clid, NULL)
         ;  if (!valivp && clvec->n_ivps > 1)
            mcxErr(me, "match error: el %ld cl %ld", elid, clid)
         ;  clvec->ivps[j].val = valivp ? MAX(0.01, valivp->val) : 0.01
      ;  }
      }
   }


void prune_it
(  mclMatrix* el_to_cl  /* must be conforming */
,  mclMatrix* el_on_cl  /* this one will be pruned */
,  double pct
,  int max
)
   {  int i
   ;  for (i=0;i<N_COLS(el_on_cl);i++)
      {  mclv* elclvec  =  el_on_cl->cols+i
      ;  long clid      =  el_to_cl->cols[i].ivps[0].idx
      ;  mclp* elivp    =  mclvGetIvp(elclvec, clid, NULL)
      ;  double elfrac  =  elivp ? elivp->val : 0.01
      ;  double sum     =  elivp ? elivp->val : 0.05
      ;  int n_others   =  0
      ;  int k          =  -1

      ;  mclvSort(elclvec, mclpValRevCmp)

      ;  while (++k < elclvec->n_ivps && sum < pct && n_others < max)
         {  long y = elclvec->ivps[k].idx
         ;  if (y == clid)
            continue
         ;  sum += elclvec->ivps[k].val
         ;  n_others++
      ;  }

         mclvResize(elclvec, k)
      ;  mclvSort(elclvec, mclpIdxCmp)
      ;  if (!mclvGetIvp(elclvec, clid, NULL))
         mclvInsertIdx(elclvec, clid, elfrac)
   ;  }
   }


/* 0: self value
 * 1: coverage
 * 2: maxcoverage
*/

mclMatrix* mkclvals
(  mclMatrix* mx
,  mclMatrix* cl
,  mclMatrix* cl_on_cl
)
   {  int i
   ;  mclxScore xscore
   ;  mclMatrix* clvals
      =  mclxCartesian
         (  mclvCopy(NULL, cl->dom_cols), mclvCanonical(NULL, 3, 1.0), 1.0 )

   ;  for(i=0;i<N_COLS(cl);i++)
      {  mclv* clvec = cl->cols+i
      ;  mclp* tivp = mclvGetIvp(cl_on_cl->cols+i, clvec->vid, NULL)

      ;  clvals->cols[i].ivps[0].val = tivp ? MAX(tivp->val, 0.001) : 0.001
      ;  mclxSubScan
         (mx, clvec, clvec, &xscore)
      ;  clvals->cols[i].ivps[1].val = MAX(xscore.cov, 0.001)
      ;  clvals->cols[i].ivps[2].val = MAX(xscore.covmax, 0.001)
   ;  }
      return clvals
;  }


/*
 *    For each node, computes the vscore for its first cluster.
 *    for other clusters, it has to be computed on the fly.
*/

mclvScore* mkelvals
(  mclMatrix* mx
,  mclMatrix* cl
,  mclMatrix* el_to_cl
)
   {  long n = N_COLS(mx), i
   ;  mclvScore* scores = mcxNAlloc(n, sizeof(mclvScore), NULL, EXIT_ON_FAIL)
   ;  for (i=0;i<n;i++)    /* only see element offsets here, no idx */
      {  long clid = el_to_cl->cols[i].ivps[0].idx
      ;  long clos = mclxGetVectorOffset(cl, clid, EXIT_ON_FAIL, -1)
      ;  mclvSubScan(mx->cols+i, cl->cols+clos, scores+i)
   ;  }
      return scores
;  }


long getclusid
(  mclMatrix* el_to_cl
,  long elid
)
   {  long offset = mclxGetVectorOffset(el_to_cl, elid, EXIT_ON_FAIL, -1)
   ;  return el_to_cl->cols[offset].ivps[0].idx
;  }


void mkanindex
(  const char* ind1
,  const char* ind2
,  char* title_ind2
,  mclMatrix* mx
,  mclMatrix* cl
,  mclMatrix* el_to_cl
,  mclMatrix* clvals
,  mclTab* tab
,  char** fnames
,  mclVector* cllist
,  mclVector* ellist       /* need not be conforming (e.g. val ordered) */
,  mclvScore* elscores
)
   {  double coh, cov, maxcov
   ;  mcxTing* scr = mcxTingEmpty(NULL, 80)
   ;  int i

   ;  cov = mclxCoverage(mx, cl, &maxcov)
               /* fixme could be infered from elscores */

   ;  coh = mclvSum(cllist)
   ;  if (cllist->n_ivps)
      coh /=  cllist->n_ivps

   ;  if (split)
      fprintf(zfp, "\\import{%s}", fname_defs)

   ;  fprintf
      (  zfp
      ,  "\\writeto{%s.\\__device__}\n"
         "\\set{cfn}{%s.\\__device__}\n"
         "\\fmt_header\n"
         "\\index_top\n"
      ,  ind1
      ,  ind1
      )

   ;  fprintf
      (  zfp
      ,  "\\clustering{%ld}{%.2f}{%.2f}{%.2f}\n"
         "\\ref_index_top{%s}\n"
      ,  (long) N_COLS(cl)
      ,  coh
      ,  cov
      ,  maxcov
      ,  ind2
      )

   /* hierverder: add average, center, max, min, count */
   /* perhaps even a little 'staafdiagram' to show size distribution */

   ;  fprintf(zfp, "\\cixcaption\n")

   ;  for (i=0;i<cllist->n_ivps;i++)
      {  long clusid = cllist->ivps[i].idx
      ;  long clusos = mclvGetIvpOffset(clvals->dom_cols, clusid, -1)
            /* redundant; we require canonical col domain somewhere */
            /* fixme; check clusos! */

      ;  fprintf
         (  zfp
         ,  "\\cix{%ld}{%s}{%ld}{%.2f}{%.2f-%.2f}\n"
         ,  clusid
         ,  fnames[clusos]
         ,  (long)   cl->cols[clusos].n_ivps
         ,  (double) clvals->cols[clusos].ivps[0].val
         ,  (double) clvals->cols[clusos].ivps[1].val
         ,  (double) clvals->cols[clusos].ivps[2].val
         )
   ;  }

      fprintf(zfp, "\\cixend\n")
   ;  fprintf(zfp, "\\eixcaption\n")

   ;  for (i=0;i<ellist->n_ivps;i++)
      {  long clusid = getclusid(el_to_cl, ellist->ivps[i].idx)
      ;  long elid = ellist->ivps[i].idx
      ;  long elos = mclxGetVectorOffset(el_to_cl, elid, EXIT_ON_FAIL, -1)

      /* BEWARE; ellist may be sorted on value.
       * elscores is always sorted on index.
       * so we need to work with elos
      */
      ;  print_el_scores(scr, elscores+elos)

      ;  fprintf
         (  zfp
         ,  "\\eix{%s}{%ld}{%ld}{%s}%s\n"
         ,  tab ? mclTabGet(tab, elid, NULL) : ""
         ,  elid
         ,  clusid
         ,  fnames[clusid] 
         ,  scr->str             /* nb2 self cv2 xi2 xo2 */
         )
      ;  mcxTingEmpty(scr, 0)
   ;  }

      fprintf(zfp, "\\eixend\n")
   ;  fputs("\\fmt_footer\n", zfp)
   ;  mcxTingFree(&scr)
;  }


void mkindex
(  const char* ind1
,  const char* ind2
,  mclMatrix* mx
,  mclMatrix* cl
,  mclMatrix* el_to_cl
,  mclMatrix* clvals
,  mclTab* tab
,  char** fnames
,  mclvScore* elscores
)
   {  mclv* cllist = mclvCopy(NULL, clvals->dom_cols)  /* coverage */
   ;  mclv* ellist = mclvCopy(NULL, cl->dom_rows)
   ;  int i

   ;  for (i=0;i<cllist->n_ivps;i++)
      {  cllist->ivps[i].val = clvals->cols[i].ivps[0].val
      ;  mcldMerge(cl->cols+i, ellist, ellist)
        /*  what is this?
         *  the cl element entries have projection as val.
         *  so this creates a list of all entries with projection values.
        */
   ;  }

      mcxTell(me, "writing index file [%s]", ind1)
   ;  mkanindex
      (  ind1
      ,  ind2
      ,  "Index II"
      ,  mx, cl
      ,  el_to_cl
      ,  clvals
      ,  tab
      ,  fnames
      ,  cllist
      ,  ellist
      ,  elscores
      )

   ;  mclvSort(cllist, mclpValRevCmp)
   ;  mclvSort(ellist, mclpValRevCmp)

   ;  mcxTell(me, "writing index file [%s]", ind2)
   ;  mkanindex
      (  ind2
      ,  ind1
      ,  "Index I"
      ,  mx, cl
      ,  el_to_cl
      ,  clvals
      ,  tab
      ,  fnames
      ,  cllist
      ,  ellist
      ,  elscores
      )

   ;  mclvFree(&cllist)
   ;  mclvFree(&ellist)
;  }


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO *xf_cl = NULL, *xf_mx = NULL
      , *xf_nsm = NULL, *xf_ccm = NULL, *xf_tab = NULL
      , *xf_dump = NULL
      , *xf_tst = mcxIOnew("-", "w")

   ;  mcxbool     write_defs  =  TRUE
   ;  mcxbool     adapt       =  FALSE
   ;  mcxbool     subgraph    =  FALSE
   ;  mcxbool     lazy_tab    =  FALSE
   ;  mcxbool     dump        =  FALSE
   ;  mcxbool     fancy       =  FALSE
   ;  mcxbool     dump_measures  =  FALSE
   ;  mcxTing*    dn_fmt      =  NULL
   ;  mcxTing*    fn_azm      =  mcxTingNew("fmt.azm")
   ;  mclMatrix   *cl         =  NULL
   ;  mclMatrix   *mx         =  NULL
   ;  mclvScore*  elscores    =  NULL 
   ;  mclvScore   score
   ;  mclx* el_to_cl = NULL, *el_on_cl = NULL, *cl_on_cl = NULL
            , *cl_on_el = NULL, *clvals = NULL
   ;  mclv* clclvec = NULL

   ;  mclVector   *meet       =  NULL
   ;  mclTab* tab = NULL

   ;  int i, j
   ;  int o, m, e, n_err = 0
   ;  int batchsize = 500
   ;  const char* infix = ""
   ;  char** fnames = NULL
   ;  float inflation = 0.0

   ;  mcxTing *txtCL0 = mcxTingEmpty(NULL, 80)
   ;  mcxTing *txtEL1 = mcxTingEmpty(NULL, 80)
   ;  mcxTing *txtEL2 = mcxTingEmpty(NULL, 80)
   ;  mcxTing *txtCL1 = mcxTingEmpty(NULL, 80)
   ;  mcxTing *txtCL2 = mcxTingEmpty(NULL, 80)

   ;  mcxstatus parseStatus   =  STATUS_OK
   ;  mcxOption* opts, *opt

   ;  mcxIOopen(xf_tst, EXIT_ON_FAIL) /* stdout, for debug dumping */
   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)

   ;  if (argc <= 1)
      {  mcxOptApropos(stdout, me, syntax, 0, MCX_OPT_DISPLAY_SKIP, options)
      ;  exit(0)
   ;  }

      opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)

   ;  if (parseStatus != STATUS_OK)
      {  mcxErr(me, "initialization failed")
      ;  exit(1)
   ;  }


      for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_ICL
         :  xf_cl =  mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_IMX
         :  xf_mx =  mcxIOnew(opt->val, "r")
         ;  mcxIOopen(xf_mx, EXIT_ON_FAIL)
         ;  break
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  exit(0)
         ;

            case MY_OPT_HELP
         :  mcxOptApropos(stdout, me, syntax, 0, MCX_OPT_DISPLAY_SKIP, options)
         ;  exit(0)
         ;

            case MY_OPT_TAB
         :  xf_tab = mcxIOnew(opt->val, "r")
         ;  break
         ;

            case MY_OPT_PI
         :  inflation = atof(opt->val)
         ;  break
         ;

            case MY_OPT_ADAPT
         :  adapt = TRUE
         ;  break
         ;

            case MY_OPT_SUBGRAPH
         :  subgraph = TRUE
         ;  break
         ;

            case MY_OPT_LAZY
         :  lazy_tab = TRUE
         ;  break
         ;

            case MY_OPT_ZMM
         :  fname_defs = opt->val
         ;  write_defs = FALSE
         ;  break
         ;

            case MY_OPT_FMT
         :  fn_azm = mcxTingNew(opt->val)
         ;  break
         ;

            case MY_OPT_DIR
         :  dn_fmt = mcxTingNew(opt->val)
         ;  break
         ;

            case MY_OPT_INFIX
         :  infix = opt->val
         ;  break
         ;

            case MY_OPT_LUMP_COUNT
         :  batchsize = strtol(opt->val, NULL, 10)
         ;  break
         ;

            case MY_OPT_DUMP_NOARG     /* construct output file from input */
         :  dump = TRUE
         ;  break
         ;

            case MY_OPT_DUMP
         :  xf_dump = mcxIOnew(opt->val, "w")
         ;  dump = TRUE
         ;  break
         ;

            case MY_OPT_FANCY
         :  fancy = TRUE
         ;  break
         ;

            case MY_OPT_DUMP_MEASURES
         :  dump_measures =  TRUE
         ;  break
         ;

            case MY_OPT_DUMP_PAIRS
         :  dump_pairs =  TRUE
         ;  break
         ;

            case MY_OPT_DNS
         :  dump_node_sep = opt->val
         ;  break
         ;

            case MY_OPT_NSM
         :  xf_nsm = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_CCM
         :  xf_ccm = mcxIOnew(opt->val, "w")
         ;  break
         ;

            default
         :  mcxDie(1, me, "option parsing bug") 
      ;  }
      }

      if (!fancy && !dump)
      fancy = TRUE

   ;  if (!xf_cl)
      mcxDie(1, me, "-icl is required in all modes (see -h)")
   ;  else if (fancy && !xf_mx)
      mcxDie(1, me, "to get fancy you have to supply -imx (see -h)")

   ;  if (xf_ccm)
      mcxIOopen(xf_ccm, EXIT_ON_FAIL)
   ;  if (xf_nsm)
      mcxIOopen(xf_nsm, EXIT_ON_FAIL)

   ;  cl =  mclxRead(xf_cl, EXIT_ON_FAIL)
   ;

      if (xf_mx)
      {  mx =  mclxReadx(xf_mx, EXIT_ON_FAIL, MCL_READX_GRAPH)
      ;  mcxIOfree(&xf_mx)

      ;  if (inflation)
         mclxInflate(mx, inflation)
      ;  mclxMakeStochastic(mx)

      ;  if (!mcldEquate(mx->dom_cols, cl->dom_rows, MCLD_EQ_EQUAL))
         {  mclVector* meet   =  mcldMeet(mx->dom_cols, cl->dom_rows, NULL)
         ;  mcxErr
            (  me
            ,  "Domain mismatch for matrix ('left') and clustering ('right')"
            )
         ;  report_domain(me, N_COLS(mx), N_ROWS(cl), meet->n_ivps)

         ;  if (adapt)
            {  mclMatrix* tmx, *tcl

            ;  tcl   =  mclxSub(cl, cl->dom_cols, meet)
            ;  mclxFree(&cl)
            ;  cl    =  tcl

            ;  tmx   =     subgraph
                        ?  mclxSub(mx, meet, meet)
                        :  mclxSub(mx, meet, mx->dom_cols)
            ;  mclxFree(&mx)
            ;  mx = tmx
         ;  }
            else
            report_exit(me, SHCL_ERROR_DOMAIN)
      ;  }

         if (mclcEnstrict(cl, &o, &m, &e, ENSTRICT_KEEP_OVERLAP))
         {  report_partition(me, cl, xf_cl->fn, o, m, e)
         ;  if (!e && !m)
            ;
            else if (adapt)
            report_fixit(me, n_err++)
         ;  else
            report_exit(me, SHCL_ERROR_PARTITION)
      ;  }

         el_to_cl =  mclxTranspose(cl)
      ;  el_on_cl =  mclxCompose(el_to_cl, mx, 0) /* el_to_cl not stochastic? */
      ;  cl_on_cl =  mclxCompose(el_on_cl, cl, 0)
      ;  mclxMakeStochastic(el_on_cl)
      ;  mclxMakeStochastic(cl_on_cl)

      ;  if (1)
         settoself(cl, el_on_cl)
      ;  prune_it(el_to_cl, el_on_cl, 0.95, 10)
      ;  cl_on_el =  mclxTranspose(el_on_cl)

      ;  for (i=0;i<N_COLS(el_to_cl);i++)    /* some checks: necessary? */
         {  long clusid
         ;  mclVector* clus = NULL

         ;  if ((el_to_cl->cols+i)->n_ivps == 0)
            {  mcxErr
               (  me
               ,  "element <%ld> not in clustering"
               ,  (long) el_to_cl->cols[i].vid
               )
            ;  exit(1)
         ;  }
            else
            {  clusid =  ((el_to_cl->cols+i)->ivps+0)->idx
            ;  clus   =  mclxGetVector(cl, clusid, RETURN_ON_FAIL, clus)
            ;  if (!clus)
               {  mcxErr("clmformat panic", "cluster not found")
               ;  exit(1)
            ;  }
            }
         }

         mclvFree(&meet)

      ;  if (xf_nsm)
         {  mclxWrite(el_on_cl, xf_nsm, 6, RETURN_ON_FAIL);
         ;  mcxIOfree(&xf_nsm)
      ;  }
         if (xf_ccm)
         {  mclxWrite(cl_on_cl, xf_ccm, 6, EXIT_ON_FAIL);
         ;  mcxIOfree(&xf_ccm)
      ;  }
      }

      if (xf_tab)
      {  const mclv* dom_check = lazy_tab ? NULL : cl->dom_rows
      ;  mcxIOopen(xf_tab, EXIT_ON_FAIL)
      ;  tab =  mclTabRead(xf_tab, dom_check, EXIT_ON_FAIL)
      ;  mcxIOfree(&xf_tab)
        /*
         *  NOTE/fixme/todo: cl might have been changed by adaptation. too
         *  bad then.  we depend on cl as we jointly do a) read the tab file
         *  conditionally on cl row domain above b) mclvGetIvp on the cl row
         *  domain below.  we could go modal, do these things on the matrix
         *  domain if present (mclxGetVector etc), do these things on the cl
         *  row domain otherwise.
        */
   ;  }

      if (fancy)
      {  if (!dn_fmt)
         dn_fmt = mcxTingPrint(NULL, "fmt.%s", xf_cl->fn->str) 
      ;  if (chdir(dn_fmt->str))
         {  if (errno == ENOENT)
            {  if (mkdir(dn_fmt->str, 0777))
                  perror("mkdir error")
               ,  mcxDie(1, me, "Cannot create dir <%s>", dn_fmt->str)
            ;  else if (chdir(dn_fmt->str))
               mcxDie(1, me, "Cannot change dir to <%s>", dn_fmt->str)
         ;  }
            else
               perror("chdir error")
            ,  mcxDie(1, me, "Cannot change dir to <%s>", dn_fmt->str)
      ;  }
      }

      if (dump && !xf_dump)
      {  if  (fancy)
         xf_dump = mcxIOnew("dump", "w")
      ;  else 
         {  mcxTing* dump_name = mcxTingPrint(NULL, "dump.%s", xf_cl->fn->str)
         ;  xf_dump = mcxIOnew(dump_name->str, "w")
         ;  mcxTingFree(&dump_name)
      ;  }
      }

      if (xf_dump)
      {  mcxIOopen(xf_dump, EXIT_ON_FAIL)
      ;  clvals = mx && dump_measures ? mkclvals(mx, cl, cl_on_cl) : NULL
      ;  dodump(xf_dump, cl, tab, clvals)
      ;  if (!fancy)
         return 0
   ;  }

      fnames = mknames(cl, infix, batchsize)

   ;  if (!clvals)
      clvals = mkclvals(mx, cl, cl_on_cl)
   ;  elscores = mkelvals(mx, cl, el_to_cl)

   ;  {  xf_azm = mcxIOnew(fn_azm->str, "w")
      ;  mcxIOopen(xf_azm, EXIT_ON_FAIL)
      ;  zfp = xf_azm->fp
      ;  if (!split)
         fprintf(zfp, "\\import{%s}\n", fname_defs)
   ;  }

      if (write_defs)
      {  mcxIO *xf_zmm  = mcxIOnew(fname_defs, "w")
      ;  int a
      ;  mcxIOopen(xf_zmm, EXIT_ON_FAIL)
      ;  for (a=0;defs_zmm[a];a++)
            fputs(defs_zmm[a], xf_zmm->fp)
         ,  fputs("\n", xf_zmm->fp)
      ;  mcxIOclose(xf_zmm)
   ;  }

      mkindex("index", "index2", mx, cl, el_to_cl, clvals, tab,fnames,elscores)
   
   ;  for (i=0;i<N_COLS(cl);i++)
      {  mclv* clvec = cl->cols+i
      ;  mclv* clsortvec = mclvCopy(NULL, clvec)
      ;  long clid = clvec->vid
      ;  long clnext = cl->cols[(i+1) % N_COLS(cl)].vid
      ;  long clprev = cl->cols[(i-1+N_COLS(cl)) % N_COLS(cl)].vid
      ;  mclp* domivp = NULL
      ;  mclv* elclvec = NULL
      ;  mclv* elmxvec = NULL
      ;  double elfrac = 0.0
      ;  double clfrac = 0.0
      ;  int endfile = 0
      ;  mcxTing* scr = mcxTingEmpty(NULL, 80)

/* ----------------------------------------------------------------------- */
      
      ;  mcxTingEmpty(txtCL0, 80)

      ;  {  int newfile = 0
         ;  if (batchsize)
            {  if (i==0 || strcmp(fnames[i-1], fnames[i]))
               {  if (split)
                  fprintf(zfp, "\\import{%s}", fname_defs)
               ;  fprintf(zfp, "\\writeto{%s.\\__device__}\n", fnames[i])
               ;  newfile = 1
            ;  }
            /* __ we need the shortcut: otherwise i+1 might overflow */
               if (i+1==N_COLS(cl) || strcmp(fnames[i], fnames[i+1]))
               endfile = 1
         ;  }

            mcxTell(me, "Writing cluster %ld in batch [%s]", clid, fnames[i])
         ;  if (newfile)
            fprintf(zfp, "\\fmt_header\n")
      ;  }

         fprintf
         (  zfp
         ,  "\\cl{%ld}{%ld}{%.2f}{%.2f-%.2f}{%ld}{%s}{%ld}{%s}\n"
         ,  (long) clvec->vid                          /* cl name */
         ,  (long) clvec->n_ivps                       /* size */
         ,  clvals->cols[i].ivps[0].val         /* cohesion */
         ,  clvals->cols[i].ivps[1].val         /* coverage */
         ,  clvals->cols[i].ivps[2].val         /* maxcoverage */
         ,  (long) clprev
         ,  fnames[clprev]       /* fixme; clprev need not be offset */
         ,  (long) clnext
         ,  fnames[clnext]       /* fixme; clprev need not be offset */
         )
                                 /* html: make inner outer bottom links */

/* ----------------------------------------------------------------------- */
 /* yes, it's a sign of poverty to use such delimiters */
 /* the part could be stuffed into a sub */

      ;  if (cl_on_cl)
         {  mclv* tmpclvec       /* used for sorting */
         ;  mclp* clivp
         ;  double sumcl = clfrac
         ;  double clselfval = 0.0
         ;  int n_cls = 0
         ;  long x = 0
         ;  clclvec = mclxGetVector(cl_on_cl, clid, EXIT_ON_FAIL, clclvec)
         ;  clivp = mclvGetIvp(clclvec, clid, NULL)
         ;  if (!clivp && clvec->n_ivps > 1)
            mcxErr
            (  me
            ,  "Cluster %ld does not project onto itself"
            ,  (long) clid
            )
         ;  clselfval = clivp ? clivp->val : 0.0
         ;  tmpclvec = mclvCopy(NULL, clclvec)

         ;  mclvSort(tmpclvec, mclpValRevCmp)

         ;  mcxTingPrint(scr, "\\c2c{%.2f}{\n", clselfval)

         ;  while (sumcl < 0.95 && n_cls < 10 && ++x < tmpclvec->n_ivps)
            {  long otherid = tmpclvec->ivps[x].idx

            ;  if (otherid == clid)
               continue

            ;  mcxTingPrintAfter
               (  scr
               ,  "{%ld}{%s}{%.2f}"
               ,  (long) otherid
               ,  fnames[otherid]
               ,  (double) tmpclvec->ivps[x].val
               )
            ;  sumcl += tmpclvec->ivps[x].val
            ;  n_cls++
         ;  }

            fprintf(zfp, "%s}\n", scr->str)
         ;  mclvFree(&tmpclvec)
     ;   }

/* ----------------------------------------------------------------------- */

         mclvSort(clsortvec, mclpValRevCmp)
      ;  fprintf(zfp, "\\sec_inner{%ld}\n", clid)

      ;  for (j=0;j<clsortvec->n_ivps;j++)
         {  long elid = clsortvec->ivps[j].idx
         ;  long elos
         ;  domivp = mclvGetIvp(cl->dom_rows, elid, NULL)
               /* clsortvec must be ascending in elid if #3 != NULL*/

         ;  if (!domivp)
            mcxDie(1, me, "match error 2: el %ld cl %ld", elid, clid)

         ;  elos = domivp - cl->dom_rows->ivps
         ;  elmxvec = mclxGetVector(mx, elid, EXIT_ON_FAIL, NULL)
               /* clsortvec must be ascending in elid if #4 != NULL*/

         ;  mcxTingEmpty(txtEL1, 80)

         ;  if (mx)                 /* fixme (warn, demand, or extend)  */
            {  print_el_scores(scr, elscores+elos)
            ;  fprintf
               (  zfp
               ,  "\\eis{%s}{%ld}%s{%.2f}\n"
               ,  tab ? mclTabGet(tab, elid, NULL) : ""
               ,  elid
               ,  scr->str
               ,  (double) elmxvec->val   /* set by mclxMakeStochastic */
               )
            ;  mcxTingEmpty(scr, 0)
         ;  }

/* ----------------------------------------------------------------------- */

            if(mx)
            {  double cov, covmax
            ;  mclp *elivp

            ;  score = elscores[elos]
            ;  mclvScoreCoverage(&score, &cov, &covmax)

            ;  elclvec = mclxGetVector(el_on_cl, elid, EXIT_ON_FAIL, NULL)
               /* clsortvec must be ascending in elid if #3 != NULL*/
            ;  elivp = mclvGetIvp(elclvec, clid, NULL)
            ;  elfrac = elivp ? elivp->val : 0.0

            ;  mcxTingEmpty(txtCL2, 80)
            ;  /* mclvSubScan(mx->cols+elos, clvec, &vscore) */
            ;  /* mclvSubScan(elmxvec, clvec, &vscore) */

            ;  if (1)   /* other clusters branch */
               {  mclv* tmpelvec = mclvCopy(NULL, elclvec)
               ;  long k
               ;  mclvSort(tmpelvec, mclpValRevCmp)

               ;  for (k=0;k<tmpelvec->n_ivps;k++)
                  {  mclv *cl2vec
                  ;  mclvScore tscore
                  ;  long cl2id = tmpelvec->ivps[k].idx
                  ;  if (cl2id == clid)
                     continue

                  ;  cl2vec = mclxGetVector(cl, cl2id, EXIT_ON_FAIL, NULL)
                  ;  mclvSubScan(elmxvec, cl2vec, &tscore)
                  ;  mclvScoreCoverage(&tscore, &cov, &covmax)

                  ;  fprintf
                     (  zfp
                     ,  "\\eio"
                        "{%ld}{%ld}{%s}{%ld/%ld}{%.2f}{%.3f-%.3f}{%.3f-%.3f}\n"
                     ,  (long) elid
                     ,  (long) cl2id
                     ,  fnames[cl2id]        /* fixme, offset */
                     ,  (long) cl2vec->n_ivps
                     ,  (long) tscore.n_meet
                     ,  (double) tscore.sum_i
                     ,  (double) cov
                     ,  (double) covmax
                     ,  (double) tscore.max_i
                     ,  (double) tscore.min_i
                     )
               ;  }
                  mclvFree(&tmpelvec)
            ;  }
            }
         }

         fprintf(zfp, "\\sec_inner_end{%ld}\n", clid)

/* ----------------------------------------------------------------------- */

      ;  if (mx)
         {  mclv* aliens = mcldMinus(cl_on_el->cols+i, clvec, NULL)
         ;  long j
         ;  mclvScore ascore
         ;  mcxTingEmpty(txtEL2, 80)

         ;  mclvSort(aliens, mclpValRevCmp)

         ;  fprintf(zfp, "\\sec_outer{%ld}\n", clid)

         ;  for (j=0;j<aliens->n_ivps;j++)
            {  mclv* cl3vec, *almxvec
            ;  double acov, acovmax
            ;  long alelid = aliens->ivps[j].idx
            ;  long alelos = mclxGetVectorOffset(mx, alelid, EXIT_ON_FAIL, -1)
            ;  long alclid = getclusid(el_to_cl, alelid)
            ;  domivp = mclvGetIvp(cl->dom_rows, alelid, NULL)
                  /* note that alelid need not be ascending */

            ;  almxvec = mclxGetVector(mx, alelid, EXIT_ON_FAIL, NULL)
            ;  mclvSubScan(almxvec, clvec, &ascore)
            ;  cl3vec = mclxGetVector(cl, alclid, EXIT_ON_FAIL, NULL)

            ;  mclvScoreCoverage(&ascore, &acov, &acovmax)

           /*  ascore:  alien element + current cluster
           */

            ;  print_el_scores(scr, elscores+alelos)

            ;  fprintf
               (  zfp
               ,  "\\eas{%s}{%ld}{%ld}{%s}{%ld}{%s}\n"
               ,  tab ? mclTabGet(tab, alelid, NULL) : ""
               ,  alelid
               ,  alclid
               ,  fnames[alclid]       /* fixme, must be offset */
               ,  (long) cl3vec->n_ivps
               ,  scr->str
               )
            ;  mcxTingEmpty(scr, 0)

            ;  fprintf
               (  zfp
               ,  "\\eat{%ld}{%.2f}{%.3f-%.3f}{%.3f-%.3f}{%.2f}\n"
               ,  (long)   ascore.n_meet
               ,  (double) ascore.sum_i
               ,  (double) acov
               ,  (double) acovmax
               ,  (double) ascore.max_i
               ,  (double) ascore.min_i
               ,  (double) almxvec->val   /* set by mclxMakeStochastic */
               )
         ;  }
            mclvFree(&aliens)
         ;  fprintf(zfp, "\\sec_outer_end{%ld}\n", clid)
      ;  }

         if (endfile)
         fprintf(zfp, "\\fmt_footer\n")
      ;  else
         fprintf(zfp, "\\fmt_rule\n")

      ;  mclvFree(&clsortvec)
   ;  }

      freenames(fnames)

   ;  mclTabFree(&tab)
   ;  mclxFree(&cl)
   ;  mclxFree(&mx)
   ;  mclxFree(&el_to_cl)
   ;  mclxFree(&el_on_cl)
   ;  mclxFree(&cl_on_cl)
   ;  mclxFree(&cl_on_el)
   ;  mclxFree(&clvals)

   ;  mcxTingFree(&dn_fmt)
   ;  mcxTingFree(&txtCL0)
   ;  mcxTingFree(&txtEL1)
   ;  mcxTingFree(&txtEL2)
   ;  mcxTingFree(&txtCL1)
   ;  mcxTingFree(&txtCL2)

   ;  return 0
;  }

