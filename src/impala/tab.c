/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
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



void mclTabFree
(  mclTab**       tabpp
)
   {  mclTab* tab = *tabpp
   ;  if (tab && tab->labels)
      {  char** lblpp = tab->labels
      ;  while(*lblpp)
         {  mcxFree(*lblpp)
         ;  lblpp++
      ;  }
         mcxFree(tab->labels)
      ;  mclvFree(&(tab->domain))
      ;  mcxFree(tab)
      ;  *tabpp = NULL
   ;  }
   }


mclTab*   mclTabRead
(  mcxIO*         xf
,  const mclv*    dom
,  mcxOnFail      ON_FAIL
)
   {  mclTab* tab    =  mcxAlloc(sizeof(mclTab), EXIT_ON_FAIL)
   ;  mcxTing* line  =  mcxTingEmpty(NULL, 100)
   ;  mclpAR*     ar =  mclpARensure(NULL, 100)
   ;  const char* me =  "mclTabRead"
   ;  int c_seen     =  0
   ;  int n_ivps     =  0  
   ;  long vid       =  0
   ;  long vidprev   =  -1

   ;  int sz_labels  =  80
   ;  char** labels  =  mcxAlloc(sz_labels * sizeof(char*), EXIT_ON_FAIL)

   ;  tab->domain    =  mclvResize(NULL, 0)
   ;  tab->labels    =  NULL
   ;  tab->na        =  "?"

   ;  while(STATUS_OK == mcxIOreadLine(xf, line, MCX_READLINE_CHOMP))
      {  mclp *ivp
      ;  char* c

      ;  if (!(c = mcxStrChrAint(line->str, isspace, line->len)) || *c == '#')
         continue
      ;  if (sscanf(line->str, "%ld%n", &vid, &c_seen) != 1)
         {  mcxErr(me, "expected vector index")
         ;  goto fail
      ;  }
         if (vid <= vidprev)
         {  mcxErr
            (me, "order violation: <%ld> follows <%ld>", vid, vidprev)
         ;  goto fail
      ;  }
         if (dom && (!dom->n_ivps || dom->ivps[n_ivps].idx != vid))
         {  mcxErr
            (me, "domain violation: unexpected index <%ld>", vid)
         ;  goto fail
      ;  }

         while (isspace((unsigned char)*(line->str+c_seen)))
         c_seen++
      
      ;  n_ivps++

      ;  if (ar->n_alloc <= n_ivps)
         mcxResize
         (  &(ar->ivps)
         ,  sizeof(mclp)
         ,  &(ar->n_alloc)
         ,  n_ivps * 2
         ,  EXIT_ON_FAIL   /* fixme; respect ON_FAIL */
         )

      ;  ivp = ar->ivps + n_ivps - 1
      ;  ivp->idx = vid
      ;  ivp->val = 1.0
      ;  ar->n_ivps = n_ivps
      ;  vidprev = vid

      ;  if (sz_labels <= n_ivps)
         mcxResize
         (  &labels
         ,  sizeof(char*)
         ,  &sz_labels
         ,  n_ivps * 2
         ,  EXIT_ON_FAIL   /* fixme; respect ON_FAIL */
         )

      ;  labels[n_ivps-1] = mcxTingSubStr(line, c_seen, -1)
   ;  }

      if (dom && ar->n_ivps != dom->n_ivps)
      {  mcxErr
         (  me
         ,  "label count mismatch: got/need %ld/%ld"
         ,  (long) ar->n_ivps
         ,  (long) dom->n_ivps
         )
      ;  goto fail
   ;  }

      mclvFromIvps_x(tab->domain, ar->ivps, ar->n_ivps, 0, 0, NULL, NULL)
   ;  mcxResize         /* return unused memory */
      (  &labels
      ,  sizeof(char*)
      ,  &sz_labels
      ,  n_ivps + 1
      ,  EXIT_ON_FAIL   /* fixme; respect ON_FAIL */
      )
   ;  labels[n_ivps] = NULL
   ;  tab->labels = labels
   ;  mclpARfree(&ar)

   ;  if (0)
      {  fail
      :  mcxIOpos(xf, stderr)
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


char* mclTabGet
(  mclTab*  tab
,  long     id
,  long*    ofs
)
   {  long old_ofs = ofs ? *ofs : -1
   ;  long new_ofs = mclvGetIvpOffset(tab->domain, id, old_ofs)
   ;  if (ofs)
      *ofs = new_ofs
   ;  return new_ofs >= 0 ? tab->labels[new_ofs] : tab->na
;  }



