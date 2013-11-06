/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "tab.h"
#include "vector.h"

#include "util/types.h"
#include "util/ding.h"
#include "util/err.h"
#include "util/array.h"
#include "util/ting.h"
#include "util/hash.h"



void mclTabFree
(  mclTab**       tabpp
)
   {  mclTab* tab = *tabpp
   ;  if (tab)
      {  if (tab->labels)
         {  char** lblpp = tab->labels
         ;  while(*lblpp)
            {  mcxFree(*lblpp)
            ;  lblpp++
         ;  }
            mcxFree(tab->labels)
      ;  }
         mclvFree(&(tab->domain))
      ;  mcxTingFree(&(tab->na))
      ;  mcxFree(tab)
      ;  *tabpp = NULL
   ;  }
   }


/* fixme: get rid of mcxResize. */

mclTab*   mclTabRead
(  mcxIO*         xf
,  const mclv*    dom
,  mcxOnFail      ON_FAIL
)
   {  mclTab* tab    =  mcxAlloc(sizeof(mclTab), EXIT_ON_FAIL)
   ;  mcxTing* line  =  mcxTingEmpty(NULL, 100)
   ;  mclpAR*     ar =  mclpARensure(NULL, 100)
   ;  const char* me =  "mclTabRead"
   ;  mcxstatus status= STATUS_OK
   ;  int c_seen     =  0
   ;  int n_ivps     =  0  
   ;  long vid       =  0
   ;  long vidprev   =  -1

   ;  int sz_labels  =  80
   ;  char** labels  =  mcxAlloc(sz_labels * sizeof(char*), EXIT_ON_FAIL)

   ;  tab->domain    =  mclvResize(NULL, 0)
   ;  tab->labels    =  NULL
   ;  tab->na        =  mcxTingNew("?")

   ;  if ((status = mcxIOtestOpen(xf, ON_FAIL)))
      mcxErr(me, "stream open error")
   ;  else
      while(STATUS_OK == (status = mcxIOreadLine(xf, line, MCX_READLINE_CHOMP)))
      {  mclp *ivp
      ;  char* c
      ;  status = STATUS_FAIL

      ;  if (!(c = mcxStrChrAint(line->str, isspace, line->len)) || *c == '#')
         continue
      ;  if (sscanf(line->str, "%ld%n", &vid, &c_seen) != 1)
         {  mcxErr(me, "expected vector index")
         ;  break
      ;  }
         if (vid <= vidprev)
         {  mcxErr
            (me, "order violation: <%ld> follows <%ld>", vid, vidprev)
         ;  break
      ;  }
         if (dom && (!dom->n_ivps || dom->ivps[n_ivps].idx != vid))
         {  mcxErr
            (me, "domain violation: unexpected index <%ld>", vid)
         ;  break
      ;  }

         while (isspace((unsigned char)*(line->str+c_seen)))
         c_seen++
      
      ;  n_ivps++

      ;  if
         (  ar->n_alloc <= n_ivps
         && mcxResize
            (&(ar->ivps), sizeof(mclp), &(ar->n_alloc), n_ivps * 2, ON_FAIL)
         )
         break

      ;  ivp = ar->ivps + n_ivps - 1
      ;  ivp->idx = vid
      ;  ivp->val = 1.0
      ;  ar->n_ivps = n_ivps
      ;  vidprev = vid

      ;  if
         (  sz_labels <= n_ivps
         && mcxResize
            (  &labels
            ,  sizeof(char*)
            ,  &sz_labels
            ,  n_ivps * 2
            ,  ON_FAIL
            )
         )
         break

      ;  labels[n_ivps-1] = mcxTingSubStr(line, c_seen, -1)
      ;  status = STATUS_OK
   ;  }

      while (1)
      {  if (status == STATUS_FAIL)
         break
      ;  status = STATUS_FAIL
      ;  if (dom && ar->n_ivps != dom->n_ivps)
         {  mcxErr
            (  me
            ,  "label count mismatch: got/need %ld/%ld"
            ,  (long) ar->n_ivps
            ,  (long) dom->n_ivps
            )
         ;  break
      ;  }

         mclvFromIvps_x(tab->domain, ar->ivps, ar->n_ivps, 0, 0, NULL, NULL)
      ;  if (  mcxResize         /* return unused memory */
               (  &labels
               ,  sizeof(char*)
               ,  &sz_labels
               ,  n_ivps + 1
               ,  ON_FAIL
             ) )
         break
      ;  labels[n_ivps] = NULL
      ;  tab->labels = labels
      ;  mclpARfree(&ar)
      ;  status = STATUS_OK
      ;  break
   ;  }

      if (status)
      {  mcxIOpos(xf, stderr)
      ;  mclvFree(&(tab->domain))
      ;  mcxFree(tab->labels)
      ;  mcxFree(tab)
      ;  mcxTingFree(&line)
      ;  tab = NULL
      ;  if (ON_FAIL == EXIT_ON_FAIL)
            mcxErr(me, "curtains")
         ,  mcxExit(1)
   ;  }

      mcxTingFree(&line)
   ;  return tab
;  }


mcxHash* mclTabHash
(  mclTab* tab
)
   {  int size = tab->domain ? tab->domain->n_ivps : 0
   ;  mcxHash* h = mcxHashNew(2 * size, mcxTingDPhash, mcxTingCmp)
   ;  int i
   ;  for (i=0;i<tab->domain->n_ivps;i++)
      {  mcxTing* tg = mcxTingNew(tab->labels[i])
      ;  mcxKV* kv   = mcxHashSearch(tg, h, MCX_DATUM_INSERT)
      ;  unsigned long index

      ;  if (kv->key != tg)
         {  mcxErr("mclTabHash", "duplicate label <%s>", tg->str)
         ;  mcxTingFree(&tg)
         ;  continue
      ;  }

         index = tab->domain->ivps[i].idx
      ;  kv->val = UINT_TO_VOID index
   ;  }
      return h
;  }


mclTab* mclTabFromMap
(  mcxHash* map
)
   {  mclTab* tab    =  mcxAlloc(sizeof(mclTab), EXIT_ON_FAIL)
   ;  int n_keys     =  0
   ;  void** keys    =  mcxHashKeys(map, &n_keys, mcxTingCmp, 0)
                                          /* fixme: ssize_t iface */
   ;  long i         =  0
   ;  const char* me =  "mclTabFromMap"
   ;  int n_missing  =  0

   ;  if (!(tab->labels=mcxAlloc((n_keys+1) * sizeof(char*), RETURN_ON_FAIL)))
      return NULL

   ;  tab->domain    =  mclvCanonical(NULL, n_keys, 1.0)
   ;  tab->na        =  mcxTingNew("?")

   ;  for (i=0;i<n_keys;i++)
      tab->labels[i] = NULL

   ;  for (i=0;i<n_keys;i++)
      {  mcxTing* lbl   =  keys[i]
      ;  mcxKV*  kv     =  mcxHashSearch(lbl, map, MCX_DATUM_FIND)
      ;  unsigned long  idx = kv ? VOID_TO_UINT kv->val : 0

      ;  if (!kv)
         {  mcxErr("mclTabFromMap panic", "cannot retrieve <%s>!?", lbl->str)
         ;  return NULL
      ;  }
         if (idx >= n_keys)
         {  mcxErr("mclTabFromMap panic", "index %ld out of bounds", idx)
         ;  return NULL
      ;  }

         if (tab->labels[idx])
         mcxErr
         (  "mclTabFromMap panic"
         ,  "duplicates for index %lu: <%s> <%s>"
         ,  idx
         ,  lbl->str
         ,  tab->labels[idx]
         )
      ;  else
         tab->labels[idx] =  lbl->str
;if(0)fprintf(stderr, "TAB %lu %s\n", idx, lbl->str)
   ;  }

      tab->labels[n_keys] = NULL

   ;  for (i=0;i<n_keys;i++)
      if (!tab->labels[i])
      {  mcxTing* tmp = mcxTingPrint(NULL, "%s%d", tab->na->str, ++n_missing)
      ;  mcxErr(me, "mapping missing %ld index to %s", i, tmp->str)
      ;  tab->labels[i] = mcxTinguish(tmp)
   ;  }

      mcxFree(keys)
   ;  return tab
;  }



mcxstatus mclTabWriteDomain
(  mclv*          select
,  mcxIO*         xfout
,  mcxOnFail      ON_FAIL
)
   {  int i
   ;  if (mcxIOtestOpen(xfout, ON_FAIL))
      return STATUS_FAIL

   ;  for (i=0;i<select->n_ivps;i++)
      {  long idx = select->ivps[i].idx
      ;  fprintf(xfout->fp, "%ld\t%ld\n", idx, idx)
   ;  }
      mcxTell
      (  "mclIO"
      ,  "wrote %ld tab entries to stream <%s>"
      ,  (long) select->n_ivps
      ,  xfout->fn->str
      )
   ;  return STATUS_OK
;  }



mcxstatus mclTabWrite
(  mclTab*        tab
,  mcxIO*         xfout
,  const mclv*    select   /* if NULL, use all */
,  mcxOnFail      ON_FAIL
)
   {  long label_o = -1, i
   ;  long miss = 1

   ;  if (!tab)
      {  mcxErr("mclTabWrite", "no tab! target file: <%s>", xfout->fn->str)
      ;  return STATUS_FAIL
   ;  }

      if (!select)
      select = tab->domain

   ;  if (mcxIOtestOpen(xfout, ON_FAIL))
      return STATUS_FAIL

   ;  for (i=0;i<select->n_ivps;i++)
      {  long idx = select->ivps[i].idx
      ;  const char* label = mclTabGet(tab, idx, &label_o)
      ;  if (label == tab->na->str)
            mcxErr("mclTabWrite", "warning index %ld not found", idx)
         ,  fprintf(xfout->fp, "%ld\t%s%ld\n", idx, label, miss)
      ;  else
         fprintf(xfout->fp, "%ld\t%s\n", idx, label)
   ;  }
      mcxTell
      (  "mclIO"
      ,  "wrote %ld tab entries to stream <%s>"
      ,  (long) select->n_ivps
      ,  xfout->fn->str
      )
   ;  return STATUS_OK
;  }



char* mclTabGet
(  const mclTab*  tab
,  long     id
,  long*    ofs
)
   {  long old_ofs = ofs ? *ofs : -1
   ;  long new_ofs = mclvGetIvpOffset(tab->domain, id, old_ofs)
   ;  if (ofs)
      *ofs = new_ofs
   ;  return new_ofs >= 0 ? tab->labels[new_ofs] : tab->na->str
;  }



