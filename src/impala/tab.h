/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_label_h
#define impala_label_h

#include "vector.h"
#include "util/types.h"
#include "util/io.h"
#include "util/hash.h"


typedef struct
{  mclVector*  domain
;  char**      labels   /* size: domain->n_ivps+1, also NULL terminated */
;  mcxTing*    na       /* not available, returned if element not found */
;
}  mclTab       ;



/* If dom is nonNULL, demand equality.
*/

mclTab* mclTabRead
(  mcxIO*         xf
,  const mclv*    dom
,  mcxOnFail      ON_FAIL  
)  ;


mcxstatus mclTabWrite
(  mclTab*        tab
,  mcxIO*         xf
,  const mclv*    select   /* if NULL, use all */
,  mcxOnFail      ON_FAIL
)  ;


mcxstatus mclTabWriteDomain
(  mclv*          select
,  mcxIO*         xfout
,  mcxOnFail      ON_FAIL
)  ;


char* mclTabGet
(  const mclTab*  tab
,  long     id
,  long*    ofs
)  ;


/*    Assumes tings as keys; integer encoded as (char*) kv->val - (char*) NULL.
 *    labels in map are shallow pointer copies of (kv->key)->str.  When freeing
 *    hash, *do* free the ting, do not free the str!  - use mcxTingAbandon
 *    callback.
 *
 *    Future: optionalize this if needed.
*/

mclTab* mclTabFromMap
(  mcxHash* map
)  ;

mcxHash* mclTabHash
(  mclTab* tab
)  ;

void mclTabFree
(  mclTab**       tabpp
)  ;


#endif

