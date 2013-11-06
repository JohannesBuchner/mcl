/* (c) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* NOTE: with --cleanup, this app should return all memory.
 *   TODO
 *
*/

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "impala/io.h"
#include "impala/stream.h"

#include "impala/matrix.h"
#include "impala/vector.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "impala/pval.h"
#include "impala/scan.h"
#include "impala/app.h"

#include "util/ting.h"
#include "util/err.h"
#include "util/types.h"
#include "util/opt.h"
#include "util/hash.h"


const char* me = "mcxload";
const char* syntax = "Usage: mcxload -abc <fname> -o <fname> [options]\n"
                     "all file names use - to indicate stdin/stdout";

#define  SELECT_IGQ        1 <<  0
#define  SELECT_ILQ        1 <<  1
#define  SELECT_IFLOOR     1 <<  2
#define  SELECT_ICEIL      1 <<  3
#define  SELECT_OGQ        1 <<  4
#define  SELECT_OLQ        1 <<  5
#define  SELECT_OFLOOR     1 <<  6
#define  SELECT_OCEIL      1 <<  7

enum
{  MY_OPT_ABC
,  MY_OPT_123
,  MY_OPT_ETC
,  MY_OPT_ETC_AI
,  MY_OPT_PCK
,  MY_OPT_MIRROR
,  MY_OPT_GRAPH
                  ,  MY_OPT_OUT_MX
,  MY_OPT_OUT_TAB =  MY_OPT_OUT_MX + 2
,  MY_OPT_OUT_TABC
                        ,  MY_OPT_OUT_TABR
,  MY_OPT_STRICT_TAB    =  MY_OPT_OUT_TABR + 2
,  MY_OPT_STRICT_TABC
,  MY_OPT_STRICT_TABR
,  MY_OPT_RESTRICT_TAB
,  MY_OPT_RESTRICT_TABC
,  MY_OPT_RESTRICT_TABR
,  MY_OPT_EXTEND_TAB
,  MY_OPT_EXTEND_TABC
                        ,  MY_OPT_EXTEND_TABR
,  MY_OPT_DEDUP         =  MY_OPT_EXTEND_TABR + 2
,  MY_OPT_STREAM_TRANSFORM
,  MY_OPT_TRANSFORM
,  MY_OPT_STREAM_LOG
,  MY_OPT_STREAM_NEGLOG
,  MY_OPT_IMAGE
,  MY_OPT_TRANSPOSE
,  MY_OPT_CLEANUP
,  MY_OPT_BINARY
,  MY_OPT_DEBUG
,  MY_OPT_HELP
,  MY_OPT_APROPOS
,  MY_OPT_VERSION
}  ;


mcxOptAnchor options[] =
{
   {  "-h"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_HELP
   ,  NULL
   ,  "print this help"
   }
,  {  "--debug"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DEBUG
   ,  NULL
   ,  "debug"
   }
,  {  "--apropos"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "print this help"
   }
,  {  "--binary"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_BINARY
   ,  NULL
   ,  "output matrix in binary format"
   }
,  {  "--clean-up"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MY_OPT_CLEANUP
   ,  NULL
   ,  "free all memore used (test purpose)"
   }
,  {  "--version"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "print version information"
   }
,  {  "-ri"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_IMAGE
   ,  "<max|add|mul>"
   ,  "combine input matrix with its transpose"
   }
,  {  "-re"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_DEDUP
   ,  "<max|add|first|last>"
   ,  "deduplicate repeated entries"
   }
,  {  "--stream-mirror"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_MIRROR
   ,  NULL
   ,  "add y -> x when x -> y"
   }
,  {  "--graph"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_GRAPH
   ,  NULL
   ,  "assume graph, implies single domain"
   }
,  {  "-o"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUT_MX
   ,  "<fname>"
   ,  "output matrix to file <fname>"
   }
,  {  "-cache-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUT_TAB
   ,  "<fname>"
   ,  "output domain tab to file <fname>"
   }
,  {  "-cache-tabc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUT_TABC
   ,  "<fname>"
   ,  "output column tab to file <fname>"
   }
,  {  "-cache-tabr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_OUT_TABR
   ,  "<fname>"
   ,  "output row tab to file <fname>"
   }
,  {  "-extend-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_EXTEND_TAB
   ,  "<fname>"
   ,  "use dom tab in file <fname>, extend if necessary"
   }
,  {  "-extend-tabc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_EXTEND_TABC
   ,  "<fname>"
   ,  "use col tab in file <fname>, extend if necessary"
   }
,  {  "-extend-tabr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_EXTEND_TABR
   ,  "<fname>"
   ,  "use row tab in file <fname>, extend if necessary"
   }
,  {  "-strict-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_STRICT_TAB
   ,  "<fname>"
   ,  "use dom tab in file <fname>, die on miss"
   }
,  {  "-strict-tabc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_STRICT_TABC
   ,  "<fname>"
   ,  "use col tab in file <fname>, die on miss"
   }
,  {  "-strict-tabr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_STRICT_TABR
   ,  "<fname>"
   ,  "use row tab in file <fname>, die on miss"
   }
,  {  "-restrict-tab"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_RESTRICT_TAB
   ,  "<fname>"
   ,  "use dom tab in file <fname>, ignore miss"
   }
,  {  "-restrict-tabc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_RESTRICT_TABC
   ,  "<fname>"
   ,  "use col tab in file <fname>, ignore miss"
   }
,  {  "-restrict-tabr"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_RESTRICT_TABR
   ,  "<fname>"
   ,  "use row tab in file <fname>, ignore miss"
   }
,  {  "-abc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ABC
   ,  "<fname>"
   ,  "input file in abc format"
   }
,  {  "-etc-ai"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ETC_AI
   ,  "<fname>"
   ,  "input file in etc format, auto-increment columns"
   }
,  {  "-etc"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_ETC
   ,  "<fname>"
   ,  "input file in etc format"
   }
,  {  "-packed"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_PCK
   ,  "<fname>"
   ,  "input file in packed format"
   }
,  {  "-123"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_123
   ,  "<fname>"
   ,  "input file in 123 format"
   }
,  {  "-tf"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_TRANSFORM
   ,  "<func(arg)[, func(arg)]*>"
   ,  "apply unary transformations to matrix"
   }
,  {  "-stream-tf"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_STREAM_TRANSFORM
   ,  "<func(arg)[, func(arg)]*>"
   ,  "apply unary transformations to stream values"
   }
,  {  "--stream-log"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_STREAM_LOG
   ,  NULL
   ,  "take log of stream value"
   }
,  {  "--stream-neg-log"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_STREAM_NEGLOG
   ,  NULL
   ,  "take negative log of stream value"
   }
,  {  "-t"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_TRANSPOSE
   ,  NULL
   ,  "transpose result"
   }
,  {  NULL, 0, 0, NULL, NULL  }
}  ;


void usage
(  const char**
)  ;


int main
(  int                  argc
,  const char*          argv[]
)  
   {  mcxIO* xfin   =   mcxIOnew(argc > 1 ? argv[1] : "-", "r")
   ;  mcxIO* xfmx   =   mcxIOnew("-", "w")
   ;  mcxIO* xfcachetab  =   NULL
   ;  mcxIO* xfcachetabc =   NULL
   ;  mcxIO* xfcachetabr =   NULL
   ;  mcxIO* xfusetabc =   NULL
   ;  mcxIO* xfusetabr =   NULL

   ;  mclpAR* stream_transform = NULL
   ;  mcxTing* stream_transform_spec = NULL

   ;  mclpAR* transform = NULL
   ;  mcxTing* transform_spec = NULL

   ;  mcxbits bits_stream_input  =  MCLXIO_STREAM_ABC
   ;  mcxbits bits_stream_tab    =  0
   ;  mcxbits bits_stream_other  =  0

   ;  double (*symfunc) (pval val1, pval val2) = NULL

   ;  mclx* mx = NULL

   ;  mclTab* tabc = NULL
   ;  mclTab* tabr = NULL
   ;  mclTab* tabcxch, *tabrxch
   ;  void (*merge)(void* ivp1, const void* ivp2) = NULL

   ;  mcxbool symmetric =  FALSE
   ;  mcxbool user_tab  =  FALSE
   ;  mcxbool read_tabc =  FALSE
   ;  mcxbool read_tabr =  FALSE
   ;  mcxbool read_tab  =  FALSE
   ;  mcxbool user_symmetric=  FALSE
   ;  mcxbool transpose =  FALSE
   ;  mcxbool cleanup   =  FALSE

   ;  mcxOption* opts, *opt
   ;  mcxstatus parseStatus = STATUS_OK
   
   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)
   ;  opts = mcxOptParse(options, (char**) argv, argc, 1, 0, &parseStatus)

   ;  if (!opts)
      exit(0)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch
      ;  int t = 0

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, me, syntax, 0, MCX_OPT_DISPLAY_SKIP, options)
         ;  return 0
         ;

            case MY_OPT_CLEANUP
         :  cleanup = TRUE
         ;  break
         ;

            case MY_OPT_DEBUG
         :  bits_stream_other |= MCLXIO_STREAM_DEBUG | MCLXIO_STREAM_WARN
         ;  break
         ;

            case MY_OPT_BINARY
         :  mclxSetBinaryIO()
         ;  break
         ;

            case MY_OPT_VERSION
         :  app_report_version(me)
         ;  return 0
         ;

            case MY_OPT_OUT_MX
         :  mcxIOrenew(xfmx, opt->val, NULL)
         ;  break
         ;

            case MY_OPT_OUT_TAB
         :  xfcachetab = mcxIOnew(opt->val, "w")
         ;  symmetric = TRUE
         ;  user_tab = TRUE
         ;  break
         ;

            case MY_OPT_OUT_TABC
         :  xfcachetabc = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_OUT_TABR
         :  xfcachetabr = mcxIOnew(opt->val, "w")
         ;  break
         ;

            case MY_OPT_STRICT_TAB :   t++
         ;  case MY_OPT_RESTRICT_TAB : t++
         ;  case MY_OPT_EXTEND_TAB
         :
            {  xfusetabc = mcxIOnew(opt->val, "r")
            ;  symmetric = TRUE
            ;  user_tab = TRUE
            ;  read_tab  = TRUE
            ;  bits_stream_tab
               |=    t == 2 ? MCLXIO_STREAM_TAB_STRICT
                  :  t == 1 ? MCLXIO_STREAM_TAB_RESTRICT
                  :           MCLXIO_STREAM_TAB_EXTEND
            ;  break
         ;  }

            case MY_OPT_STRICT_TABC :   t++
         ;  case MY_OPT_RESTRICT_TABC : t++
         ;  case MY_OPT_EXTEND_TABC
         :
            {  xfusetabc = mcxIOnew(opt->val, "r")
            ;  read_tabc = TRUE
            ;  bits_stream_tab
               |=    t == 2 ? MCLXIO_STREAM_CTAB_STRICT
                  :  t == 1 ? MCLXIO_STREAM_CTAB_RESTRICT
                  :           MCLXIO_STREAM_CTAB_EXTEND
            ;  break
         ;  }

            case MY_OPT_STRICT_TABR :   t++
         ;  case MY_OPT_RESTRICT_TABR : t++
         ;  case MY_OPT_EXTEND_TABR
         :
            {  xfusetabr = mcxIOnew(opt->val, "r")
            ;  read_tabr = TRUE
            ;  bits_stream_tab
               |=    t == 2 ? MCLXIO_STREAM_RTAB_STRICT
                  :  t == 1 ? MCLXIO_STREAM_RTAB_RESTRICT
                  :           MCLXIO_STREAM_RTAB_EXTEND
            ;  break
         ;  }

            case MY_OPT_PCK
         :  mcxIOrenew(xfin, opt->val, NULL)
         ;  bits_stream_input = MCLXIO_STREAM_PACKED
         ;  break
         ;

            case MY_OPT_123
         :  mcxIOrenew(xfin, opt->val, NULL)
         ;  bits_stream_input = MCLXIO_STREAM_123
         ;  break
         ;

            case MY_OPT_ETC_AI
         :  mcxIOrenew(xfin, opt->val, NULL)
         ;  bits_stream_input = MCLXIO_STREAM_ETC | MCLXIO_STREAM_ETC_AI
         ;  break
         ;

            case MY_OPT_ETC
         :  mcxIOrenew(xfin, opt->val, NULL)
         ;  bits_stream_input = MCLXIO_STREAM_ETC
         ;  break
         ;

            case MY_OPT_ABC
         :  mcxIOrenew(xfin, opt->val, NULL)
         ;  bits_stream_input = MCLXIO_STREAM_ABC
         ;  break
         ;

            case MY_OPT_STREAM_NEGLOG
         :  bits_stream_other |= MCLXIO_STREAM_NEGLOGTRANSFORM
         ;  break
         ;

            case MY_OPT_STREAM_LOG
         :  bits_stream_other |= MCLXIO_STREAM_LOGTRANSFORM
         ;  break
         ;

            case MY_OPT_TRANSFORM
         :  transform_spec = mcxTingNew(opt->val)
         ;  break
         ;

            case MY_OPT_STREAM_TRANSFORM
         :  stream_transform_spec = mcxTingNew(opt->val)
         ;  break
         ;

            case MY_OPT_GRAPH
         :  symmetric = TRUE
         ;  user_symmetric = TRUE
         ;  break
         ;

            case MY_OPT_MIRROR
         :  bits_stream_other |=  MCLXIO_STREAM_MIRROR
         ;  symmetric = TRUE
         ;  user_symmetric = TRUE
         ;  break
         ;

            case MY_OPT_IMAGE
         :  if (!strcmp(opt->val, "max"))
            symfunc = fltMax
         ;  else if (!strcmp(opt->val, "add"))
            symfunc = fltAdd
         ;  else if (!strcmp(opt->val, "mul"))
            symfunc = fltCross
         ;  else
            mcxDie(1, me, "unknown image mode %s", opt->val)
         ;  break
         ;

            case MY_OPT_DEDUP
         :  if (!strcmp(opt->val, "max"))
            merge = mclpMergeMax
         ;  else if (!strcmp(opt->val, "add"))
            merge = mclpMergeAdd
         ;  else if (!strcmp(opt->val, "first"))
            merge = mclpMergeLeft
         ;  else if (!strcmp(opt->val, "last"))
            merge = mclpMergeRight
         ;  else
            mcxDie(1, me, "unknown dup mode %s", opt->val)
         ;  break
         ;

            case MY_OPT_TRANSPOSE
         :  transpose = TRUE
         ;  break
      ;  }
      }
   
      mcxOptFree(&opts)

   ;  if
      (  (bits_stream_other & MCLXIO_STREAM_MIRROR)
      && (bits_stream_input & MCLXIO_STREAM_ETC_AI)
      )
      mcxDie(1, me, "--stream-mirror and -etc-ai cannot jive")

   ;  if
      (  stream_transform_spec
      && !(stream_transform = mclpTFparse(NULL, stream_transform_spec))
      )
      mcxDie(1, me, "stream tf-spec does not parse")

   ;  if
      (  transform_spec
      && !(transform = mclpTFparse(NULL, transform_spec))
      )
      mcxDie(1, me, "matrix tf-spec does not parse")

   ;  if (user_tab && !user_symmetric)
      mcxWarn(me, "I will assume labels are from same domain")

   ;  if (!symmetric)
      bits_stream_other |= MCLXIO_STREAM_TWODOMAINS
   ;  else
      {  if (xfcachetabr || xfcachetabc)
         mcxErr(me, "useless use of -cache-tabc or -cachetabr")
   ;  }

      if (read_tab)
      {  tabc = mclTabRead(xfusetabc, NULL, EXIT_ON_FAIL)
      ;  tabr = NULL
   ;  }
      else
      {  if (read_tabc)
         tabc = mclTabRead(xfusetabc, NULL, EXIT_ON_FAIL)
      ;  if (read_tabr)
         tabr = mclTabRead(xfusetabr, NULL, EXIT_ON_FAIL)
   ;  }

      tabcxch = tabc    /* callee might create new tab */
   ;  tabrxch = tabr    /* callee might create new tab */

   ;  mx =
      mclxIOstreamIn
      (  xfin
      ,  bits_stream_input | bits_stream_tab | bits_stream_other
      ,  stream_transform
      ,  merge
      ,  &tabcxch
      ,  symmetric ? NULL : &tabrxch
      ,  EXIT_ON_FAIL
      )

   ;  if (!mx)
      mcxDie(1, me, "error occurred")
   ;  mcxIOclose(xfin)

   ;  if (transpose || symfunc)
      {  mclx* tp = mclxTranspose(mx)

      ;  if (symfunc)
         {  mclx* sym = mclxBinary(mx, tp, symfunc)
         ;  mclxFree(&mx)
         ;  mclxFree(&tp)
         ;  mx = sym
      ;  }
         else
            mclxFree(&mx)
         ,  mx = tp
   ;  }

      if (transform)
      mclxUnaryList(mx, transform)

   ;  mclxWrite(mx, xfmx, MCLXIO_VALUE_GETENV, RETURN_ON_FAIL)
   ;  mcxIOclose(xfmx)

   ;  if (bits_stream_input & (MCLXIO_STREAM_ABC | MCLXIO_STREAM_ETC))
      {  if (xfcachetab)
         {  mclTabWrite(tabcxch, xfcachetab, NULL, RETURN_ON_FAIL)
         ;  mcxIOclose(xfcachetab)
         ;  if (!tabc)
            mcxTell(me, "tab has %ld entries", (long) tabcxch->domain->n_ivps)
         ;  else if (tabcxch != tabc)
            mcxTell
            (  me
            ,  "tab went from %ld to %ld nodes"
            ,  (long) tabc->domain->n_ivps
            ,  (long) tabcxch->domain->n_ivps
            )
         ;  else
            mcxTell(me, "tab same as before")
      ;  }

         if (!symmetric && xfcachetabc)
         {  if (bits_stream_input & MCLXIO_STREAM_ETC_AI)
            {  mclTabWriteDomain(mx->dom_cols, xfcachetabc, RETURN_ON_FAIL)
            ;  mcxIOclose(xfcachetabc)
         ;  }
            else
            {  mclTabWrite(tabcxch, xfcachetabc, NULL, RETURN_ON_FAIL)
            ;  mcxIOclose(xfcachetabc)
            ;  if (!tabc)
               mcxTell(me, "tabc has %ld entries", (long) tabcxch->domain->n_ivps)
            ;  else if (tabcxch != tabc)
               mcxTell
               (  me
               ,  "tabc went from %ld to %ld nodes"
               ,  (long) tabc->domain->n_ivps
               ,  (long) tabcxch->domain->n_ivps
               )
            ;  else
               mcxTell(me, "tabc same as before")
         ;  }
         }

         if (!symmetric && xfcachetabr)
         {  mclTabWrite(tabrxch, xfcachetabr, NULL, RETURN_ON_FAIL)
         ,  mcxIOclose(xfcachetabr)
         ;  if (!tabr)
            mcxTell(me, "tabr has %ld entries", (long) tabrxch->domain->n_ivps)
         ;  else if (tabrxch != tabr)
            mcxTell
            (  me
            ,  "tabr went from %ld to %ld nodes"
            ,  (long) tabr->domain->n_ivps
            ,  (long) tabrxch->domain->n_ivps
            )
         ;  else
            mcxTell(me, "tabr same as before")
      ;  }

         if (cleanup)
         {  if (tabcxch != tabc)
            mclTabFree(&tabcxch)
         ;  if (tabrxch != tabr)
            mclTabFree(&tabrxch)
      ;  }
      }

      if (cleanup)
      {  mclxFree(&mx)
      ;  mclTabFree(&tabc)
      ;  if (!symmetric)
         mclTabFree(&tabr)
      ;  mcxTingFree(&stream_transform_spec)
      ;  mclpARfree(&stream_transform)
   ;  }

      mcxIOfree(&xfmx)
   ;  mcxIOfree(&xfcachetab)
   ;  mcxIOfree(&xfcachetabc)
   ;  mcxIOfree(&xfcachetabr)
   ;  mcxIOfree(&xfusetabc)
   ;  mcxIOfree(&xfusetabr)
   ;  mcxIOfree(&xfin)
   ;  return 0
;  }

