/*   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/*
 * TODO
 *    -  generate random clusterings
 *    -  warn on strange/wrong combinations of options (shuffle & xxx)
 *    -  support DAGs
 *    -  for small matrices and many additions keep track of
 *          matrix with non-edges (worthwhile?).
 *    -  Poisson distribution, others.
 *    #  prove that Pierre's approach to Gaussians works.
 *    -  pick value according to sampled distribution or removals
 *    -  we use only lower bits for edge selection
 *    -  insertion is slow due to array insertion;
 *          ideally store graph in a hash.
*/

#define DEBUG 0

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

#include "impala/io.h"
#include "impala/stream.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "impala/compose.h"
#include "impala/iface.h"

#include "util/types.h"
#include "util/ding.h"
#include "util/array.h"
#include "util/ting.h"
#include "util/io.h"
#include "util/err.h"
#include "util/equate.h"
#include "util/rand.h"
#include "util/opt.h"

#include "mcl/proc.h"
#include "mcl/procinit.h"
#include "mcl/alg.h"

#include "clew/clm.h"
#include "gryphon/path.h"


const char* me = "mcxrand";

void usage
(  const char**
)  ;


enum
{  MY_OPT_APROPOS
,  MY_OPT_HELP
,  MY_OPT_VERSION
,  MY_OPT_OUT
,  MY_OPT_WB
,  MY_OPT_PLUS
,  MY_OPT_IMX
,  MY_OPT_GEN
,  MY_OPT_ADD
,  MY_OPT_N_SDEV
,  MY_OPT_SHUFFLE
,  MY_OPT_REMOVE
,  MY_OPT_N_RADIUS
,  MY_OPT_N_RANGE
,  MY_OPT_E_MIN
,  MY_OPT_E_MAX
,  MY_OPT_G_MEAN
,  MY_OPT_G_SDEV
,  MY_OPT_G_RADIUS
,  MY_OPT_G_MIN
,  MY_OPT_G_MAX
,  MY_OPT_SKEW
}  ;

const char* syntax = "Usage: mcxrand [options] -imx <mx-file>";

mcxOptAnchor options[] =
{
   {  "--apropos"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "print this help"
   }
,  {  "-h"
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
,  {  "-gen"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_GEN
   ,  "<num>"
   ,  "node count"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "input matrix"
   }
,  {  "-noise-radius"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_N_RADIUS
   ,  "<num>"
   ,  "edge perturbation maximum absolute size"
   }
,  {  "-noise-sdev"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_N_SDEV
   ,  "<num>"
   ,  "edge perturbation standard deviation"
   }
,  {  "-noise-range"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_N_RANGE
   ,  "<num>"
   ,  "number of standard deviations allowed"
   }
,  {  "-new-g-radius"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_G_RADIUS
   ,  "<num>"
   ,  "maximum spread to generate new edges with"
   }
,  {  "-new-g-mean"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_G_MEAN
   ,  "<num>"
   ,  "mean to generate new edges with"
   }
,  {  "-new-g-min"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_G_MIN
   ,  "<num>"
   ,  "absolute lower bound for generated weights"
   }
,  {  "-new-g-max"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_G_MAX
   ,  "<num>"
   ,  "absolute upper bound for generated weights"
   }
,  {  "-new-g-sdev"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_G_SDEV
   ,  "<num>"
   ,  "standard deviation to generate new edges with"
   }
,  {  "-edge-min"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_E_MIN
   ,  "<num>"
   ,  "edge addition weight minimum (use -edge-max as well)"
   }
,  {  "-edge-max"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_E_MAX
   ,  "<num>"
   ,  "edge addition weight maximum (use -edge-min as well)"
   }
,  {  "-skew"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SKEW
   ,  "<num>"
   ,  "skew towards min (<num> > 1) or max (<num> < 1)"
   }
,  {  "-remove"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_REMOVE
   ,  "<num>"
   ,  "remove <num> edges"
   }
,  {  "--write-binary"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_WB
   ,  NULL
   ,  "write binary format"
   }
,  {  "+"
   ,  MCX_OPT_HIDDEN
   ,  MY_OPT_PLUS
   ,  NULL
   ,  "write binary format"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUT
   ,  "<fname>"
   ,  "output matrix to <fname>"
   }
,  {  "-add"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ADD
   ,  "<num>"
   ,  "add <num> edges not yet present"
   }
,  {  "-shuffle"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_SHUFFLE
   ,  "<num>"
   ,  "shuffle edge, repeat <num> times"
   }
,  {  NULL, 0, 0, NULL, NULL }
}  ;


int dimCmp
(  const void*   xp
,  const void*   yp
)
   {  return *((dim*) xp) < *((dim*) yp) ? -1 : *((dim*) xp) > *((dim*) yp) ? 1 : 0
;  }


static void mx_readd_diagonal
(  mclx*  mx
,  mclv*  diag
)
   {  dim i
   ;  for (i=0;i<diag->n_ivps;i++)
      {  mclp* ivp = diag->ivps+i
      ;  if (ivp->val)
         mclvInsertIdx(mx->cols+i, ivp->idx, ivp->val)
   ;  }
   }


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxIO* xfmx          =  mcxIOnew("-", "r"), *xfout = mcxIOnew("-", "w")
   ;  mclx* mx             =  NULL
   ;  mclv* mx_diag        =  NULL

   ;  mcxstatus parseStatus = STATUS_OK
   ;  mcxOption* opts, *opt
   ;  dim N_edge = 0
   ;  dim*  offsets

   ;  dim template_n_nodes = 0
   ;  mcxbool plus = FALSE

   ;  double e_min  = 1.0
   ;  double e_max  = 0.0
   ;  double skew = 0.0

   ;  double radius = 0.0
   ;  double n_sdev  = 0.5
   ;  double n_range = 2.0

   ;  double g_radius  = 0.0
   ;  double g_mean   = 0.0
   ;  double g_sdev = 0.0
   ;  double g_min  = 1.0
   ;  double g_max  = 0.0

   ;  dim i = 0

   ;  dim N_remove  = 0
   ;  dim N_add     = 0
   ;  dim N_shuffle = 0

   ;  dim n_remove  = 0
   ;  dim n_add     = 0
   ;  dim n_shuffle = 0

   ;  unsigned long ignore = 0

   ;  srandom(mcxSeed(2308947))
   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)

   ;  if
      (!(opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)))
      exit(0)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 20, MCX_OPT_DISPLAY_SKIP, options)
         ;  return 0
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  return 0
         ;

            case MY_OPT_SKEW
         :  skew = atof(opt->val)
         ;  break
         ;

            case MY_OPT_GEN
         :  template_n_nodes = atoi(opt->val)
         ;  break
         ;

            case MY_OPT_IMX
         :  mcxIOrenew(xfmx, opt->val, NULL)
         ;  break
         ;

            case MY_OPT_PLUS
         :  case MY_OPT_WB
         :  plus = TRUE
         ;  break
         ;

            case MY_OPT_OUT
         :  mcxIOrenew(xfout, opt->val, NULL)
         ;  break
         ;

            case MY_OPT_E_MAX
         :  if (!strcmp(opt->val, "copy"))
            e_max = -DBL_MAX
         ;  else
            e_max = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_E_MIN
         :  e_min = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_G_MIN
         :  g_min = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_G_MAX
         :  g_max = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_G_SDEV
         :  g_sdev = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_G_MEAN
         :  g_mean = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_G_RADIUS
         :  g_radius = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_N_RANGE
         :  n_range = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_N_SDEV
         :  n_sdev = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_N_RADIUS
         :  radius = atof(opt->val)
         ;  break
         ;  

            case MY_OPT_SHUFFLE
         :  N_shuffle = atoi(opt->val)
         ;  break
         ;  

            case MY_OPT_ADD
         :  N_add = atoi(opt->val)
         ;  break
         ;  
            
            case MY_OPT_REMOVE
         :  N_remove  = atoi(opt->val)
         ;  break
      ;  }
      }

   ;  if (template_n_nodes)
      mx =  mclxAllocZero
            (  mclvCanonical(NULL, template_n_nodes, 1.0)
            ,  mclvCanonical(NULL, template_n_nodes, 1.0)
            )
   ;  else
      mx =  mclxReadx
            (  xfmx
            ,  EXIT_ON_FAIL
            ,  MCLX_REQUIRE_GRAPH
            )

   ;  mx_diag = mclxDiagValues(mx, MCL_VECTOR_COMPLETE)

   ;  if (N_shuffle)
      mclxAdjustLoops(mx, mclxLoopCBremove, NULL)
   ;  else
      mclxSelectUpper(mx)
   
   ;  if (g_mean)
      {  if (!g_radius)
         {  if (g_sdev)
            g_radius = 2 * g_sdev
         ;  mcxWarn(me, "set radius to %.5f\n", g_radius)
      ;  }
      }

      offsets = mcxAlloc(sizeof offsets[0] * N_COLS(mx), EXIT_ON_FAIL)

   ;  N_edge = 0
   ;  for (i=0;i<N_COLS(mx);i++)
      {  offsets[i] = N_edge
      ;  N_edge += mx->cols[i].n_ivps
   ;  }

      if (N_edge < N_remove)
      {  mcxErr
         (  me
         ,  "removal count %ld exceeds edge count %ld"
         ,  (long) N_remove
         ,  (long) N_edge
         )
      ;  N_remove = N_edge
   ;  }

      ignore = RAND_MAX - (N_edge ? RAND_MAX %  N_edge : 0)

   ;  if (RAND_MAX / 2 < N_edge)
      mcxDie(1, me, "graph too large!")

   ;  n_shuffle = 0
   ;  if (N_shuffle)
      {  while (n_shuffle < N_shuffle)
         {  unsigned long rx = (unsigned long) random()
         ;  unsigned long ry = (unsigned long) random()
         ;  mclp* ivpll, *ivplr, *ivprl, *ivprr
         ;  dim edge_x, edge_y, *edge_px, *edge_py
         ;  ofs xro, yro, xlo, ylo = -1, vxo, vyo
         ;  long xl, xr, yl, yr
         ;  mclv* vecxl, *vecxr, *vecyl, *vecyr
         ;  double xlval, xrval, ylval, yrval

         ;  if (rx >= ignore || ry >= ignore)
            continue

         ;  edge_x = rx % N_edge
         ;  edge_y = ry % N_edge

         ;  if (!(edge_px = mcxBsearchFloor(&edge_x, offsets, N_COLS(mx), sizeof edge_x, dimCmp)))
            mcxDie(1, me, "edge %ld not found (max %ld)", (long) edge_x, (long) N_edge)

         ;  if (!(edge_py = mcxBsearchFloor(&edge_y, offsets, N_COLS(mx), sizeof edge_y, dimCmp)))
            mcxDie(1, me, "edge %ld not found (max %ld)", (long) edge_y, (long) N_edge)

         ;  vxo   =  edge_px - offsets
         ;  xl    =  mx->dom_cols->ivps[vxo].idx
         ;  vecxl =  mx->cols+vxo
         ;  xro   =  edge_x - offsets[vxo]

         ;  vyo   =  edge_py - offsets
         ;  yl    =  mx->dom_cols->ivps[vyo].idx
         ;  vecyl =  mx->cols+vyo
         ;  yro   =  edge_y - offsets[vyo]

                           /* Offset computation gone haywire */
         ;  if (xro >= vecxl->n_ivps || yro >= vecyl->n_ivps)
            mcxDie(1, me, "paradox 1 in %ld or %ld", xl, yl)

         ;  xr = vecxl->ivps[xro].idx
         ;  yr = vecyl->ivps[yro].idx
         ;  xrval = vecxl->ivps[xro].val
         ;  yrval = vecyl->ivps[yro].val

                           /* Impossible, should have graph */
         ;  vecxr = mclxGetVector(mx, xr, EXIT_ON_FAIL, NULL)
         ;  vecyr = mclxGetVector(mx, yr, EXIT_ON_FAIL, NULL)

                           /* check that we have four different nodes
                            * loops are not present so no need to check those
                           */
         ;  if (xl == yl || xl == yr || xr == yl || xr == yr)
            continue

         ;  if
            (  (0 > (xlo = mclvGetIvpOffset(vecxr, xl, -1)))
            || (0 > (ylo = mclvGetIvpOffset(vecyr, yl, -1)))
            )
            mcxDie
            (  1
            ,  me
            ,  "symmetry violation 1"
               " %ld not found in %ld/%ld OR %ld not found in %ld/%ld"
            ,  (long) xl, (long) vecxr->vid, (long) vecxr->n_ivps
            ,  (long) yl, (long) vecyr->vid, (long) vecyr->n_ivps
            )

                           /* Now:  xl yl :  ivpll
                            *       xl yr :  ivplr
                            *       xr yl :  ivprl
                            *       xr yr :  ivprr
                           */
         ;  xlval = vecxr->ivps[xlo].val
         ;  ylval = vecyr->ivps[ylo].val

         ;  ivpll = mclvGetIvp(vecxl, yl, NULL)
         ;  ivplr = mclvGetIvp(vecxl, yr, NULL)
         ;  ivprl = mclvGetIvp(vecxr, yl, NULL)
         ;  ivprr = mclvGetIvp(vecxr, yr, NULL)

         ;  if
            (  (ivpll && !mclvGetIvp(vecyl, xl, NULL))
            || (ivplr && !mclvGetIvp(vecyr, xl, NULL))
            || (ivprl && !mclvGetIvp(vecyl, xr, NULL))
            || (ivprr && !mclvGetIvp(vecyr, xr, NULL))
            )
            mcxDie(1, me, "symmetry violation 2")

         ;  if ((ivpll && ivplr) || (ivprl && ivprr))
            continue

         ;  {  if (!ivpll && !ivprr) 
               {        /* vecxl <-> xr   becomes vecxl <-> yl
                         * vecxr <-> xl   becomes vecxr <-> yr
                         * vecyl <-> yr   becomes vecyl <-> xl 
                         * vecyr <-> yl   becomes vecyr <-> xr
                        */
               ;  if
                  (  mclvReplace(vecxl, xro, yl, xrval)
                  || mclvReplace(vecyl, yro, xl, xrval)
                  || mclvReplace(vecxr, xlo, yr, ylval)
                  || mclvReplace(vecyr, ylo, xr, ylval)
                  )
                  mcxDie(1, me, "parallel replacement failure\n")
#if DEBUG
;fprintf(stderr, "parallel edge change remove  %d-%d  %d-%d add  %d-%d  %d-%d\n",
      vecxl->vid, xr, vecyr->vid, yl,  vecxl->vid, yl, vecyr->vid, xr)
#endif
            ;  }
               else if (!ivplr && !ivprl)
               {        /* vecxl -> xr   becomes vecxl <-> yr
                         * vecxr -> xl   becomes vecxr <-> yl
                         * vecyl -> yr   becomes vecyl <-> xr 
                         * vecyr -> yl   becomes vecyr <-> xl
                        */
                  if
                  (  mclvReplace(vecxl, xro, yr, xrval)
                  || mclvReplace(vecyr, ylo, xl, xlval)
                  || mclvReplace(vecxr, xlo, yl, yrval)
                  || mclvReplace(vecyl, yro, xr, yrval)
                  )
                  mcxDie(1, me, "cross replacement failure\n")
#if DEBUG
;fprintf(stderr, "cross edge change remove  %d-%d  %d-%d add  %d-%d  %d-%d\n",
      vecxl->vid, xr, vecyl->vid, yr,  vecxl->vid, yr, vecyl->vid, xr)
#endif
            ;  }
            }
            n_shuffle++
      ;  }

         mx_readd_diagonal(mx, mx_diag)
      ;  mclxWrite(mx, xfout, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
      ;  exit(0)
   ;  }

      n_remove = 0
   ;  while (n_remove < N_remove)
      {  unsigned long r = (unsigned long) random()
      ;  dim e, *ep
      ;  ofs xo, yo
      ;  long x
      ;  mclv* vecx

      ;  if (r >= ignore)
         continue

      ;  e = r % N_edge

      ;  if (!(ep = mcxBsearchFloor(&e, offsets, N_COLS(mx), sizeof e, dimCmp)))
         mcxDie(1, me, "edge %ld not found (max %ld)", (long) e, (long) N_edge)

      ;  xo = ep - offsets
;if(DEBUG)fprintf(stderr, "have e=%d o=%d\n", (int) e, (int) xo)
      ;  x = mx->dom_cols->ivps[xo].idx
      ;  vecx = mx->cols+xo;
      ;  yo = e - offsets[xo]

      ;  if (yo >= vecx->n_ivps)
         mcxDie
         (  1
         ,  me
         ,  "vector %ld has not enough entries: %ld < %ld"
         ,  (long) vecx->vid
         ,  (long) vecx->n_ivps
         ,  (long) yo
         )

      ;  if (!vecx->ivps[yo].val)
         continue

      ;  vecx->ivps[yo].val = 0.0

;if(DEBUG)fprintf(stderr, "remove [%d] %ld %ld\n", (int) n_remove, (long) x, (long) vecx->ivps[yo].idx)
      ;  n_remove++
   ;  }

         /* NOTE we work with *upper* matrix; this counts graph edges.
         */
      N_edge = mclxNrofEntries(mx) - N_remove
   ;  ignore = RAND_MAX - (RAND_MAX % N_COLS(mx))

   ;  n_add = 0
   ;  while (n_add < N_add)
      {  unsigned long r = (unsigned long) random()
      ;  unsigned long s = (unsigned long) random()
      ;  double val
      ;  mclp* ivp

      ;  dim xo = r % N_COLS(mx)
      ;  dim yo = s % N_COLS(mx)

      ;  long x = mx->dom_cols->ivps[xo].idx
      ;  long y = mx->dom_cols->ivps[yo].idx

      ;  if (N_edge >= N_COLS(mx) * (N_COLS(mx)-1) / 2)
         break

      ;  if (x >= y)       /* never add loops */
         continue

      ;  ivp = mclvGetIvp(mx->cols+xo, y, NULL)

      ;  if (ivp && ivp->val)
         continue

      ;  N_edge++
      ;  if (g_mean)
         {  do
            {  val = mcxNormalCut(g_radius, g_sdev)
            ;  if (skew)
               {  val = (g_radius + val) / (2 * g_radius)
               ;  val = pow(val, skew)
               ;  val = (val * 2 * g_radius) - g_radius
            ;  }
               val += g_mean
         ;  }
            while (g_min < g_max && (val < g_min || val > g_max))
      ;  }
         else
         {  val = (((unsigned long) random()) * 1.0) / RAND_MAX
         ;  if (skew)
            val = pow(val, skew)
         ;  val = e_min + val * (e_max - e_min)
      ;  }

         if (!val)
         continue

;if(DEBUG)fprintf(stderr, "add [%d] %ld %ld value %f\n", (int) n_add, (long) x, (long) y, val)
      ;  mclvInsertIdx(mx->cols+xo, y, val)
      ;  n_add++
   ;  }

      if (radius)
      {  for (i=0;i<N_COLS(mx);i++)
         {  mclp* ivp = mx->cols[i].ivps, *ivpmax = ivp + mx->cols[i].n_ivps
;if(DEBUG)fprintf(stderr, "here %d\n", (int) i)
         ;  while (ivp < ivpmax)
            {  double val = ivp->val
            ;  if (val >= e_min && val <= e_max)
               {  double r = mcxNormalCut(n_range * n_sdev, n_sdev)
               ;  val += radius * (r / (n_range * n_sdev))

;if(DEBUG)fprintf(stderr, "mod [%d][%d] %.5f to %.5f (r = %.4f)\n", (int) mx->cols[i].vid, (int) ivp->idx, ivp->val, val, r)
               ;  if (e_min > e_max || (val >= e_min && val <= e_max))
                  ivp->val = val
            ;  }
               ivp++
         ;  }
         }
      }

      mclxUnary(mx, fltxCopy, NULL)
   ;  mclxAddTranspose(mx, 0.0)
   ;  mx_readd_diagonal(mx, mx_diag)

   ;  if (plus)
      mclxbWrite(mx, xfout, RETURN_ON_FAIL)
   ;  else
      mclxWrite(mx, xfout, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   ;  return 0
;  }


