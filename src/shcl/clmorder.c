/*      Copyright (C) 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <string.h>
#include <stdio.h>

#include "report.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/io.h"
#include "impala/compose.h"
#include "impala/iface.h"
#include "impala/app.h"
#include "mcl/clm.h"

#include "util/io.h"
#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"


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

mcxstatus rerank
(  long     clid
,  int      depth
,  mclx**   clxs
,  mclx**   maps
,  mclv*    ordv
,  double*  ord
)  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxIO       *xfin    =  mcxIOnew("-", "r")
   ;  mcxIO       *xfout   =  mcxIOnew("out.order", "w")
   ;  mcxTing     *fname   =  NULL
   ;  mclv*       ordv     =  mclvInit(NULL)
   ;  double      ord      =  1.0
   ;  long        i        =  0

   ;  mclx       **clxs    =  NULL
   ;  mclx       **maps    =  NULL
   ;  mclx        *cls     =  NULL
   ;  int         n_clxs   =  0

   ;  const char  *me      =  "clmorder"
   ;  int         a        =  1

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  clxs = mcxAlloc(argc*sizeof(mclx*), EXIT_ON_FAIL)
   ;  maps = mcxAlloc(argc*sizeof(mclx*), EXIT_ON_FAIL)

   ;  while(a<argc)
      {  mcxIOnewName(xfin, argv[a])
      ;  cls = mclxRead(xfin, EXIT_ON_FAIL)
      ;  mclxColumnsRealign(cls, mclvSizeRevCmp)
      ;  mclxMakeCharacteristic(cls)
      ;  mcxIOclose(xfin)

/* mcx /out.order-1 lm tp /out.order-2 lm mul tp /x1 wm
 */
      ;  if (!n_clxs)
         clxs[n_clxs] = cls
      ;  else
         {  mclx* clinv, *map

         ;  clxs[n_clxs]   =  mclcMeet(clxs[n_clxs-1], cls, EXIT_ON_FAIL)
         ;  mclxColumnsRealign(clxs[n_clxs], mclvSizeRevCmp)

         ;  clinv          =  mclxTranspose(clxs[n_clxs])
         ;  map            =  mclxCompose(clinv, clxs[n_clxs-1], 0)
         ;  maps[n_clxs-1] =  map

         ;  fname = mcxTingPrint(fname, "out.map.%d-%d", (int) a-1, (int) a)
         ;  mcxIOnewName(xfout, fname->str)
         ;  mclxWrite(map, xfout, MCLXIO_VALUE_GETENV, EXIT_ON_FAIL)
         ;  mcxIOclose(xfout)

         ;  mclxFree(&cls)
         ;  mclxFree(&clinv)
      ;  }

         fname = mcxTingPrint(fname, "out.order.%d", (int) a)
      ;  mcxIOnewName(xfout, fname->str)
      ;  mclxWrite(clxs[n_clxs], xfout, MCLXIO_VALUE_NONE, EXIT_ON_FAIL)
      ;  mcxIOclose(xfout)

      ;  n_clxs++
      ;  a++
   ;  }

      if (!n_clxs)
      return 0

   ;  ordv           =  mclvClone(clxs[0]->dom_rows)
   ;  clxs[n_clxs]   =  NULL
   ;  maps[n_clxs-1] =  NULL

   ;  {  mclx* cls0  =  clxs[0]
      ;  for (i=0;i<cls0->dom_cols->n_ivps;i++)
         if (rerank(cls0->dom_cols->ivps[i].idx, 0,  clxs, maps, ordv, &ord))
         break

      ;  if (i == cls0->dom_cols->n_ivps)
         {  mcxTell
            (me, "reordering successful (%ld entries)", (long) ordv->n_ivps)
         ;  mclvSort(ordv, mclpValCmp)
         ;  mclvaDump2(ordv, stdout, 0, -1, "\n", MCLVA_DUMP_EOV_NO)
      ;  }
      }

      return(0)
;  }


mcxstatus rerank
(  long     clid
,  int      depth
,  mclx**   clxs
,  mclx**   maps
,  mclv*    ordv
,  double*  ord
)
   {  mclx* cls = clxs[depth]
   ;  mclx* map = maps[depth]
   ;  mcxstatus status = STATUS_FAIL
   ;  mclv* domv
   ;  const char* errmsg = "all's well"

;if (0) fprintf(stderr, "in at %ld/%d %f\n", clid, depth, *ord) 

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

         if (map)
         {  mclv* mapv = mclxGetVector(map, clid, RETURN_ON_FAIL, NULL)
         ;  long i
         ;  if (!map)
            {  errmsg = "no mapv"
            ;  break
         ;  }

            for (i=0;i<mapv->n_ivps;i++)
            if (rerank(mapv->ivps[i].idx, depth+1, clxs, maps, ordv, ord))
            break

         ;  if (i != mapv->n_ivps)
            break
      ;  }
         else
         {  long i
         ;  for (i=0;i<domv->n_ivps;i++)
            {  mclvInsertIdx(ordv, domv->ivps[i].idx, *ord)
            ;  (*ord)++
         ;  }
         }

         status = STATUS_OK
      ;  break
   ;  }
      
      if (status)
      {  mcxErr("rerank", "panic <%s> clid/depth %ld/%d", errmsg, clid, depth)
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }

