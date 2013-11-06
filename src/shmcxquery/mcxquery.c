/* (c) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/ivp.h"
#include "impala/pval.h"
#include "impala/io.h"
#include "impala/app.h"
#include "impala/tab.h"
#include "mcl/interpret.h"

#include "util/types.h"
#include "util/err.h"
#include "util/ting.h"
#include "util/ding.h"
#include "util/tok.h"
#include "util/opt.h"
#include "util/array.h"
#include "util/rand.h"

#include "taurus/parse.h"
#include "taurus/la.h"


enum
{  MY_OPT_IMX
,  MY_OPT_DOMAIN
,  MY_OPT_TAB
,  MY_OPT_VERSION
,  MY_OPT_HELP
,  MY_OPT_APROPOS
}  ;

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
,  {  "-dom"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DOMAIN
   ,  "<fname>"
   ,  "domain matrix (target for 'd' specs)"
   }
,  {  "-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAB
   ,  "<fname>"
   ,  "tab file name"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "matrix/graph file name"
   }
,  {  NULL, 0, 0, NULL, NULL }  
}  ;



const char* syntax = "Usage: mcxquery <options> <sub-spec>+";
const char* me = "mcxquery";


typedef struct
{  mcxTing*    fname
;  mcxTing*    txt              /*  text representation of spec   */
;  mclv*       cvec             /*  contains col indices    */
;  mclv*       rvec             /*  contains row indices    */
;  long        sz_min
;  long        sz_max
;  long        ext_disc         /* TODO remove ext_disc */
;  long        ext_cdisc
;  long        ext_rdisc
;  mclv*       path_set
;  long        ext_ring
;  int         n_col_specs
;  int         n_row_specs
;  mcxbool     do_extend
;  mclpAR*     sel_val
;  mcxbits     sel_sz_opts 
;  mcxbits     fin_map_opts
;  mcxbits     fin_misc_opts
;  
}  subspec_mt   ;


typedef struct
{  mclx*       mx
;  mclx*       dom
;  mclx*       el2dom
;  mclv*       universe_cols
;  mclv*       universe_rows
;  mclv*       randselect_cols
;  mclv*       randselect_rows
;  mclTab*     tab
;  long        min
;  long        max
;  int         rand_mode
;  int         spec_ct
;  
}  context_mt   ;


mclVector*  VectorFromString
(  const char* str
,  mclMatrix*  dom
,  mclVector*  universe
,  int*        n_parsed
)  ;


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxIO *xfcl = NULL, *xfmx = NULL, *xftab = NULL

   ;  mclx *dom = NULL,*el2dom = NULL, *mx = NULL
   ;  mclv *universe_rows = NULL, *universe_cols = NULL
   ;  mclv *randselect_rows = NULL, *randselect_cols = NULL
   ;  context_mt  ctxt
   ;  mcxTing* fnout    =  mcxTingNew("out.mcxquery")

   ;  mclTab* tab       =  NULL

   ;  subspec_mt *specs =  NULL
   ;  int    n_spec     =  0
   ;  mcxBuf spec_buf

   ;  int digits        =  MCLXIO_VALUE_GETENV
   ;  int a             =  1
   ;  int i             =  0
   ;  int n_arg_read    =  0

   ;  mcxstatus parseStatus = STATUS_OK
   ;  mcxOption* opts, *opt


   ;  mcxBufInit(&spec_buf,  &specs, sizeof(subspec_mt), 30)

   ;  if (argc==1)
      {  mcxOptApropos(stdout, me, syntax, 20, MCX_OPT_DISPLAY_SKIP, options)
      ;  exit(0)
   ;  }

      mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)

   ;  if
      (!(opts = mcxOptExhaust(options, (char**) argv, argc, 1, &n_arg_read, &parseStatus)))
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

            case MY_OPT_TAB
         :  xftab = mcxIOnew(opt->val, "r")
         ;  mcxIOopen(xftab, EXIT_ON_FAIL)
         ;  break
         ;

            case MY_OPT_IMX
         :  xfmx = mcxIOnew(opt->val, "r")
         ;  mcxIOopen(xfmx, EXIT_ON_FAIL)
         ;  break
         ;

            case MY_OPT_DOMAIN
         :  xfcl = mcxIOnew(opt->val, "r")
         ;  mcxIOopen(xfcl, EXIT_ON_FAIL)
         ;  break
         ;
      ;  }
      }

   ;  if (!xfmx)
      mcxDie(1, me, "need input matrix")

   ;  mx    =  mclxRead(xfmx, EXIT_ON_FAIL)
   ;  universe_rows =  mclvClone(mx->dom_rows)
   ;  universe_cols =  mclvClone(mx->dom_cols)

   ;  mcxIOclose(xfmx)

   ;  if (xftab)
      {  tab = mclTabRead(xftab, NULL, EXIT_ON_FAIL)
      ;  mcxIOfree(&xftab)
   ;  }

      if (xfcl)
      {  dom = mclxRead(xfcl, EXIT_ON_FAIL)
      ;  mcxIOfree(&xfcl)
   ;  }

      if (!dom)
         mcxTell(me, "using -imx matrix for domain retrieval")
      ,  dom = mx

   ;  ctxt.tab             =  tab
   ;  ctxt.dom             =  dom
   ;  ctxt.mx              =  mx
   ;  ctxt.el2dom          =  el2dom
   ;  ctxt.universe_cols   =  universe_cols
   ;  ctxt.universe_rows   =  universe_rows
   ;  ctxt.randselect_cols   =  randselect_cols
   ;  ctxt.randselect_rows   =  randselect_rows
   ;  ctxt.rand_mode       =  0
   ;  ctxt.spec_ct         =  i

   ;  mclxFree(&el2dom)
   ;  return 0
;  }


mcxstatus add_vec
(  const char*       dtype
,  int               itype
,  const mclVector*  invec
,  subspec_mt*       spec
,  context_mt*       ctxt
)
   {  const char* modec =  strpbrk(dtype, "cC")
   ;  const char* moder =  strpbrk(dtype, "rR")

   ;  mclv* cvec        =  mclvInit(NULL)
   ;  mclv* rvec        =  mclvInit(NULL)

   ;  const mclv* randselect_cols  =  ctxt->randselect_cols
   ;  const mclv* randselect_rows  =  ctxt->randselect_rows

   ;  const mclv* universe_cols  =  ctxt->universe_cols
   ;  const mclv* universe_rows  =  ctxt->universe_rows

   ;  int rand_mode     =  ctxt->rand_mode

   ;  mclv* invec2      =  NULL

   ;  if (itype == 'i' || itype == 'I')   /* fixme danger const cast */
      invec2 = (mclv*) invec              /* modify in 'd/D' case */

   ;  else if (itype == 'd' || itype == 'D')
      invec2 = mclxUnionv(ctxt->dom, invec, NULL)

   ;  else if (itype == 't' || itype == 'T')
      {  if (!ctxt->tab)
         {  mcxErr(me, "no tab file specified!")
         ;  return STATUS_FAIL
      ;  }
         invec2 = ctxt->tab->domain
   ;  }

      if (modec)
      {  mclv* invec_c = mclvClone(invec2)

      ;  if (itype == 'I' || itype == 'T')
         mcldMinus(universe_cols, invec_c, invec_c)
      ;  else if (itype == 'D')
         mcldMinus(ctxt->dom->dom_rows, invec_c, invec_c)

      ;  if ((unsigned char) modec[0] == 'C')
         mcldMinus(universe_cols, invec_c, invec_c)

      ;  mcldMerge(cvec, invec_c, cvec)
      ;  mclvFree(&invec_c)

      ;  if (randselect_cols)
         {  if (rand_mode == 'd')      /* discard */
            mcldMinus(cvec, randselect_cols, cvec)
         ;  else if (rand_mode == 'e')
            mcldMinus(randselect_cols, cvec, cvec)
         ;  else if (rand_mode == 'i')
            mcldMeet(randselect_cols, cvec, cvec)
         ;  else
            mcldMerge(cvec, randselect_cols, cvec)
      ;  }
         mcldMerge(cvec, spec->cvec, spec->cvec)
      ;  spec->n_col_specs++
   ;  }

      if (moder)
      {  mclv* invec_r = mclvClone(invec2)

      ;  if (itype == 'I' || itype == 'T')
         mcldMinus(universe_rows, invec_r, invec_r)
      ;  else if (itype == 'D')
         mcldMinus(ctxt->dom->dom_rows, invec_r, invec_r)

      ;  if ((unsigned char) moder[0] == 'R')
         mcldMinus(universe_rows, invec_r, invec_r)

      ;  mcldMerge(rvec, invec_r, rvec)
      ;  mclvFree(&invec_r)

      ;  if (randselect_rows)
         {  if (rand_mode == 'd')
            mcldMinus(rvec, randselect_rows, rvec)
         ;  else if (rand_mode == 'e')
            mcldMinus(randselect_rows, rvec, rvec)
         ;  else if (rand_mode == 'i')
            mcldMeet(randselect_rows, rvec, rvec)
         ;  else
            mcldMerge(rvec, randselect_rows, rvec)
      ;  }
         mcldMerge(rvec, spec->rvec, spec->rvec)
      ;  spec->n_row_specs++
   ;  }

      if (invec2 != invec)
      mclvFree(&invec2)
   ;  return STATUS_OK
;  }


mcxstatus parse_dom
(  mcxLink*    src
,  int         n_args
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src->next
   ;  const char* dtype =  ((mcxTing*)lk->val)->str
   ;  int dlen          =  strlen(dtype)
   ;  mcxstatus status  =  STATUS_FAIL

   ;  if
      (  (strspn(dtype, "cCrR") != dlen)
      || (  dlen == 2
         && (  strspn(dtype, "cC") == 2
            || strspn(dtype, "rR") == 2
            )
         )
      )
      {  mcxErr(me, "wonky domain indication <%s>", dtype)
      ;  return STATUS_FAIL
   ;  }

     /*  The bit that follows is ugly. It checks a special case
      *  that cannot be handled in add_vec because we never get
      *  to add_vec in case there is no 'iIdD' specification.
     */
      if (!lk->next)
      {  mclv* empty = mclvInit(NULL)
      ;  mcxstatus status = STATUS_FAIL

      ;  while (1)
         {  if (  strchr(dtype, 'C')
               && add_vec("c", 'i', empty, spec, ctxt)
               )
               break
         ;  if (  strchr(dtype, 'R')
               && add_vec("r", 'i', empty, spec, ctxt)
               )
               break
         ;  if (  strchr(dtype, 'c')
               && add_vec("c", 'i', ctxt->universe_cols, spec, ctxt)
               )
               break
         ;  if (  strchr(dtype, 'r')
               && add_vec("r", 'i', ctxt->universe_rows, spec, ctxt)
               )
               break
         ;  status = STATUS_OK
         ;  break
      ;  }
         if (!status)
         spec->n_row_specs++
      ;  return status
   ;  }

      while ((lk = lk->next))
      {  mcxTokFunc tf
      ;  mcxTing* id = lk->val
      ;  char* z

      ;  tf.opts = MCX_TOK_DEL_WS

      ;  if
         (  STATUS_OK
         == mcxTokExpectFunc(&tf, id->str, id->len, &z, -1, -1, NULL)
         )
         {  mclVector* vec
         ;  const char* str      =  tf.key->str
         ;  unsigned char itype  =  str[0]
         ;  long twilch = 0

         ;  if (!strchr("iIdDtT", (int) itype))
            {  mcxErr(me, "unknown index type <%c>", (int) itype)
            ;  break
         ;  }

            if (tf.key->len > 1)
            {  mcxErr(me, "spurious characters in itype <%s>", str)
            ;  break
         ;  }

            if (itype == 'd' || itype == 'D')
            {                       /* fixme/hackish control flow        */
               if (!ctxt->dom)      /* may happen with reread + dom = mx */
               {  mcxErr(me, "did you use reread without -dom? PANIC!")
               ;  break
            ;  }
               twilch = -2
         ;  }

            if             /* fixme really redundant for tab spec */
            ( !(  vec 
               =  ilSpecToVec(tf.args->next, &twilch, NULL, RETURN_ON_FAIL)
               )
            )
            {  mcxErr(me, "error converting")
            ;  mclvFree(&vec)
            ;  break
         ;  }
            mcxTokFuncFree(&tf)

         ;  if (add_vec(dtype, itype, vec, spec, ctxt))
            {  mclvFree(&vec)
            ;  break
         ;  }
            mclvFree(&vec)
      ;  }
         else
         break
   ;  }

      while (1)         /* this used to be longer */
      {  if (lk)
         break

      ;  status = STATUS_OK
      ;  break
   ;  }

      return status
;  }


mcxstatus parse_path
(  mcxLink*    src
,  int         n_args
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src
   ;  mclv* set = mclvInit(NULL)

   ;  if (!ctxt->mx)
      {  mcxErr(me, "cannot compute paths - no matrix! (did you use --from-disk?)")
      ;  return STATUS_FAIL
   ;  }
   
      while ((lk = lk->next))
      {  mcxTokFunc tf
      ;  mcxTing* ti = lk->val
      ;  int i = atoi(ti->str)
      ;  mclvInsertIdx(set, i, 1.0)
   ;  }

      if (set->n_ivps)
      spec->path_set = set
   ;  else
      mclvFree(&set)

   ;  return lk ? STATUS_FAIL : STATUS_OK
;  }



mcxstatus parse_ext
(  mcxLink*    src
,  int         n_args
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src

   ;  if (!ctxt->mx)
      {  mcxErr(me, "cannot extend - no matrix! (did you use --from-disk?)")
      ;  return STATUS_FAIL
   ;  }
   
      while ((lk = lk->next))
      {  mcxTokFunc tf
      ;  mcxTing* extspec = lk->val
      ;  char* z

      ;  tf.opts = MCX_TOK_DEL_WS

      ;  if
         (  STATUS_OK
         == mcxTokExpectFunc(&tf, extspec->str, extspec->len, &z, 1, 1, NULL)
         )
         {  const char* val = ((mcxTing*) tf.args->next->val)->str
         ;  const char* key = tf.key->str
         ;  char* onw = NULL        /* onwards */
         ;  long l = strtol(val, &onw, 10)

         ;  if (val == onw)
            {  mcxErr(me, "failed to parse number <%s>", val)
            ;  break
         ;  }

            if (!strcmp(key, "disc"))
            spec->ext_disc = l
         ;  else if (!strcmp(key, "ring"))
            spec->ext_ring = l
         ;  else if (!strcmp(key, "cdisc"))
            spec->ext_cdisc = l
         ;  else if (!strcmp(key, "rdisc"))
            spec->ext_rdisc = l
         ;  else
            {  mcxErr(me, "unexpected <%s>", key)
            ;  break
         ;  }
            mcxTell(me, "extending %s %ld", key, (long) l)
      ;  }
         else
         break
   ;  }
      return lk ? STATUS_FAIL : STATUS_OK
;  }


mcxstatus parse_size
(  mcxLink*    src
,  int         n_args
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src
   
   ;  while ((lk = lk->next))
      {  mcxTokFunc tf
      ;  mcxTing* valspec = lk->val
      ;  char* z

      ;  tf.opts = MCX_TOK_DEL_WS

      ;  if
         (  STATUS_OK
         == mcxTokExpectFunc(&tf, valspec->str, valspec->len, &z, 1, 1, NULL)
         )
         {  const char* val = ((mcxTing*) tf.args->next->val)->str
         ;  const char* key = tf.key->str
         ;  mcxbits bits = 0
         ;  char* onw = NULL        /* onwards */
         ;  long l = strtol(val, &onw, 10)

         ;  if (val == onw)
            {  mcxErr(me, "failed to parse number <%s>", val)
            ;  break
         ;  }

            if (!strcmp(key, "gq"))
            bits = MCLX_EQT_GQ
         ;  else if (!strcmp(key, "gt"))
            bits = MCLX_EQT_GT
         ;  else if (!strcmp(key, "lt"))
            bits = MCLX_EQT_LT
         ;  else if (!strcmp(key, "lq"))
            bits = MCLX_EQT_LQ

         ;  if (bits & (MCLX_EQT_GQ | MCLX_EQT_GT))
            spec->sz_min = l
         ;  else
            spec->sz_max = l

         ;  spec->sel_sz_opts  |= bits
         ;  mcxTell(me, "selecting num entries <%s> <%ld>", key, l)
      ;  }
         else
         break
   ;  }
      return lk ? STATUS_FAIL : STATUS_OK
;  }



mcxstatus dispatch
(  mcxLink* src
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src

   ;  while ((lk = lk->next))
      {  mcxTing* txt = lk->val
      ;  char* z      = NULL
      ;  int n_args   = 0
      ;  mcxstatus status = STATUS_FAIL
      ;  mcxTokFunc  tf

      ;  tf.opts = MCX_TOK_DEL_WS

      ;  while (1)
         {  if (mcxTokExpectFunc(&tf, txt->str, txt->len,  &z, -1, -1, &n_args))
            {  mcxErr(me, "cannot parse <%s>", txt->str)
            ;  break
         ;  }

            if (mcxStrChrAint(z, isspace, -1))
            {  mcxErr(me, "spurious spunk <%s>", z)
            ;  break
         ;  }

            if (!strcmp(tf.key->str, "dom"))
            {  if (parse_dom(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "path"))
            {  if (parse_path(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "ext"))
            {  if (parse_ext(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "size"))
            {  if (parse_size(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "val"))
            {  if (!(spec->sel_val = mclpTFparse(tf.args, NULL)))
               break
         ;  }

            else
            {  mcxErr(me, "unknown type <%s>", tf.key->str)
            ;  break
         ;  }
            status = STATUS_OK
         ;  break
      ;  }

         mcxTokFuncFree(&tf)
      ;  if (status)
         break
   ;  }
      return lk ? STATUS_FAIL : STATUS_OK
;  }


mcxstatus extend_path
(  mclv*       dom
,  const mclv* path_set
,  context_mt* ctxt
)
   /* NOTE dom is alias for spec->cvec or spec->rvec */

   {  mclv* punters = mclxSSPd(ctxt->mx, path_set)
   ;  if (punters)
      {  mclvCopy(dom, punters)
      ;  mclvFree(&punters)
      ;  return STATUS_OK
   ;  }
      return STATUS_FAIL
;  }



mcxstatus extend_disc
(  mclv*       dom
,  int         ext_disc
,  context_mt* ctxt
)
   /* NOTE dom is alias for spec->cvec or spec->rvec */

   {  mclx* mx = ctxt->mx
   ;  mclv* wave = mclxUnionv(mx, dom, NULL)
   ;  mclv* new  = mcldMinus(wave, dom, NULL)

   ;  while (ext_disc-- && new->n_ivps)
      {  mcldMerge(dom, new, dom)
      ;  wave = mclxUnionv(mx, dom, wave)
      ;  new  = mcldMinus(wave, dom, new)
   ;  }
      mclvFree(&wave)
   ;  mclvFree(&new)
   ;  return STATUS_OK
;  }


mcxstatus spec_parse
(  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxTing* txt      =  spec->txt
   ;  mcxstatus status  =  STATUS_FAIL
   ;  int n_args        =  0
   ;  mcxLink* args     =  mcxTokArgs
                           (txt->str, txt->len, &n_args, MCX_TOK_DEL_WS)

   ;  if (args)
         mcxTell
         (me, "dispatching <%d> argument%s", n_args, n_args > 1 ? "s" : "")
      ,  status = dispatch(args, spec, ctxt)

   ;  return status
;  }


