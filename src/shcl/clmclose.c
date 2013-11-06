/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <string.h>
#include <stdio.h>

#include "clm.h"
#include "report.h"
#include "clmclose.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/app.h"
#include "impala/iface.h"

#include "mcl/interpret.h"

#include "clew/scan.h"
#include "clew/clm.h"

#include "util/io.h"
#include "util/err.h"
#include "util/types.h"
#include "util/opt.h"
#include "util/minmax.h"


#include "impala/matrix.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/compose.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "taurus/ilist.h"
#include "taurus/la.h"

#include "clew/clm.h"

#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"

static const char* me = "clmclose";

enum
{  MY_OPT_IMX = CLM_DISP_UNUSED
,  MY_OPT_DOMAIN
,  MY_OPT_OUTPUT
,  MY_OPT_CCBOUND
,  MY_OPT_TABIN
,  MY_OPT_MXOUT
,  MY_OPT_TABOUT
,  MY_OPT_TABXOUT
,  MY_OPT_MAPOUT
,  MY_OPT_TF
,  MY_OPT_CAN
}  ;


mcxOptAnchor closeOptions[] =
{  {  "-write-cc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUT
   ,  "<fname>"
   ,  "output cluster/connected-component file"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG | MCX_OPT_REQUIRED
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "input matrix file, presumably dumped mcl iterand or dag"
   }
,  {  "-cc-bound"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CCBOUND
   ,  "<num>"
   ,  "select selected components of size at least <num>"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_TABIN
   ,  "<fname>"
   ,  "read tab file"
   }
,  {  "-write-tab"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_TABOUT
   ,  "<fname>"
   ,  "write tab file of selected domain"
   }
,  {  "-write-tabx"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_TABXOUT
   ,  "<fname>"
   ,  "write tab file of deselected domain"
   }
,  {  "-write-matrix"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_MXOUT
   ,  "<fname>"
   ,  "write matrix of selected domain"
   }
,  {  "-write-map"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_MAPOUT
   ,  "<fname>"
   ,  "write mapping"
   }
,  {  "-dom"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DOMAIN
   ,  "<fname>"
   ,  "input domain file"
   }
,  {  "-tf"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TF
   ,  "<tf-spec>"
   ,  "first apply tf-spec to matrix"
   }
,  {  "--canonical"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_CAN
   ,  NULL
   ,  "make result matrix canonical"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


static mcxIO*  xfclout  =  (void*) -1;
static mcxIO*  xfmx     =  (void*) -1;
static mcxIO*  xftabin  =  (void*) -1;
static mcxIO*  xftabout =  (void*) -1;
static mcxIO*  xftabxout=  (void*) -1;
static mcxIO*  xfmapout =  (void*) -1;
static mcxIO*  xfmxout  =  (void*) -1;
static mcxIO*  xfdom    =  (void*) -1;
static mcxTing* tfting  =  (void*) -1;
static dim     ccbound_num  =  -1;
static mcxbool canonical=  -1;


static mcxstatus closeInit
(  void
)
   {  xfclout  =  NULL
   ;  xfmapout =  NULL
   ;  xfmxout  =  NULL
   ;  xftabout =  NULL
   ;  xftabxout=  NULL
   ;  xftabin  =  NULL
   ;  xfmx     =  NULL
   ;  xfdom    =  NULL
   ;  tfting   =  NULL
   ;  ccbound_num  =  0
   ;  canonical=  FALSE
   ;  return STATUS_OK
;  }


static mcxstatus closeArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_IMX
      :  xfmx = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_OUTPUT
      :  xfclout = mcxIOnew(val, "w")
      ;  break
      ;

         case MY_OPT_DOMAIN
      :  xfdom= mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_MAPOUT
      :  xfmapout = mcxIOnew(val, "w")
      ;  break
      ;

         case MY_OPT_MXOUT
      :  xfmxout = mcxIOnew(val, "w")
      ;  break
      ;

         case MY_OPT_CCBOUND
      :  ccbound_num = atoi(val)
      ;  break
      ;

         case MY_OPT_TABXOUT
      :  xftabxout = mcxIOnew(val, "w")
      ;  break
      ;

         case MY_OPT_TABOUT
      :  xftabout = mcxIOnew(val, "w")
      ;  break
      ;

         case MY_OPT_TABIN
      :  xftabin = mcxIOnew(val, "r")
      ;  break
      ;

         case MY_OPT_CAN
      :  canonical = TRUE
      ;  break
      ;

         case MY_OPT_TF
      :  tfting = mcxTingNew(val)
      ;  break
      ;

         default
      :  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }


static double mclv_check_ccbound
(  const mclv* vec
,  void* data
)
   {  dim bound = *((dim*) data)
   ;  return vec->n_ivps >= bound ? 1.0 : 0.0
;  }


static mcxstatus closeMain
(  int                  argc
,  const char*          argv[]
)
   {  mclx *dom =  NULL, *cc = NULL, *ccbound = NULL
   ;  mclx *mx  =  NULL
   ;  mclx *map  =  NULL
   ;  const mclv *ccbound_rows = NULL
   ;  const mclv *ccbound_cols = NULL
   ;  mclTab* tab = NULL
   ;  dim N_start = 0
   ;  dim N_bound = 0

   ;  if (!xfmx)
      mcxDie(1, me, "-imx argument required")

   ;  if ((xftabout || xftabxout) && !xftabin)
      mcxDie(1, me, "-write-tab currently requires -tab")

   ;  mx = mclxReadx(xfmx, EXIT_ON_FAIL, MCLX_REQUIRE_GRAPH)
   ;  dom =    xfdom
            ?  mclxRead(xfdom, EXIT_ON_FAIL)
            :  NULL

   ;  if (dom && !MCLD_EQUAL(dom->dom_rows, mx->dom_cols))
      mcxDie(1, me, "domains not equal")

   ;  N_start = N_ROWS(mx)

   ;  if (xftabout || xftabxout)
      tab = mclTabRead(xftabin, mx->dom_cols, EXIT_ON_FAIL)

   ;  if (tfting)
      {  mclpAR* tfar = mclpTFparse(NULL, tfting)
      ;  if (!tfar)
         mcxDie(1, me, "errors in tf-spec")
      ;  mclxUnaryList(mx, tfar)
   ;  }

      cc = clmComponents(mx, dom)

                              /*
                               * thin out domain based on cc
                              */
   ;  if (ccbound_num)
      {  ccbound_cols = mclxColSelect(cc, mclv_check_ccbound, &ccbound_num)
      ;  ccbound_rows = mclgUnionv(cc, ccbound_cols, NULL, SCRATCH_READY, NULL)
   ;  }
      else
         ccbound_cols = cc->dom_cols
      ,  ccbound_rows = cc->dom_rows

   ;  N_bound = ccbound_rows->n_ivps

   ;  if
      (  canonical
      && (! (  map
            =  mclxMakeMap
               (  mclvClone(ccbound_rows)
               ,  mclvCanonical(NULL, ccbound_rows->n_ivps, 1.0)
               )
            )
         )
      )
      mcxDie(1, me, "cannot make a map")

   ;  if (N_bound < N_start)
      ccbound = mclxSub(cc, ccbound_cols, ccbound_rows)
   ;  else
      ccbound = cc

   ;  if (xfmxout)
      {  if (N_bound < N_start)      /* thin out matrix */
         {  mclx* sub = mclxSub(mx, ccbound_rows, ccbound_rows)
         ;  mclxFree(&mx)
         ;  mx = sub
      ;  }

         if (map)
         {  if (mclxMapRows(mx, map))
            mcxDie(1, me, "cannot map rows")

         ;  if (mclxMapCols(mx, map))
            mcxDie(1, me, "cannot map cols")
      ;  }
         mclxWrite(mx, xfmxout, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   ;  }


      if (xftabxout)
      {  mclv* deselect = mcldMinus(tab->domain, ccbound->dom_rows, NULL)
      ;  if (canonical)
         NOTHING                 /* fixme: DOIT */
      ;  else
         mclTabWrite(tab, xftabxout, deselect, RETURN_ON_FAIL)
      ;  mclvFree(&deselect)
   ;  }


      if (xftabout)
      {  mclTab* tabsel = mclTabSelect(tab, ccbound->dom_rows), *tabout
      ;  mclTabFree(&tab)
      ;  if (map)
            tabout = mclTabMap(tabsel, map)
         ,  mclTabFree(&tabsel)
      ;  else
         tabout = tabsel
      ;  if (!tabout)
         mcxDie(1, me, "no tab, baton")
      ;  mclTabWrite(tabout, xftabout, NULL, RETURN_ON_FAIL)

      ;  mclTabFree(&tabsel)
      ;  mclTabFree(&tabout)
   ;  }

      if (xfclout)
      {  if (map)
         {  if (mclxMapRows(ccbound, map))
            mcxDie(1, me, "cannot map rows")

         ;  if (mclxMapCols(ccbound, NULL))
            mcxDie(1, me, "cannot map cols")
      ;  }
         mclxaWrite(ccbound, xfclout, MCLXIO_VALUE_NONE, RETURN_ON_FAIL)
   ;  }

      if (xfmapout && map)
      mclxaWrite(map, xfmapout, MCLXIO_VALUE_NONE, RETURN_ON_FAIL)

   ;  mcxIOfree(&xfmx)
   ;  mcxIOfree(&xfclout)

   ;  mclxFree(&mx)
   ;  mclxFree(&cc)
   ;  mclxFree(&dom)
   ;  return STATUS_OK
;  }


static mcxDispHook closeEntry
=  {  "close"
   ,  "close [options] -imx <mx file>"
   ,  closeOptions
   ,  sizeof(closeOptions)/sizeof(mcxOptAnchor) - 1
   ,  closeArgHandle
   ,  closeInit
   ,  closeMain
   ,  0
   ,  0
   ,  MCX_DISP_DEFAULT
   }
;


mcxDispHook* mcxDispHookClose
(  void
)
   {  return &closeEntry
;  }


