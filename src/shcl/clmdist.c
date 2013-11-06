/*      Copyright (C) 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO
 * -  logical tab-separated output, line based or pair-based.
 *
 *    fixme: keep this domain error information. (deline ala scline ?
 *    "{%d,%d}"
 *    N_ROWS(cl1) - meet->n_ivps
 *    N_ROWS(cl2) - meet->n_ivps
 *
*/

#include <string.h>
#include <stdio.h>

#include "report.h"

#include "impala/matrix.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "taurus/ilist.h"
#include "taurus/la.h"
#include "mcl/clm.h"

#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"


#define  DIST_SPLITJOIN 1
#define  DIST_VARINF    2
#define  DIST_MIRKIN    4
#define  DIST_SCATTER   8

#define  DIST_MAIN      (DIST_SPLITJOIN | DIST_VARINF | DIST_MIRKIN)
/* fixme: ugly hack. unify setcount with others, support simultaneous output */

#define  PART_LOWER     1
#define  PART_DIAGONAL  2
#define  PART_UPPER     4

#define  DO_LOWER       (parts & PART_LOWER ? 1 : 0)
#define  DO_UPPER       (parts & PART_UPPER ? 1 : 0)
#define  DO_DIAGONAL    (parts & PART_DIAGONAL ? 1 : 0)

/*
 * clmdist will enstrict the clusterings, and possibly project them onto
 * the meet of the domains if necessary.
*/


typedef struct
{  mclMatrix*  cl
;  mcxTing*    name
;
}  clnode      ;


enum
{  D_SJA =  0
,  D_SJB
,  D_JQA
,  D_JQB
,  D_VIA
,  D_VIB
,  D_SCA
,  D_SCB
,  N_DIST
}  ;


typedef struct
{  double  vals[N_DIST]
;  
}  dstnode     ;


const char* usagelines[] =
{  "Usage: clmdist [options] <cl file> <cl file>+"
,  "where each <cl file> is a matrix in MCL format encoding a clustering."
,  ""
,  "Options: [--adapt] [-mode <sj|vi|jq|sc>]"
,  "  [-digits k] [-width k]"
,  "  [--noindex]"
,  "  [-parts <l|d|u>+]"
,  "Mode sj: Split/join distance"
,  "Mode vi: Variance of information"
,  "Mode ehd: Edge hamming distance, aka Mirkin metric"
,  "Mode mk: Edge hamming distance, (Mirkin metric)"
,  NULL
}  ;


int main
(  int                  argc
,  const char*          argv[]
)
   {  int               i, j, x
   ;  int n_clusterings =  0
   ;  clnode*  cls      =  mcxAlloc(argc * sizeof(clnode), EXIT_ON_FAIL)
   ;  dstnode* dists    =  mcxAlloc(argc * argc * sizeof(dstnode), EXIT_ON_FAIL)
   ;  const char* me    =  "clmdist"
   ;  int adapt         =  0
   ;  int index         =  1
   ;  int a             =  1
   ;  int mode          =  0
   ;  int n_err         =  0
   ;  int width         =  15
   ;  int digits        =  2
   ;  int parts         =  PART_DIAGONAL | PART_UPPER
   ;  mcxTing* scline   =  mcxTingEmpty(NULL, 30)

   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_NO)
   ;  mclxIOsetQMode("MCLXIOFORMAT", MCL_APP_WB_NO)

   ;  if (argc == 1)
      goto help

   ;  while(a<argc)
      {  if (!strcmp(argv[a], "-h"))
         {  help
         :  mcxUsage(stdout, me, usagelines)
         ;  mcxExit(0)
      ;  }
         else if (!strcmp(argv[a], "--adapt"))
         {  adapt = 1
      ;  }
         else if (!strcmp(argv[a], "--version"))
         {  app_report_version(me)
         ;  exit(0)
      ;  }
         else if (!strcmp(argv[a], "--noindex"))
         {  index = 0
      ;  }
         else if (!strcmp(argv[a], "-parts"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  parts = 0
         ;  if (strchr(argv[a], 'l'))
            parts |= PART_LOWER
         ;  if (strchr(argv[a], 'd'))
            parts |= PART_DIAGONAL
         ;  if (strchr(argv[a], 'u'))
            parts |= PART_UPPER
      ;  }
         else if (!strcmp(argv[a], "-digits"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  digits = atoi(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-width"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  width = (int) atol(argv[a])
      ;  }
         else if (!strcmp(argv[a], "-mode"))
         {  if (a++ + 1 >= argc)
            goto arg_missing
         ;  if (!strcmp(argv[a], "sj"))
            mode |= DIST_SPLITJOIN
         ;  else if (!strcmp(argv[a], "vi"))
            mode |= DIST_VARINF
         ;  else if (!strcmp(argv[a], "ehd") || !strcmp(argv[a], "mk"))
            mode |= DIST_MIRKIN
         ;  else if (!strcmp(argv[a], "sc"))
            mode |= DIST_SCATTER
      ;  }
         else if (0)
         {  arg_missing:
            mcxErr
            (  me
            ,  "flag <%s> needs argument (see -h for usage)"
            ,  argv[argc-1]
            )
         ;  mcxExit(1)
      ;  }
         else
         {  break
      ;  }
         a++
   ;  }

      if (!mode)
      mode = DIST_SPLITJOIN

   ;  if (mode & DIST_MIRKIN || mode & DIST_SPLITJOIN)
      {  digits = 0
   ;  }

      for (i=a;i<argc;i++)
      {  mcxIO *xfcl             =  mcxIOnew(argv[i], "r")
      ;  mclMatrix*   cl         =  mclxRead(xfcl, EXIT_ON_FAIL)
      ;  cls[n_clusterings].cl   =  cl
      ;  cls[n_clusterings].name =  mcxTingNew(xfcl->fn->str)
      ;  fprintf
         (  stdout
         ,  "%4ld.%8ld  %s\n"
         ,  (long) n_clusterings+1
         ,  (long) N_COLS(cl)
         ,  xfcl->fn->str
         )
      ;  n_clusterings++
   ;  }

      if (a<argc)
      {  if (mode & DIST_SPLITJOIN)
         fprintf(stdout, "Using the split/join distance (metric)\n")
      ;  else if (mode & DIST_VARINF)
         fprintf(stdout, "Using variance of information measure (metric)\n")
      ;  else if (mode & DIST_MIRKIN)
         fprintf(stdout, "Using Mirkin metric (metric)\n")
      ;  if (mode & DIST_SCATTER)
         fprintf(stdout, "Using set count (metric)\n")
      ;  fprintf(stdout, "\n")
   ;  }

      for (i=0;i<n_clusterings;i++)
      {  mclMatrix* cl1    =  cls[i].cl
      ;  mclMatrix* cl1a
      ;  mcxTing*   name1  =  cls[i].name
      ;  int o, m, e, err
      ;  if ((err = mclcEnstrict(cl1, &o, &m, &e, ENSTRICT_TRULY)))
         {  report_partition("clmdist", cl1, name1, o, m, e)
         ;  if (adapt)
            report_fixit(me, n_err++)
         ;  else
            report_exit(me, SHCL_ERROR_PARTITION)
      ;  }
         for (j=i; j<n_clusterings;j++)      /* fixme; diagonal is special:zero */ 
         {  mclMatrix* cl2    =  cls[j].cl
         ;  mcxTing*  name2   =  cls[j].name
         ;  int  domain_error = 0
         ;  mclMatrix* cl2a, *meet12, *meet21
         ;  double dist1d, dist2d
         ;  int dist1i, dist2i
         ;  mclVector* meet

         ;  if (!mcldEquate(cl1->dom_rows, cl2->dom_rows, MCLD_EQ_EQUAL))
            {  domain_error=  1

            ;  if (adapt)
               {  meet     =  mcldMeet(cl1->dom_rows,cl2->dom_rows, NULL)
               ;  cl1a     =  mclcProject(cl1, meet)
               ;  cl2a     =  mclcProject(cl2, meet)

               ;  mcxErr
                  (  me
                  ,  "Domain inequality between files <%s><%s>"
                  ,  name2->str, name1->str
                  )

               ;  report_domain(me, N_ROWS(cl1), N_ROWS(cl2), meet->n_ivps)
               ;  mcxTell(me, "Using projections onto the meet of domains")

               ;  cl1 = cl1a
               ;  cl2 = cl2a
            ;  }
               else
               {  report_exit(me, SHCL_ERROR_DOMAIN)
            ;  }
            }

                                    /* fixme: support simultaneous output (?) */
                                    /* fixme: support xml-like output */

            meet12 =  mclcContingency(cl1, cl2)
         ;  meet21 =  mclxTranspose(meet12)

#define FILL(i,j,D_XXX, D_YYY, val1, val2)            \
      dists[i*n_clusterings+j].vals[D_XXX] = val1     \
   ;  dists[i*n_clusterings+j].vals[D_YYY] = val2     \
   ;  if (j>i)                                        \
      {  dists[j*n_clusterings+i].vals[D_XXX] = val2  \
      ;  dists[j*n_clusterings+i].vals[D_YYY] = val1  \
   ;  }

#define USE(i,j,D_XXX) (double) dists[i*n_clusterings+j].vals[D_XXX]

         ;  if (mode & DIST_SPLITJOIN)
            {  mclcSJDistance(cl1, cl2, meet12, meet21, &dist1i, &dist2i)
            ;  FILL(i,j,D_SJA,D_SJB, dist2i, dist1i)
         ;  }
            else if (mode & DIST_VARINF)
            {  mclcVIDistance(cl1, cl2, meet12, &dist1d, &dist2d)
            ;  FILL(i,j,D_VIA,D_VIB, dist1d, dist2d)
         ;  }
            else if (mode & DIST_MIRKIN)
            {  mclcJQDistance(cl1, cl2, meet12, &dist1d, &dist2d)
            ;  FILL(i,j,D_JQA, D_JQB,dist2d, dist1d)
         ;  }
            if (mode & DIST_SCATTER)
            {  int n_meet = mclxNrofEntries(meet12)
            ;  dist1i = n_meet-N_COLS(cl1)
            ;  dist2i = n_meet-N_COLS(cl2)
            ;  FILL(i,j,D_SCA, D_SCB,dist2i, dist1i)
         ;  }
            if (domain_error)
            {  mclxFree(&cl1a)  
            ;  mclxFree(&cl2a)  
            ;  mclvFree(&meet)  
            ;  cl1 = cls[i].cl
            ;  cl2 = cls[j].cl
         ;  }
            mclxFree(&meet12)
         ;  mclxFree(&meet21)
      ;  }
      }

      if (index && n_clusterings > 1)
      {  fprintf(stdout, "%6s", "")
      ;  for (i=0;i<n_clusterings;i++)
         fprintf(stdout, "%*d", width, (int) i+1)
      ;  fprintf(stdout, "\n")
   ;  }

      for (i=0;i<n_clusterings;i++)
      {  int  offset  =    DO_LOWER             ?  0 
                           :  DO_DIAGONAL       ?  i
                              :  DO_UPPER       ?  i+1
                                 :                 n_clusterings

      ;  int  bound  =     DO_UPPER             ?  n_clusterings
                           :  DO_DIAGONAL       ?  i+1
                              :  DO_LOWER       ?  i
                                 :                 0
      ;  mcxTingWrite(scline, "")

      ;  if (index)
         {  fprintf(stdout, "%4d. ", (int) i+1)
         ;  mcxTingPrintAfter(scline, "%6s", "")
      ;  }

         if (offset < bound)
         for (x=0;x<offset;x++)
         {  fprintf(stdout, "%*s", width, "")      /* fixme; smarter way ? */
         ;  mcxTingPrintAfter(scline, "%*s", width, "")
      ;  }

         for (j=offset;j<bound;j++)
         {  mcxTing* tmp = mcxTingEmpty(NULL, 200)
         ;  if (i == j && !DO_DIAGONAL)
            {  fprintf(stdout, "%*s", width, "")
            ;  mcxTingPrintAfter(scline, "%*s", width, "")
            ;  continue
         ;  }

            if (mode & DIST_VARINF)
            mcxTingPrint(tmp, "[%.*f,%.*f]", digits,USE(i,j,D_VIA),digits,USE(i,j,D_VIB))
         ;  else if (mode & DIST_SPLITJOIN)
            mcxTingPrint(tmp, "[%.*f,%.*f]", digits,USE(i,j,D_SJA),digits,USE(i,j,D_SJB))
         ;  else if (mode & DIST_MIRKIN)
            mcxTingPrint(tmp, "[%.*f,%.*f]", digits,USE(i,j,D_JQA),digits,USE(i,j,D_JQB))

         ;  if (mode & DIST_MAIN)
            fprintf(stdout, "%*s", width, tmp->str)

         ;  if (mode & DIST_SCATTER)
            {  mcxTingPrint(tmp, "[%.0f,%.0f]", USE(i,j, D_SCA), USE(i,j,D_SCB))
            ;  mcxTingPrintAfter(scline, "%*s", width, tmp->str)
         ;  }
            mcxTingFree(&tmp)
      ;  }

         if (offset < bound)
         for (x=bound;x<n_clusterings;x++)
         {  fprintf(stdout, "%*s", width, "")
         ;  mcxTingPrintAfter(scline, "%*s", width, "")
      ;  }
         fprintf(stdout, "\n")
      ;  if (mode & DIST_SCATTER)
         fprintf(stdout, "%s\n\n", scline->str)
   ;  }
      return 0
;  }


