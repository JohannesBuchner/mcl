/*   (C) Copyright 2005, 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* TODO
 *
 *  - only works for canonical graphs.
 *  - optionally reorder clusterings; presumably based on count or center.
 *  # output a map matrix (one of maps?)
 *  - remove recursion.
 *  ? permute graph according to map.
 *  ? permute stack according to map. 
*/

/*  This program reorders indices so that that ordering represents
    blocks from different clusterings (which are preferably more or
    less hierarchically organized).

    It first transforms a list of input matrices by taking successive
    meets, yielding a perfectly nested list of clusterings.  The
    clusterings should be given ordered from coarse to fine-grained.
    The meets are all internally reordered so that largest clusters
    come first.

    The resulting Babushka structure is then traversed to create the
    ordering.
*/

#include <string.h>
#include <stdio.h>

#include "clm.h"
#include "report.h"
#include "clmorder.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/compile.h"

#include "impala/matrix.h"
#include "impala/cat.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/app.h"

#include "clew/clm.h"

static const char* me = "clmorder";

enum
{  MY_OPT_OUTPUT = CLM_DISP_UNUSED
,  MY_OPT_REORDER
,  MY_OPT_WRITEMAP
}  ;


static mcxOptAnchor orderOptions[] =
{  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUTPUT
   ,  "<fname>"
   ,  "output file name"
   }
,  {  "--reorder"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_REORDER
   ,  NULL
   ,  "(DEFUNCT) reorder from fine-grained to coarse-grained"
   }
,  {  "--write-maps"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_WRITEMAP
   ,  NULL
   ,  "write matrix map files"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


static mcxIO*  xfout    =  (void*) -1;
static mcxIO*  xfmap    =  (void*) -1;
static mcxbool reorder  =  -1;
static mcxbool write_map  =  -1;


static mcxstatus orderInit
(  void
)
   {  xfout = mcxIOnew("-", "w")
   ;  xfmap = mcxIOnew("-", "w")
   ;  reorder = FALSE
   ;  write_map = FALSE
   ;  return STATUS_OK
;  }


static mcxstatus orderArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case MY_OPT_REORDER
      :  reorder = TRUE
      ;  break
      ;

         case MY_OPT_WRITEMAP
      :  write_map = TRUE
      ;  break
      ;

         case MY_OPT_OUTPUT
      :  mcxIOnewName(xfout, val)
      ;  break
      ;

         default
      :  return STATUS_FAIL
      ;
      }
      return STATUS_OK
;  }



static mcxstatus rerank
(  long     clid
,  int      depth
,  mclxCat*  st
,  mclx**   maps
,  mclx*    mxmap
,  ofs*     ord
)  ;


static mcxstatus orderMain
(  int          argc_unused  cpl__unused
,  const char*  argv[]
)
   {  mcxIO       *xfin    =  mcxIOnew(argv[0], "r")
   ;  mcxTing     *fname   =  NULL
   ;  ofs         ord      =  0
   ;  dim         i        =  0

   ;  mclx       **maps    =  NULL     /* fixme/document purpose */
   ;  mclx        *cls     =  NULL
   ;  mclx*       mxmap    =  NULL
   ;  mcxbits     bits     =     MCLX_REQUIRE_PARTITION
                              |  MCLX_PRODUCE_DOMSTACK
                              |  MCLX_REQUIRE_CANONICALC

   ;  mclxCat st
   ;  mcxstatus status = mclxCatRead(xfin, &st, 0, NULL, NULL, bits)

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)
   ;  mclx_app_init(stderr)

   ;  maps = mcxAlloc(st.n_level * sizeof(mclx*), EXIT_ON_FAIL)

   ;  if (status || !st.n_level)
      mcxDie(1, me, "not happy, you hear me ?- not happy")

#define MX(i) (st.level[i].mx)

   ;  mclxColumnsRealign(MX(st.n_level-1), mclvSizeRevCmp)
   ;  mclxMakeCharacteristic(MX(st.n_level-1))

   ;  for (i=st.n_level-1;i>0;i--)
      {  cls = st.level[i-1].mx
      ;  mclxColumnsRealign(cls, mclvSizeRevCmp)
      ;  mclxMakeCharacteristic(cls)

/* mcx /out.order-1 lm tp /out.order-2 lm mul tp /x1 wm */

      ;  {  mclx* clinv, *map

         ;  MX(i-1) = clmMeet(MX(i), cls)
         ;  mclxFree(&cls)
         ;  mclxColumnsRealign(MX(i-1), mclvSizeRevCmp)

         ;  clinv    =  mclxTranspose(MX(i-1))
         ;  map      =  mclxCompose(clinv, MX(i), 0)
         ;  maps[i]  =  map

         ;  if (write_map)
            {  fname = mcxTingPrint(fname, "out.map.%d-%d", (int) i, (int) i-1)
            ;  mcxIOnewName(xfmap, fname->str)
            ;  mclxWrite(map, xfmap, MCLXIO_VALUE_GETENV, EXIT_ON_FAIL)
            ;  mcxIOclose(xfmap)
         ;  }
         }
      }

      maps[0]  =  NULL
   ;  mxmap    =  mclxAllocZero
                  (mclvClone(MX(0)->dom_rows), mclvClone(MX(0)->dom_rows))

   ;  {  mclx* cls0  =  MX(st.n_level-1)
      ;  for (i=0;i<cls0->dom_cols->n_ivps;i++)
         if (rerank(cls0->dom_cols->ivps[i].idx, st.n_level-1,  &st, maps, mxmap, &ord))
         break

      ;  mcxIOopen(xfout, EXIT_ON_FAIL)

      ;  if (i == cls0->dom_cols->n_ivps)
         {  mcxTell(me, "reordering successful")
         ;  mclxWrite(mxmap, xfout, MCLXIO_VALUE_NONE, EXIT_ON_FAIL)
      ;  }
      }

      return(0)
;  }


static mcxstatus rerank
(  long     clid
,  int      height
,  mclxCat *st
,  mclx**   maps
,  mclx*    mxmap
,  ofs*     ord
)
   {  mclx* cls = st->level[height].mx
   ;  mclx* map = maps[height]
   ;  mcxstatus status = STATUS_FAIL
   ;  mclv* domv
   ;  const char* errmsg = "all's well"

;if (0) fprintf(stderr, "in at %ld/%ld %ld\n", (long) clid, (long) height, (long) *ord) 

   ;  while (1)
      {  if (!cls)
         {  errmsg = "no cls"
         ;  break
      ;  }

         domv = mclxGetVector(cls, clid, RETURN_ON_FAIL, NULL)

      ;  if (!domv)
         {  errmsg = "no domv"
         ;  break
      ;  }

         if (height)
         {  mclv* mapv = mclxGetVector(map, clid, RETURN_ON_FAIL, NULL)
         ;  dim i
         ;  if (!map)
            {  errmsg = "no mapv"
            ;  break
         ;  }

            for (i=0;i<mapv->n_ivps;i++)
            if (rerank(mapv->ivps[i].idx, height-1, st, maps, mxmap, ord))
            break

         ;  if (i != mapv->n_ivps)
            break
      ;  }
         else
         {  dim i
         ;  for (i=0;i<domv->n_ivps;i++)
            {  mclvInsertIdx(mxmap->cols+domv->ivps[i].idx, *ord, 1.0)
            ;  (*ord)++
         ;  }
         }

         status = STATUS_OK
      ;  break
   ;  }
      
      if (status)
      {  mcxErr("rerank", "panic <%s> clid/height %ld/%ld", errmsg, (long) clid, (long) height)
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }


mcxDispHook* mcxDispHookOrder
(  void
)
   {  static mcxDispHook orderEntry
   =  {  "order"
      ,  "order [options] <cl stack file>"
      ,  orderOptions
      ,  sizeof(orderOptions)/sizeof(mcxOptAnchor) - 1
      ,  orderArgHandle
      ,  orderInit
      ,  orderMain
      ,  1
      ,  1
      ,  MCX_DISP_MANUAL
      }
   ;  return &orderEntry
;  }
