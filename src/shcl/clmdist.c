/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* TODO
 *    rand index, randc (Hubert et ??)

   (n/2) + 2 Sum{i=1->c1} Sum{j=1->c2} (n_ij/2) - [ Sum{i=1->c1} (n_i./2) + Sum{j=1->c2} (n_.j/2) ]
                                 -------------------------
                                       n (n-1) / 2
     a + b
   ---------
     (n/2)

   a: same in C1 and same in C2
   b: different in C1 and different in C2.


            Sum{i=1->c1} Sum{j=1->c2} (n_ij/2) - [1/(n/2)] Sum{i=1->c1} (n_i/2) * Sum{j=1->c2} (n_j/2)
                                 -------------------------
 [1/2] *  ( Sum{i=1->c1} (n_i/2) + Sum{j=1->c2} (n_j/2) ) - [1/(n/2)] Sum{i=1->c1} (n_i/2) * Sum{j=1->c2} (n_j/2)

*/

#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "clm.h"
#include "report.h"
#include "clmdist.h"

#include "impala/matrix.h"
#include "impala/cat.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "taurus/ilist.h"
#include "taurus/la.h"

#include "clew/clm.h"

#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"


enum
{  DIST_SPLITJOIN 
,  DIST_VARINF
,  DIST_MIRKIN
,  DIST_SCATTER
}  ;


/*
 * clmdist will enstrict the clusterings, and possibly project them onto
 * the meet of the domains if necessary.
*/


typedef struct
{  mclx*       cl
;  mclx*       cltp
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


static const char* me = "clm dist";


enum
{  DIST_OPT_OUTPUT = CLM_DISP_UNUSED
,  DIST_OPT_MODE
,  DIST_OPT_DIGITS
,  DIST_OPT_FACTOR
}  ;


enum
{  VOL_OPT_OUTPUT = CLM_DISP_UNUSED
,  VOL_OPT_FACTOR
}  ;

static mcxOptAnchor volOptions[] =
{  {  "-o"
   ,  MCX_OPT_HASARG
   ,  VOL_OPT_OUTPUT
   ,  "<fname>"
   ,  "output file name"
   }
,  {  "-nff-fac"
   ,  MCX_OPT_HASARG
   ,  VOL_OPT_FACTOR
   ,  "<num>"
   ,  "stringency factor (require |meet| >= <num> * |cls-size|)"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


static mcxOptAnchor distOptions[] =
{  {  "-o"
   ,  MCX_OPT_HASARG
   ,  DIST_OPT_OUTPUT
   ,  "<fname>"
   ,  "output file name"
   }
,  {  "-mode"
   ,  MCX_OPT_HASARG
   ,  DIST_OPT_MODE
   ,  "<mode>"
   ,  "one of sj|vi|mk|sc"
   }
,  {  "-digits"
   ,  MCX_OPT_HASARG
   ,  DIST_OPT_DIGITS
   ,  "<num>"
   ,  "number of trailing digits for floats"
   }
,  {  "-nff-fac"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  DIST_OPT_FACTOR
   ,  "<num>"
   ,  "stringency factor (require |meet| >= <num> * |cls-size|)"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


static mcxIO*  xfout    =  (void*) -1;
static int digits       =  -1;
static int mode         =  -1;
static mcxbool i_am_vol =  FALSE;   /* node faithfulness */
static double  nff_fac  =  FLT_MAX;


static mcxstatus distInit
(  void
)
   {  xfout =  mcxIOnew("-", "w")
   ;  mode = 0
   ;  digits = 2
   ;  nff_fac     =  0.5
   ;  return STATUS_OK
;  }


static mcxstatus volInit
(  void
)
   {  xfout       =  mcxIOnew("-", "w")
   ;  i_am_vol    =  TRUE
   ;  nff_fac     =  0.5
   ;  return STATUS_OK
;  }


static mcxstatus volArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case VOL_OPT_FACTOR
      :  nff_fac = atof(val)
      ;  break
      ;

         case VOL_OPT_OUTPUT
      :  mcxIOnewName(xfout, val)
      ;  break
      ;

         default
      :  return STATUS_FAIL
      ;
      }
      return STATUS_OK
;  }


static mcxstatus distArgHandle
(  int optid
,  const char* val
)
   {  switch(optid)
      {  case DIST_OPT_OUTPUT
      :  mcxIOnewName(xfout, val)
      ;  break
      ;

         case DIST_OPT_DIGITS
      :  digits = atoi(val)
      ;  break
      ;

         case DIST_OPT_FACTOR
      :  nff_fac = atof(val)
      ;  break
      ;

         case DIST_OPT_MODE
      :     if (!strcmp(val, "sj"))
            mode = DIST_SPLITJOIN
         ;  else if (!strcmp(val, "vi"))
            mode = DIST_VARINF
         ;  else if (!strcmp(val, "ehd") || !strcmp(val, "mk"))
            mode = DIST_MIRKIN
         ;  else if (!strcmp(val, "sc"))
            mode = DIST_SCATTER
         ;  else
            mcxDie(1, me, "unknown mode <%s>", val)
      ;  break
      ;

         default
      :  return STATUS_FAIL
      ;
      }
      return STATUS_OK
;  }


static mcxstatus distMain
(  int                  argc
,  const char*          argv[]
)
   {  int               i, j
   ;  int n_clusterings =  0
   ;  clnode*  cls      =  mcxAlloc(argc * sizeof(clnode), EXIT_ON_FAIL)
   ;  int a             =  0
   ;  int mode          =  0
   ;  int digits        =  2
   ;  mclx* nff_scores  =  NULL
   ;  dim nff[5]        =  { 0, 0, 0, 0, 0 }

   ;  if (i_am_vol)
      me = "clm vol"
      
   ;  if (!mode)
      mode = DIST_SPLITJOIN

   ;  if (mode & DIST_MIRKIN || mode & DIST_SPLITJOIN)
      digits = 0

   ;  mcxIOopen(xfout, EXIT_ON_FAIL)

   ;  for (i=a;i<argc;i++)
      {  mcxIO *xfcl    =  mcxIOnew(argv[i], "r")
      ;  mclx*   cl     =  mclxRead(xfcl, EXIT_ON_FAIL)
      ;  dim o, m, e, err
      ;  if ((err = clmEnstrict(cl, &o, &m, &e, ENSTRICT_TRULY)))
            report_partition("clm dist/vol", cl, xfcl->fn, o, m, e)
         ,  exit(1)
      ;  if 
         (  n_clusterings
         && !  mcldEquate
               (cl->dom_rows, cls[n_clusterings-1].cl->dom_rows, MCLD_EQT_EQUAL)
         )
         mcxDie
         (  1
         ,  me
         ,  "domains differ between %s and %s"
         ,  xfcl->fn->str, cls[n_clusterings-1].name->str
         )
      ;  cls[n_clusterings].cl   =  cl
      ;  cls[n_clusterings].name =  mcxTingNew(xfcl->fn->str)
      ;  cls[n_clusterings].cltp =  i_am_vol ? mclxTranspose(cl) : NULL
      ;  n_clusterings++
   ;  }

      if (i_am_vol && n_clusterings)
      nff_scores
      =  mclxCartesian
         (  mclvCanonical(NULL, 1, 1.0)
         ,  mclvClone(cls[0].cl->dom_rows)
         ,  1.0
         )

   ;  for (i=0;i<n_clusterings;i++)
      {  mclx* c1    =  cls[i].cl
      ;  for (j=i+1; j<n_clusterings;j++)      /* fixme; diagonal is special:zero */ 
         {  mclx* c2  =  cls[j].cl
         ;  mclx* meet12, *meet21
         ;  double dist1d, dist2d
         ;  dim dist1i, dist2i
         ;  dim n_volatile = 0

         ;  meet12 =  clmContingency(c1, c2)
         ;  meet21 =  mclxTranspose(meet12)

         ;  {  dim k
            ;  for (k=0;k<N_COLS(meet12);k++)
               {  dim l
               ;  mclv* ct = meet12->cols+k
               ;  mclv* c1mem = c1->cols+k
               ;  mclv* c2mem = NULL
               ;  for (l=0;l<ct->n_ivps;l++)
                  {  ofs c2id = ct->ivps[l].idx
                  ;  double meet_sz = ct->ivps[l].val
                  ;  c2mem = mclxGetVector(c2, c2id, EXIT_ON_FAIL, c2mem)
                  ;  if
                     (  meet_sz < nff_fac * c1mem->n_ivps
                     && meet_sz < nff_fac * c2mem->n_ivps
                     )
                     {  mclv* meet = mcldMeet(c1mem, c2mem, NULL)
                     ;  mclp* tivp = NULL
                     ;  dim m
                     ;  if (i_am_vol)
                        for (m=0;m<meet->n_ivps;m++)
                        {  tivp = mclvGetIvp(nff_scores->cols+0, meet->ivps[m].idx, tivp)
                        ;  tivp->val += 1 / (double) meet->n_ivps
                     ;  }
                        n_volatile += meet->n_ivps
                     ;  mclvFree(&meet)
                  ;  }
                     if
                     (  meet_sz < 0.5 * c1mem->n_ivps
                     && meet_sz < 0.5 * c2mem->n_ivps
                     )
                     nff[0] += meet_sz
                  ;  if
                     (  meet_sz < 0.75 * c1mem->n_ivps
                     && meet_sz < 0.75 * c2mem->n_ivps
                     )
                     nff[1] += meet_sz
                  ;  if
                     (  meet_sz < 0.90 * c1mem->n_ivps
                     && meet_sz < 0.90 * c2mem->n_ivps
                     )
                     nff[2] += meet_sz
                  ;  if
                     (  meet_sz < 0.95 * c1mem->n_ivps
                     && meet_sz < 0.95 * c2mem->n_ivps
                     )
                     nff[3] += meet_sz
                  ;  if
                     (  meet_sz < 0.99999 * c1mem->n_ivps
                     && meet_sz < 0.99999 * c2mem->n_ivps
                     )
                     nff[4] += meet_sz
               ;  }
               }
               if (i_am_vol)  /* quickly hack this in; refactor later */
               continue
         ;  }


            if (mode == DIST_SPLITJOIN)
               clmSJDistance(c1, c2, meet12, meet21, &dist1i, &dist2i)
            ,  fprintf
               (  xfout->fp
               ,  "%lu\t%lu\t%lu\t%ld\t%ld\t%s\t%s\t[%ld,%ld,%ld,%ld,%ld]\n"
               ,  (ulong) (dist1i + dist2i)
               ,  (ulong) dist1i
               ,  (ulong) dist2i
               ,  (long) N_ROWS(c1)
               ,  (long) n_volatile
               ,  cls[i].name->str
               ,  cls[j].name->str
               ,  (long) nff[0]
               ,  (long) nff[1]
               ,  (long) nff[2]
               ,  (long) nff[3]
               ,  (long) nff[4]
               )

         ;  else if (mode == DIST_VARINF)
               clmVIDistance(c1, c2, meet12, &dist1d, &dist2d)
            ,  fprintf
               (  xfout->fp
               ,  "%.3f\t%.3f\t%.3f\t%ld\t%s\t%s\n"
               ,  dist1d + dist2d
               ,  dist1d
               ,  dist2d
               ,  (long) N_ROWS(c1)
               ,  cls[i].name->str
               ,  cls[j].name->str
               )

         ;  else if (mode == DIST_MIRKIN)
               clmJQDistance(c1, c2, meet12, &dist1d, &dist2d)
            ,  fprintf
               (  xfout->fp
               ,  "%.3f\t%.3f\t%.3f\t%ld\t%s\t%s\n"
               ,  dist1d + dist2d
               ,  dist1d
               ,  dist2d
               ,  (long) N_ROWS(c1)
               ,  cls[i].name->str
               ,  cls[j].name->str
               )

         ;  else if (mode == DIST_SCATTER)
            {  int n_meet = mclxNrofEntries(meet12)
            ;  dist1i = n_meet-N_COLS(c1)
            ;  dist2i = n_meet-N_COLS(c2)
            ;  fprintf
               (  xfout->fp
               ,  "%lu\t%lu\t%lu\t%ld\t%s\t%s\n"
               ,  (ulong) (dist1i + dist2i)
               ,  (ulong) dist1i
               ,  (ulong) dist2i
               ,  (long) N_ROWS(c1)
               ,  cls[i].name->str
               ,  cls[j].name->str
               )
         ;  }

            mclxFree(&meet12)
         ;  mclxFree(&meet21)
      ;  }
      }

      if (i_am_vol)
      {  mclxaWrite(nff_scores, xfout, 4, RETURN_ON_FAIL)
   ;  }
      return STATUS_OK
;  }


static mcxDispHook volEntry
=  {  "vol"
   ,  "vol [options] <cl file>+"
   ,  volOptions
   ,  sizeof(volOptions)/sizeof(mcxOptAnchor) - 1
   ,  volArgHandle
   ,  volInit
   ,  distMain
   ,  1
   ,  -1
   ,  MCX_DISP_DEFAULT
   }
;


static mcxDispHook distEntry
=  {  "dist"
   ,  "dist [options] <cl file>+"
   ,  distOptions
   ,  sizeof(distOptions)/sizeof(mcxOptAnchor) - 1
   ,  distArgHandle
   ,  distInit
   ,  distMain
   ,  0
   ,  -1
   ,  MCX_DISP_DEFAULT
   }
;


mcxDispHook* mcxDispHookDist
(  void
)
   {  return &distEntry
;  }


mcxDispHook* mcxDispHookVol
(  void
)
   {  return &volEntry
;  }


