/*  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* FOCUS:
 *  - Don't implement chains that can be achieved more elegantly
 *    by reusing the program.
 *  - Do implement functionality that would otherwise be very time-consuming
 *    or silly not to implement given that the matrix is held in memory.
 *  - Don't create a multitude of directives; try to achieve power by
 *    small building blocks and powerful rules for joining them.
 *       Example: extend domain construction abilities by combining
 *       different dom specs rather than expanding iIdDpPjJ logic.
*/

/* TODO
 *    Optimize 1-submatrix addition, and perhaps 2- and 4- as well.
 *
 *    Move tag and tag-digits opts into specs.
 *
 *    is mclxSub optimized for simple domain manipulations?
 *
 *    domain matrix: start by reading domains only,
 *    read matrix only if necessary. (useful with d-1 d-2 specs).
 *
 *    Move dfac logic into loop, rather then precomputing.
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
#include "mcl/interpret.h"

#include "util/types.h"
#include "util/err.h"
#include "util/ting.h"
#include "util/ding.h"
#include "util/tok.h"
#include "util/opt.h"
#include "util/array.h"
#include "util/duck.h"

#include "taurus/parse.h"
#include "taurus/la.h"


enum
{  MY_OPT_IMX
,  MY_OPT_DOMAIN
,  MY_OPT_BLOCK
,  MY_OPT_BLOCK2
,  MY_OPT_BLOCKC
,  MY_OPT_FROM_DISK
,  MY_OPT_SKIN
,  MY_OPT_CROSS
,  MY_OPT_OUT
,  MY_OPT_EFAC
,  MY_OPT_DFAC
,  MY_OPT_RFAC
,  MY_OPT_CFAC
,  MY_OPT_RAND_DISCARD
,  MY_OPT_RAND_EXCLUSIVE
,  MY_OPT_RAND_INTERSECT
,  MY_OPT_RAND_MERGE
,  MY_OPT_TAGGED
,  MY_OPT_TAG_DIGITS
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
,  {  "--block2"
   ,  MCX_OPT_HIDDEN
   ,  MY_OPT_BLOCK2
   ,  NULL
   ,  "use alternative block routine"
   }
,  {  "--block"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_BLOCK
   ,  NULL
   ,  "use the block matrix induced by dom"
   }
,  {  "-out"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUT
   ,  "<fname>"
   ,  "special purpose output file name"
   }
,  {  "--blockc"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_BLOCKC
   ,  NULL
   ,  "use the complement of block matrix"
   }
,  {  "--tag"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TAGGED
   ,  NULL
   ,  "output tagged matrices"
   }
,  {  "--skin-read"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SKIN
   ,  NULL
   ,  "only read skeleton matrix (domains, no entries)"
   }
,  {  "--extend"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_CROSS
   ,  NULL
   ,  "read extended submatrices"
   }
,  {  "--rand-discard"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_RAND_DISCARD
   ,  NULL
   ,  "discard random selection"
   }
,  {  "--rand-exclusive"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_RAND_EXCLUSIVE
   ,  NULL
   ,  "discard regular selection"
   }
,  {  "--rand-intersect"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_RAND_INTERSECT
   ,  NULL
   ,  "intersect random and regular selection"
   }
,  {  "--rand-merge"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_RAND_MERGE
   ,  NULL
   ,  "join random and regular selection"
   }
,  {  "-efac"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_EFAC
   ,  "<num>"
   ,  "random selection edge factor"
   }
,  {  "-dfac"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DFAC
   ,  "<num>"
   ,  "random selection domain factor"
   }
,  {  "-rfac"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_RFAC
   ,  "<num>"
   ,  "random selection row factor"
   }
,  {  "-cfac"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_CFAC
   ,  "<num>"
   ,  "random selection column factor"
   }
,  {  "-tag-digits"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TAG_DIGITS
   ,  "<int>"
   ,  "digits to print for tagged write"
   }
,  {  "-imx"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMX
   ,  "<fname>"
   ,  "matrix/graph file name"
   }
,  {  NULL, 0, 0, NULL, NULL }  
}  ;



#define FIN_MISC_CHR     1
#define FIN_MISC_TP      2
#define FIN_MISC_DOMC    4
#define FIN_MISC_DOMR    8
#define FIN_MISC_EMPTY  16

#define MCLX_WEED_ROWS 1
#define MCLX_WEED_COLS 2

mclx* mclxWeed
(  mclx* mx
,  mclv** colnumspp
,  mclv** rownumspp
,  mcxbits bits
)  ;


#define FIN_MAP_CAN_COLS 1
#define FIN_MAP_CAN_ROWS 2
#define FIN_MAP_UNI_COLS 4
#define FIN_MAP_UNI_ROWS 8
#define FIN_MAP_WEED_COLS 16
#define FIN_MAP_WEED_ROWS 32
#define FIN_WB             64

typedef struct
{  mcxTing*    fname
;  mcxTing*    txt              /*  text representation of spec   */
;  mclVector*  cvec             /*  contains col indices    */
;  mclVector*  rvec             /*  contains row indices    */
;  double      val_min
;  double      val_max
;  long        sz_min
;  long        sz_max
;  int         n_col_specs
;  int         n_row_specs
;  mcxbool     do_extend
;  mcxbits     sel_val_opts
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
;  mclv*       subspace_cols
;  mclv*       subspace_rows
;  long        min
;  long        max
;  int         rand_mode
;  int         spec_ct
;  
}  context_mt   ;


const char* syntax = "Usage: mcxsubs <options> <sub-spec>+";
const char* me = "mcxsubs";

/*  Conceivable options for randomly thinning out a graph.
 * -  thing out domains, either the same way or separately, or only one of them.
 * -  randomly pick edges.
*/

double efac = 0.0;         /* edges */
double dfac = 0.0;         /* both domains */
double cfac = 0.0;         /* column domain */
double rfac = 0.0;         /* row domain */


mclVector*  VectorFromString
(  const char* str
,  mclMatrix*  dom
,  mclVector*  universe
,  int*        n_parsed
)  ;


void thin_out
(  mclv* universe
,  double fac
)
   {  int i
   ;  double zero = 0.0
   ;  for (i=0;i<universe->n_ivps;i++)
      {  long r = random()
      ;  if (((double) r) / RANDOM_MAX > fac)
         universe->ivps[i].val = 0.0
   ;  }

      mclvUnary(universe, fltGtBar, &zero)
;  }


void prune_edges
(  mclx* mx
,  double efac
)
   {  long c, i
   ;  for (c=0;c<N_COLS(mx);c++)
      {  mclv* vec   = mx->cols+c
      ;  for (i=0;i<vec->n_ivps;i++)
         if (((double) random()) / RANDOM_MAX >= efac)
         vec->ivps[i].val = 0.0

      ;  mclvUnary(vec, fltCopy, NULL)    /* removes zeros */
   ;  }
   }


void spec_exec
(  subspec_mt* spec
,  context_mt* ctxt
,  mcxIO*   xfmx
,  double   efac
,  mcxbool  bCltag
,  int      digits
)  ;


mcxstatus spec_parse
(  subspec_mt*  spec
,  context_mt*  ctxt
)  ;


void spec_init
(  subspec_mt*  spec
,  const char*  str
,  int          ct
,  mcxbool      do_extend
)
   {  spec->txt            =  mcxTingNew(str)
   ;  spec->fname          =  mcxTingPrint
                              (NULL, "out-sub-%d", (int) ct)

   ;  spec->val_min        =  0.0
   ;  spec->val_max        =  0.0

   ;  spec->sz_min         =  0
   ;  spec->sz_max         =  0

   ;  spec->sel_val_opts   =  0
   ;  spec->sel_sz_opts    =  0
   ;  spec->fin_map_opts   =  0
   ;  spec->fin_misc_opts  =  0

   ;  spec->n_row_specs    =  0
   ;  spec->n_col_specs    =  0

   ;  spec->do_extend      =  do_extend
   ;  spec->cvec           =  mclvInit(NULL)
   ;  spec->rvec           =  mclvInit(NULL)
;  }


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxIO *xfcl = NULL, *xfmx = NULL

   ;  mclx *dom = NULL,*el2dom = NULL, *mx = NULL
   ;  mclv *universe_rows = NULL, *universe_cols = NULL
   ;  mclv *subspace_rows = NULL, *subspace_cols = NULL
   ;  context_mt  ctxt
   ;  mcxTing* fnout     =  mcxTingNew("out.mcxsubs")

   ;  subspec_mt *specs   =  NULL
   ;  int    n_spec  =  0
   ;  mcxBuf spec_buf

   ;  int digits        =  MCLXIO_VALUE_GETENV
   ;  int a             =  1
   ;  int i             =  0
   ;  mcxbool bCltag    =  FALSE
   ;  mcxbool reread    =  FALSE
   ;  mcxbool skin_read =  FALSE
   ;  mcxbool do_extend =  FALSE
   ;  int rand_mode     =  'm'
   ;  unsigned int seed =  mcxSeed()
   ;  int n_arg_read    =  0
   ;  mcxbits do_block  =  0     /* 1 block, 2 blockc, 4 blocks2 */

   ;  mcxstatus parseStatus = STATUS_OK
   ;  mcxOption* opts, *opt

   ;  srandom(seed)
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

            case MY_OPT_BLOCKC
         :  do_block = 2
         ;  break
         ;

            case MY_OPT_BLOCK
         :  do_block = 1
         ;  break
         ;

            case MY_OPT_BLOCK2
         :  do_block = 4
         ;  break
         ;

            case MY_OPT_OUT
         :  mcxTingWrite(fnout, opt->val)
         ;  break
         ;

            case MY_OPT_CROSS
         :  do_extend = TRUE
         ;  break
         ;

            case MY_OPT_SKIN
         :  skin_read = TRUE
         ;  break
         ;

            case MY_OPT_FROM_DISK
         :  reread = TRUE
         ;  break
         ;

            case MY_OPT_EFAC
         :  efac = atof(opt->val)
         ;  break
         ;

            case MY_OPT_DFAC
         :  dfac = atof(opt->val)
         ;  break
         ;

            case MY_OPT_RFAC
         :  rfac = atof(opt->val)
         ;  break
         ;

            case MY_OPT_CFAC
         :  cfac = atof(opt->val)
         ;  break
         ;

            case MY_OPT_RAND_DISCARD
         :  rand_mode = 'd'
         ;  break
         ;

            case MY_OPT_RAND_EXCLUSIVE
         :  rand_mode = 'e'
         ;  break
         ;

            case MY_OPT_RAND_INTERSECT
         :  rand_mode = 'i'
         ;  break
         ;

            case MY_OPT_RAND_MERGE
         :  rand_mode = 'm'
         ;  break
         ;

            case MY_OPT_TAGGED
         :  bCltag = TRUE
         ;  break
         ;

            case MY_OPT_TAG_DIGITS
         :  digits = atoi(argv[a])
         ;  break
      ;  }
      }
                                    /* fixme hackish, control-flow-wise */
      if (n_arg_read+1 == argc)
      {  if (do_block)
         {  subspec_mt* spec =   mcxBufExtend(&spec_buf, 1, EXIT_ON_FAIL)
         ;  mcxTing* txt   =  mcxTingPrint(NULL, "dom(CR, i()), out(%s)", fnout->str)
         ;  spec_init(spec, txt->str, spec_buf.n, FALSE)
         ;  mcxTingFree(&txt)
      ;  }
         else
         mcxDie(0, me, "no specs found")
   ;  }

      for (a=1+n_arg_read;a<argc;a++)
      {  subspec_mt* spec =   mcxBufExtend(&spec_buf, 1, EXIT_ON_FAIL)
      ;  spec_init(spec, argv[a], spec_buf.n, do_extend)
   ;  }

      n_spec =  mcxBufFinalize(&spec_buf)

   ;  if (!xfmx)
      {  mcxTell(me, "-imx flag is obligatory, see help (-h)")
      ;  mcxExit(1)
   ;  }

      if (reread && !do_block)
      {  universe_rows = mclvNew(NULL, 0)
      ;  universe_cols = mclvNew(NULL, 0)
      ;  if (mclxReadDomains(xfmx, universe_cols, universe_rows))
         mcxDie(1, me, "failed when reading domains")
      ;  mcxIOclose(xfmx)
   ;  }
      else if (skin_read)
      {  universe_rows = mclvNew(NULL, 0)
      ;  universe_cols = mclvNew(NULL, 0)
      ;  if (mclxReadDomains(xfmx, universe_cols, universe_rows))
         mcxDie(1, me, "failed when reading domains")
      ;  mx
         =  mclxAllocZero
            (  mclvClone(universe_cols)
            ,  mclvClone(universe_rows)
            )
      ;  mcxIOclose(xfmx)
   ;  }
      else
      {  mx    =  mclxRead(xfmx, EXIT_ON_FAIL)
      ;  universe_rows =  mclvClone(mx->dom_rows)
      ;  universe_cols =  mclvClone(mx->dom_cols)
   ;  }

      if (dfac)
      {  subspace_cols = mclvClone(universe_cols)
      ;  thin_out(subspace_cols, dfac)
      ;  if (mcldEquate(universe_rows, universe_cols, MCLD_EQ_EQUAL))
         subspace_rows = mclvClone(subspace_cols)
      ;  else
            subspace_rows = mclvClone(universe_rows)
         ,  thin_out(subspace_cols, dfac)
   ;  }
      else
      {  if (cfac)
            subspace_cols = mclvClone(universe_cols)
         ,  thin_out(subspace_cols, cfac)
      ;  else
         subspace_cols = mclvInit(NULL)
      ;  if (rfac)
            subspace_rows = mclvClone(universe_rows)
         ,  thin_out(subspace_cols, rfac)
      ;  else
         subspace_rows = mclvInit(NULL)
   ;  }

      if (xfcl)
      dom = mclxRead(xfcl, EXIT_ON_FAIL)

   ;  mcxIOclose(xfmx)
   ;  mcxIOfree(&xfcl)

   ;  if (do_block && mx && dom != mx)
      {  mclx* block = NULL, *blockc = NULL
      
      ;  if (!dom)
         mcxDie(1, me, "need domain matrix (-dom option)")

      ;  block = do_block & 4 ? mclxBlocks2(mx, dom) : mclxBlocks(mx, dom)

      ;  if (do_block == 2)
         {  mclxScale(block, -1)
         ;  blockc = mclxAdd(mx, block)
      ;  }

         mclxFree(&mx)
      ;  if (blockc)
         {  mclxFree(&block)
         ;  mx = blockc
      ;  }
         else
         mx = block
   ;  }

      if (!dom)
         mcxTell(me, "using -imx matrix for domain retrieval")
      ,  dom = mx

   ;  if (bCltag)
      el2dom = mclxTranspose(dom)

   ;  ctxt.dom             =  dom
   ;  ctxt.mx              =  mx
   ;  ctxt.el2dom          =  el2dom
   ;  ctxt.universe_cols   =  universe_cols
   ;  ctxt.universe_rows   =  universe_rows
   ;  ctxt.subspace_cols   =  subspace_cols
   ;  ctxt.subspace_rows   =  subspace_rows
   ;  ctxt.rand_mode       =  rand_mode
   ;  ctxt.spec_ct         =  i

   ;  for (i=0;i<n_spec;i++)
      {  subspec_mt* spec = specs+i
      ;  if (spec_parse(spec, &ctxt))
         {  mcxErr
            (me, "spec [%s] yields error, ignoring", spec->txt->str)
         ;  mcxTingFree(&(spec->txt))
         ;  continue
      ;  }
   ;  }

      for (i=0;i<n_spec;i++)
      if (specs[i].txt)
      spec_exec
      (  specs+i
      ,  &ctxt
      ,  reread ? xfmx : NULL
      ,  efac
      ,  bCltag
      ,  digits
      )

   ;  mclxFree(&el2dom)
   ;  return 0
;  }


mclx* mclxWeed
(  mclx* mx
,  mclv** colnumspp
,  mclv** rownumspp
,  mcxbits bits
)
   {  long c
   ;  mclv* colnums = mclxColNums(mx, mclvSize, 0)
   ;  mclv* rownums = mclvNew(NULL, 0)

   ;  for (c=0;c<N_COLS(mx);c++)
      mclvAdd(mx->cols+c, rownums, rownums)

   ;  if (colnumspp && rownumspp)
      {  *colnumspp = colnums
      ;  *rownumspp = rownums
      ;  return NULL
   ;  }
      return mclxSub(mx, colnums, rownums)
;  }


mcxstatus parse_fin
(  mcxLink*    src
,  int         n_args
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src

   ;  while ((lk = lk->next))
      {  const char* key = ((mcxTing*) lk->val)->str

      ;  if (!strcmp(key, "vdoms"))
         spec->fin_misc_opts |= (FIN_MISC_DOMC | FIN_MISC_DOMR)
      ;  else if (!strcmp(key, "vdomc"))
         spec->fin_misc_opts |= FIN_MISC_DOMC
      ;  else if (!strcmp(key, "vdomr"))
         spec->fin_misc_opts |= FIN_MISC_DOMR

      ;  else if (!strcmp(key, "empty"))
         spec->fin_misc_opts |= FIN_MISC_EMPTY

      ;  else if (!strcmp(key, "map"))
         spec->fin_map_opts |= (FIN_MAP_CAN_COLS | FIN_MAP_CAN_ROWS)
      ;  else if (!strcmp(key, "mapr"))
         spec->fin_map_opts |= FIN_MAP_CAN_ROWS
      ;  else if (!strcmp(key, "mapc"))
         spec->fin_map_opts |= FIN_MAP_CAN_COLS

      ;  else if (!strcmp(key, "uni"))
         spec->fin_map_opts |= (FIN_MAP_UNI_COLS | FIN_MAP_UNI_ROWS)
      ;  else if (!strcmp(key, "unir"))
         spec->fin_map_opts |= FIN_MAP_UNI_ROWS
      ;  else if (!strcmp(key, "unic"))
         spec->fin_map_opts |= FIN_MAP_UNI_COLS

      ;  else if (!strcmp(key, "weed"))
         spec->fin_map_opts |= (FIN_MAP_WEED_COLS | FIN_MAP_WEED_ROWS)
      ;  else if (!strcmp(key, "weedc"))
         spec->fin_map_opts |= FIN_MAP_WEED_COLS
      ;  else if (!strcmp(key, "weedr"))
         spec->fin_map_opts |= FIN_MAP_WEED_ROWS

      ;  else if (!strcmp(key, "tp"))
         spec->fin_misc_opts |= FIN_MISC_TP
      ;  else if (!strcmp(key, "cc"))
         spec->fin_misc_opts |= FIN_MISC_CHR

      ;  else
         {  mcxErr(me, "unknown mod: <%s>", key)
         ;  break
      ;  }
      }
      return lk ? STATUS_FAIL : STATUS_OK
;  }


mcxstatus parse_out
(  mcxLink*    src
,  int         n_args
,  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxLink* lk = src->next
   ;  mcxTing* fn = lk->val
   ;  mcxstatus status = STATUS_FAIL

   ;  while (1)
      {  if (!fn->len)
         {  mcxErr("parse_out", "expect file name as first OUT arg")
         ;  break
      ;  }

         mcxTingWrite(spec->fname, fn->str)

      ;  while ((lk = lk->next))
         {  const char* str = ((mcxTing*) lk->val)->str

         ;  if (!strcmp(str, "wb"))
            spec->fin_misc_opts |= FIN_WB

         ;  else
            {  mcxErr("parse_out", "unknown directive <%s>", str)
            ;  break
         ;  }
         }

         if (lk)
         break

      ;  status = STATUS_OK
      ;  break
   ;  }

      return status
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

   ;  const mclv* subspace_cols  =  ctxt->subspace_cols
   ;  const mclv* subspace_rows  =  ctxt->subspace_rows

   ;  const mclv* universe_cols  =  ctxt->universe_cols
   ;  const mclv* universe_rows  =  ctxt->universe_rows

   ;  int rand_mode     =  ctxt->rand_mode

   ;  mclv* invec2      =  NULL

   ;  if (itype == 'i' || itype == 'I')
      invec2 = (mclv*) invec              /* modify in 'd/D' case */

   ;  else if (itype == 'd' || itype == 'D')
      invec2 = mclxUnionv(ctxt->dom, invec)

   ;  if (modec)
      {  mclv* invec_c = mclvClone(invec2)

      ;  if (itype == 'I')
         mcldMinus(universe_cols, invec_c, invec_c)
      ;  else if (itype == 'D')
         mcldMinus(ctxt->dom->dom_rows, invec_c, invec_c)

      ;  if ((unsigned char) modec[0] == 'C')
         mcldMinus(universe_cols, invec_c, invec_c)

      ;  mcldMerge(cvec, invec_c, cvec)
      ;  mclvFree(&invec_c)

      ;  if (subspace_cols)
         {  if (rand_mode == 'd')      /* discard */
            mcldMinus(cvec, subspace_cols, cvec)
         ;  else if (rand_mode == 'e')
            mcldMinus(subspace_cols, cvec, cvec)
         ;  else if (rand_mode == 'i')
            mcldMeet(subspace_cols, cvec, cvec)
         ;  else
            mcldMerge(cvec, subspace_cols, cvec)
      ;  }
         mcldMerge(cvec, spec->cvec, spec->cvec)
      ;  spec->n_col_specs++
   ;  }

      if (moder)
      {  mclv* invec_r = mclvClone(invec2)

      ;  if (itype == 'I')
         mcldMinus(universe_rows, invec_r, invec_r)
      ;  else if (itype == 'D')
         mcldMinus(ctxt->dom->dom_rows, invec_r, invec_r)

      ;  if ((unsigned char) moder[0] == 'R')
         mcldMinus(universe_rows, invec_r, invec_r)

      ;  mcldMerge(rvec, invec_r, rvec)
      ;  mclvFree(&invec_r)

      ;  if (subspace_rows)
         {  if (rand_mode == 'd')
            mcldMinus(rvec, subspace_rows, rvec)
         ;  else if (rand_mode == 'e')
            mcldMinus(subspace_rows, rvec, rvec)
         ;  else if (rand_mode == 'i')
            mcldMeet(subspace_rows, rvec, rvec)
         ;  else
            mcldMerge(rvec, subspace_rows, rvec)
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
      {  if (strchr(dtype, 'C'))
            mclvResize(spec->cvec, 0)
         ,  spec->n_col_specs++
      ;  if (strchr(dtype, 'R'))
            mclvResize(spec->rvec, 0)
         ,  spec->n_row_specs++
      ;  return STATUS_OK
   ;  }

      while ((lk = lk->next))
      {  mcxTokFunc tf
      ;  mcxTing* id = lk->val
      ;  char* z

      ;  tf.opts = TOK_DEL_WS

      ;  if
         (  STATUS_OK
         == mcxTokExpectFunc(&tf, id->str, id->len, &z, -1, -1, NULL)
         )
         {  mclVector* vec
         ;  const char* str      =  tf.key->str
         ;  unsigned char itype  =  str[0]
         ;  long twilch = 0

         ;  if (itype != 'i' && itype != 'I' && itype != 'd' && itype != 'D')
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

            if (!(vec = ilSpecToVec(tf.args->next, &twilch, NULL, RETURN_ON_FAIL)))
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

      ;  tf.opts = TOK_DEL_WS

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
            bits = MCLX_GQ
         ;  else if (!strcmp(key, "gt"))
            bits = MCLX_GT
         ;  else if (!strcmp(key, "lt"))
            bits = MCLX_LT
         ;  else if (!strcmp(key, "lq"))
            bits = MCLX_LQ

         ;  if (bits & (MCLX_GQ | MCLX_GT))
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


mcxstatus parse_val
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

      ;  tf.opts = TOK_DEL_WS

      ;  if
         (  STATUS_OK
         == mcxTokExpectFunc(&tf, valspec->str, valspec->len, &z, 1, 1, NULL)
         )
         {  const char* val = ((mcxTing*) tf.args->next->val)->str
         ;  const char* key = tf.key->str
         ;  mcxbits bits = 0
         ;  char* onw = NULL
         ;  double d = strtod(val, &onw)

         ;  if (val == onw)
            {  mcxErr(me, "failed to parse number <%s>", val)
            ;  break
         ;  }

            if (!strcmp(key, "gq"))
            bits = MCLX_GQ
         ;  else if (!strcmp(key, "gt"))
            bits = MCLX_GT
         ;  else if (!strcmp(key, "lt"))
            bits = MCLX_LT
         ;  else if (!strcmp(key, "lq"))
            bits = MCLX_LQ

         ;  if (bits & (MCLX_GQ | MCLX_GT))
            spec->val_min = d
         ;  else
            spec->val_max = d

         ;  spec->sel_val_opts |= bits
         ;  mcxTell(me, "selecting entries <%s> <%f>", key, d)
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

      ;  tf.opts = TOK_DEL_WS

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

            else if (!strcmp(tf.key->str, "out"))
            {  if (parse_out(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "size"))
            {  if (parse_size(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "val"))
            {  if (parse_val(tf.args, n_args, spec, ctxt))
               break
         ;  }

            else if (!strcmp(tf.key->str, "fin"))
            {  if (parse_fin(tf.args, n_args, spec, ctxt))
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


mcxstatus spec_parse
(  subspec_mt* spec
,  context_mt* ctxt
)
   {  mcxTing* txt      =  spec->txt
   ;  mcxstatus status  =  STATUS_FAIL
   ;  int n_args        =  0
   ;  mcxLink* args     =  mcxTokArgs(txt->str, txt->len, &n_args, TOK_DEL_WS)

   ;  if (args)
         mcxTell(me, "dispatching <%d> argument%s", n_args, n_args > 1 ? "s" : "")
      ,  status = dispatch(args, spec, ctxt)

   ;  return status
;  }


void spec_exec
(  subspec_mt* spec
,  context_mt* ctxt
,  mcxIO*   xfmx
,  double   efac
,  mcxbool  bCltag
,  int      digits
)
   {  mclx* sub   =  NULL
   ;  mclx* mx    =  ctxt->mx
   ;  mclx* el2dom=  ctxt->el2dom

   ;  mcxIO *xf = mcxIOnew(spec->fname->str, "w")

   ;  if (!spec->n_row_specs)
      mclvCopy(spec->rvec, ctxt->universe_rows)
   ;  if (!spec->n_col_specs)
      mclvCopy(spec->cvec, ctxt->universe_cols)

   ;  if (mcxIOopen(xf, RETURN_ON_FAIL) == STATUS_FAIL)
      {  mcxErr
         (me, "cannot open file <%s> for writing! Ignoring", xf->fn->str)
      ;  mcxIOfree(&xf)
      ;  return
   ;  }

      if (spec->fin_misc_opts & (FIN_MISC_DOMC | FIN_MISC_DOMR))
      {  mclv* domr =   mclvInit(NULL)
      ;  mclv* domc =   mclvCanonical(NULL, 2, 1.0)
      ;  int cidx   =   0

      ;  if (spec->fin_misc_opts & FIN_MISC_DOMR)
            mcldMerge(domr, spec->rvec, domr)
         ,  cidx++
      ;  if (spec->fin_misc_opts & FIN_MISC_DOMC)
            mcldMerge(domr, spec->cvec, domr)
         ,  cidx++

      ;  mclvResize(domc, cidx)

      ;  sub = mclxAllocZero(domc, domr)

      ;  cidx = 0
      ;  if (spec->fin_misc_opts & FIN_MISC_DOMC)
            mclvAdd(sub->cols+cidx, spec->cvec, sub->cols+cidx)
         ,  cidx++
      ;  if (spec->fin_misc_opts & FIN_MISC_DOMR)
            mclvAdd(sub->cols+cidx, spec->rvec, sub->cols+cidx)
         ,  cidx++

      ;  mclxWrite(sub, xf, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
      ;  mclxFree(&sub)
      ;  mcxIOfree(&xf)
      ;  return
   ;  }

      else if (spec->fin_misc_opts & FIN_MISC_EMPTY)
      sub = mclxAllocZero(mclvClone(spec->cvec), mclvClone(spec->rvec))

   ;  else if (spec->do_extend)
      sub = mclxExtSub(mx, spec->cvec, spec->rvec)

   ;  else if (xfmx)
      sub = mclxSubRead
            (  xfmx
            ,  spec->cvec
            ,  spec->rvec
            ,  EXIT_ON_FAIL
            )
      ,  mcxIOclose(xfmx)
   ;  else                 /* fixme: must check subness */
      sub =  mclxSub(mx, spec->cvec, spec->rvec)

   ;  if (spec->sel_val_opts)
      mclxSelectValues
      (  sub
      ,  spec->sel_val_opts & (MCLX_GQ | MCLX_GT) ? &(spec->val_min) : NULL
      ,  spec->sel_val_opts & (MCLX_LQ | MCLX_LT) ? &(spec->val_max) : NULL
      ,  spec->sel_val_opts
      )

   ;  if (spec->sel_sz_opts)
fprintf(stderr, "%ld %ld %ld\n", (long) spec->sel_sz_opts, spec->sz_min, spec->sz_max)
,     mclxMakeSparse
      (  sub
      ,     spec->sel_sz_opts & MCLX_GQ
         ?  spec->sz_min
         :     spec->sel_sz_opts & MCLX_GT
            ?  spec->sz_min + 1
            :  0
      ,     spec->sel_sz_opts & MCLX_LQ
         ?  spec->sz_max
         :     spec->sel_sz_opts & MCLX_LT
            ?  spec->sz_max - 1
            :  0
      )

   ;  if (efac)
      prune_edges(sub, efac)

   ;  if ((spec->fin_map_opts & FIN_MAP_UNI_COLS) || (spec->fin_map_opts & FIN_MAP_UNI_ROWS))
      mclxChangeDomains
      (  sub
      ,  spec->fin_map_opts & FIN_MAP_UNI_COLS ? ctxt->universe_cols : NULL
      ,  spec->fin_map_opts & FIN_MAP_UNI_ROWS ? ctxt->universe_rows : NULL
      )

   ;  if (spec->fin_misc_opts & FIN_MISC_TP)
      {  mclMatrix* subt = mclxTranspose(sub)
      ;  mclxFree(&sub)
      ;  sub = subt
   ;  }

      if (spec->fin_misc_opts & FIN_MISC_CHR)
      mclxMakeCharacteristic(sub)

   ;  if (spec->fin_map_opts & (FIN_MAP_WEED_COLS | FIN_MAP_WEED_ROWS))
      {  mclv* colnums = NULL, *rownums = NULL
      ;  mcxbits bc
         =  spec->fin_map_opts & FIN_MAP_WEED_COLS ? MCLX_WEED_COLS : 0
      ;  mcxbits br
         =  spec->fin_map_opts & FIN_MAP_WEED_ROWS ? MCLX_WEED_ROWS : 0
      ;  mclxWeed(sub, &colnums, &rownums, bc | br)
   ;  }

      if (spec->fin_map_opts & FIN_MAP_CAN_COLS)
      mclxMapCols(sub, NULL)

   ;  if (spec->fin_map_opts & FIN_MAP_CAN_ROWS)
      mclxMapRows(sub, NULL)


   ;  if (bCltag)
      mclxTaggedWrite(sub, el2dom, xf, digits, RETURN_ON_FAIL)
   ;  else
            spec->fin_misc_opts & FIN_WB
         ?  mclxbWrite(sub, xf, RETURN_ON_FAIL)
         :  mclxWrite(sub, xf, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)

   ;  mclxFree(&sub)
   ;  mcxIOfree(&xf)
;  }

