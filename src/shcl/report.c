/*      Copyright (C)  2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include "report.h"

#include "util/ting.h"
#include "util/err.h"
#include "impala/matrix.h"



void report_partition
(  const char* me
,  mclMatrix* cl
,  mcxTing* fname
,  int o
,  int m
,  int e
)
   {  mcxErr
      (  me
      ,  "Clustering in file <%s> is not a partition", fname->str
      )
   ;  mcxErr(me, "Overlap <%d> Missing <%d> Empt(y/ied) <%d>", o, m, e)
;  }

void report_exit
(  const char* me
,  mcxbits error
)
   {  if (error & (SHCL_ERROR_PARTITION | SHCL_ERROR_DOMAIN))
      mcxErr
      (  me
      ,  "Error. Use --adapt for enstricting clusterings and projecting domains"
      )
   ;  else
      mcxErr(me, "Fatal error")
   ;  mcxExit(error)
;  }


void report_domain
(  const char* me
,  int nlft
,  int nrgt
,  int meet
)
   {  mcxErr
      (  me
      ,  "left|right|new domain sizes: <%d><%d><%d>"
      ,  (int) nlft
      ,  (int) nrgt
      ,  (int) meet
      )
;  }


void report_fixit
(  const char* me
,  int n_fix
)
   {  mcxTell
      (  me
      ,     n_fix == 0
         ?  "I am fixing it as we scroll"
         :     n_fix == 1
            ?  "I'll fix you up once more"
            :     n_fix == 2
               ?  "Fixing it again"
               :     n_fix == 3
                  ?  "On the job"
                  :  "Don't mention it"
      )
;  }

