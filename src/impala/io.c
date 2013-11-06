/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* This is the big file of MCL IO.
 *    -  interchange/binary
 *    -  submatrices from disk
 *    -  domain checks
 *    -  streamed output/input
*/


/* TODO
 *    header parsing code is ugly. Some part is line-based,
 *    some other part is not.
 *
 *    mclx{a,b}ReadBody: what do they free on failure?
 *
 *    This test:
 *    if (tst_rows != mx->dom_rows)
         requires understanding between two pieces of code.
         document, or improve.
*/

/* TODO NOW
 *
 *    (re)strict: what happens with symmetric?
 *       say (symmetric and) restrict_tabc and extend_tabr
mcxload -abc small.data -extend-tabc small.tab-short -o foo -cache-tabr bar
 *    warn on strange combinations, e.g. tab[s] + 123.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "io.h"
#include "vector.h"
#include "iface.h"

#include "util/compile.h"
#include "util/types.h"
#include "util/err.h"
#include "util/minmax.h"
#include "util/alloc.h"
#include "util/ting.h"
#include "util/ding.h"
#include "util/io.h"
#include "util/hash.h"
#include "util/array.h"
#include "util/list.h"
#include "util/tok.h"


static mcxstatus mclxa_parse_dimpart
(  mcxIO          *xf
,  mcxHash        *header
)  ;

static mcxstatus mclxa_parse_domain
(  mcxIO        *xf
,  mclVector**  dompp
)  ;

static void mclxa_write_header
(  const mclMatrix* mx
,  FILE* fp
)  ;

static mclMatrix* mclxa_read_body
(  mcxIO* xf
,  mclVector* dom_cols
,  mclVector* dom_rows
,  mclVector* colmask
,  mclVector* rowmask
,  mcxOnFail ON_FAIL
)  ;


static mclMatrix* mclxb_read_body
(  mcxIO* xf
,  mclVector* dom_cols
,  mclVector* dom_rows
,  mclVector* colmask
,  mclVector* rowmask
,  mcxOnFail ON_FAIL
)  ;


static mcxstatus mclxa_read_dompart
(  mcxIO*      xf
,  mclv       *dom_cols
,  mclv       *dom_rows
,  mcxTing*    line
)  ;


static mcxstatus mclxb_read_dompart
(  mcxIO*      xf
,  mclv*       dom_cols
,  mclv*       dom_rows
,  int*        levelp
)  ;


static mcxstatus mclxa_read_dimpart
(  mcxIO*   xf
,  long     *pn_cols
,  long     *pn_rows
)  ;


/* reads a vector (interchange format)
 * ensures it is in ascending format and has no negative entries
 * or repeated entries.
*/

static mcxstatus mclxa_readavec
(  mcxIO*      xf
,  mclVector*  vec
,  mclpAR*     ar
,  int         fintok
,  mcxbits     bits              /* inherited from mcl{x,v}aReadRaw */
,  mclpAR*     transform
,  void      (*ivpmerge)(void* ivp1, const void* ivp2)
,  double    (*fltbinary)(pval val1, pval val2)
)  ;


enum
{  OK_START  = 0
,  OK_COOKIE            /* not really used */
,  OK_DIMS
,  OK_DOMS
,  OK_BODY
}  ;


typedef struct
{  mcxTing*       line
;  unsigned char  format
;  long           n_cols
;  long           n_rows
;  long           status
;
}  mclxIOinfo     ;

static const char *mclxar = "mclxaRead";


static double loop_adjust_discard
(  mclv* vec
,  long  r
,  void* data
)
   {  return 0.0
;  }


static double loop_adjust_force
(  mclv* vec
,  long  r
,  void* data
)
   {  mclp* ivp = mclvGetIvp(vec, r, NULL)
   ;  return ivp && ivp->val ? ivp->val : 1.0
;  }



void mclxIOinfoFree
(  void*  info_v
)
   {  mclxIOinfo* info = info_v
   ;  mcxTingFree(&(info->line))
   ;  mcxFree(info)
;  }


void mclxIOinfoReset
(  void*  info_v
)
   {  mclxIOinfo* info = info_v
   ;  info->format   =  '0'
   ;  info->n_cols   =  -1
   ;  info->n_rows   =  -1
   ;  info->status   =  OK_START
   ;  mcxTingFree(&(info->line))
;  }


int mclxIOformat
(  mcxIO* xf
)
   {  mclxIOinfo* info  =  xf->usr
   ;  return
         info
      ?  info->format
      :  '0'
;  }


mclxIOinfo* mclxIOinfofy
(  mcxIO* xf
)
   {  mclxIOinfo* info  =  xf->usr
   ;  mcxbool     new   =  info ? FALSE : TRUE

   ;  if (info)
      return info
   ;  else if (!(xf->usr = info = mcxAlloc(sizeof(mclxIOinfo), RETURN_ON_FAIL)))
      return NULL

   ;  if (new)
      {  info->line     =  NULL
      ;  mclxIOinfoReset(info)
      ;  xf->usr_reset  = mclxIOinfoReset
      ;  xf->usr_free   = mclxIOinfoFree
   ;  }

      return info
;  }


static void tell_read_native
(  const mclMatrix* mx
,  const char* mode
)
   {  mcxTell
      (  "mclIO"
      ,  "read native %s %ldx%ld matrix with %ld entries"
      ,  mode
      ,  (long) N_ROWS(mx)
      ,  (long) N_COLS(mx)
      ,  (unsigned long) mclxNEntries(mx)
      )
;  }


static void tell_wrote_native
(  const mclMatrix* mx
,  const char* mode
,  const mcxIO* xf
)
   {  mcxTell
      (  "mclIO"
      ,  "wrote native %s %ldx%ld matrix with %ld entries to stream <%s>"
      ,  mode
      ,  (long) N_ROWS(mx)
      ,  (long) N_COLS(mx)
      ,  (unsigned long) mclxNEntries(mx)
      ,  xf->fn->str
      )
;  }


unsigned long get_env_flags
(  const char* opt
)
   {  unsigned long val = 0
   ;  const char* envp  =  getenv(opt)
   ;  if (envp)
      val = strtol(envp, NULL, 10)
   ;  return val
;  }


unsigned long get_quad_mode
(  const char* opt
)
   {  unsigned long val
      
   ;  if ((val = get_env_flags(opt)))

   ;  else if (!strcmp(opt, "MCLXIOVERBOSITY"))
      val = 8     /* default verbose,  apps can not override */
   ;  else if (!strcmp(opt, "MCLXIOFORMAT"))
      val = 2     /* do default interchange, apps can not override */

   ;  return val
;  }



mcxbool mclxIOsetQMode
(  const char* opt
,  unsigned long newmode
)
   {  int mode = get_quad_mode(opt)
   ;  mcxTing* tmp
      = mcxTingPrint(NULL, "%s=%ld", opt, (unsigned long) (newmode & 10))
   ;  mcxbool ok = FALSE
   ;  char* str = mcxTinguish(tmp)

   ;  while (1)
      {  if (mode & 10)     /* modes 2 and 8 cannot be overridden */
         break
      ;  if (putenv(str))
         break
      ;  ok = TRUE
      ;  break
   ;  }

      return ok
;  }



mcxbool mclxIOgetQMode
(  const char* opt
)
   {  int mode = get_quad_mode(opt)
   ;  if (mode & 3)
      return 0
   ;  else if (mode & 12)
      return 1
   ;  return 1
;  }


int set_interchange_digits
(  int valdigits
)
   {  const char* envp  =  getenv("MCLXIODIGITS")

   ;  if (valdigits == MCLXIO_VALUE_GETENV)
      {  if (envp)
         valdigits = strtol(envp, NULL, 10)
      ;  else
         valdigits = 4
   ;  }

      if (valdigits < -1)     /* 0 is valid and means: no digits please */
      valdigits = 4
   ;  else if (valdigits > 16)
      valdigits = 16

   ;  return valdigits
;  }



mcxstatus mclxReadDimensions
(  mcxIO          *xf
,  long           *pn_cols
,  long           *pn_rows
)
   {  unsigned char format = '0'
   ;  mclxIOinfo* info = mclxIOinfofy(xf)
   
   ;  if (!info || (!xf->fp && mcxIOopen(xf, RETURN_ON_FAIL) != STATUS_OK))
      {  if (!info)
         mcxErr("mclxReadDimensions", "mclxIOinfo malloc failure")
      ;  return STATUS_FAIL
   ;  }

      if (info->status >= OK_DIMS)
      {  *pn_cols = info->n_cols
      ;  *pn_rows = info->n_rows
      ;  return STATUS_OK
   ;  }

         /* do not attempt bin write for nonseekable streams
          * otherwise we cannot pipe matrices around
         */
#if 1
      if (mcxFPisSeekable(xf->fp) && mcxIOtryCookie(xf, mclxCookie))
#else
      if (mcxIOtryCookie(xf, mclxCookie))
#endif
      {  format = 'b'
      ;  fread(pn_cols, sizeof(long), 1, xf->fp)
      ;  fread(pn_rows, sizeof(long), 1, xf->fp)
   ;  }
      else if (mclxa_read_dimpart(xf, pn_cols, pn_rows) == STATUS_OK)
      format = 'a'

   ;  if (format == '0')
      {  mcxErr("mclxReadDimensions", "could not parse header")
      ;  return STATUS_FAIL
   ;  }
      else
      {  info->format = format
      ;  info->n_cols = *pn_cols
      ;  info->n_rows = *pn_rows
      ;  info->status = OK_DIMS
   ;  }

      return STATUS_OK
;  }


mcxstatus mclxReadDomains
(  mcxIO*      xf
,  mclv*       dom_cols
,  mclv*       dom_rows
)
   {  long n_cols = -1, n_rows = -1  
   ;  mclxIOinfo* info = NULL

;if (!xf || !dom_rows)
fprintf(stderr, "--> !xf||dom_rows!\n")

   ;  if (mclxReadDimensions(xf, &n_cols, &n_rows))
      return STATUS_FAIL

   ;  info = xf->usr

   ;  if (info->status >= OK_DOMS)
      return STATUS_FAIL

   ;  if (info->format == 'b')
      {  if (mclxb_read_dompart(xf, dom_cols, dom_rows, NULL))
         return STATUS_FAIL
   ;  }
      else if (info->format == 'a')
      {  mcxTing* line  = mcxTingEmpty(NULL, 80)
      ;  if (mclxa_read_dompart(xf, dom_cols, dom_rows, line))
         {  mcxTingFree(&line)
         ;  return STATUS_FAIL
      ;  }
         ((mclxIOinfo*) xf->usr)->line   =  line
   ;  }
      else
      return STATUS_FAIL

   ;  info->status = OK_DOMS
   ;  return STATUS_OK
;  }



mclMatrix* mclxReadBody
(  mcxIO* xf
,  mclVector* dom_cols
,  mclVector* dom_rows
,  mclVector* colmask
,  mclVector* rowmask
,  mcxOnFail ON_FAIL
)
   {  mclxIOinfo *info = xf->usr
   ;  if (info->format == 'b')
      return mclxb_read_body(xf, dom_cols, dom_rows, colmask, rowmask, ON_FAIL)
   ;  else if (info->format == 'a')
      return mclxa_read_body(xf, dom_cols, dom_rows, colmask, rowmask, ON_FAIL)
   ;  return NULL
;  }


mclMatrix* mclxSubRead
(  mcxIO* xf
,  mclVector* colmask         /* create submatrix on this domain */
,  mclVector* rowmask         /* create submatrix on this domain */
,  mcxOnFail ON_FAIL
)
   {  return mclxSubReadx(xf, colmask, rowmask, ON_FAIL, 0)
;  }


mclMatrix* mclxSubReadx
(  mcxIO* xf
,  mclVector* colmask         /* create submatrix on this domain */
,  mclVector* rowmask         /* create submatrix on this domain */
,  mcxOnFail ON_FAIL
,  mcxbits    bits
)
   {  mclv* dom_cols = mclvNew(NULL, 0)
   ;  mclv* dom_rows = mclvNew(NULL, 0)
   ;  mcxstatus status = STATUS_FAIL

   ;  while (1)
      {  if (mcxIOtestOpen(xf, ON_FAIL))  
         break
      
      ;  if (mclxReadDomains(xf, dom_cols, dom_rows))
         break

      ;  if
         (  bits & MCL_READX_GRAPH
         && !MCLD_EQUAL(dom_cols, dom_rows)
         )
         {  mcxErr("mclxReadGraph", "domains are not equal in file %s", xf->fn->str)
         ;  break
      ;  }
         status = STATUS_OK
      ;  break
   ;  }

      if (status)
      {  mclvFree(&dom_rows)
      ;  mclvFree(&dom_cols)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, "mclxSubReadx", "curtains")
      ;  return NULL
   ;  }

      return mclxReadBody(xf, dom_cols, dom_rows, colmask, rowmask, ON_FAIL)
;  }


mcxstatus mclIOvcheck
(  mclv* vec
,  mclv* dom
)
   {  long ct = 0
   ;  const char* me = "mclIOvcheck"

   ;  if (get_env_flags("MCLXIOUNCHECKED"))
      return STATUS_OK

   ;  if (mcldIsCanonical(dom))
      return
      mclvCheck
      (  vec
      ,  MCLV_MINID(dom)
      ,  MCLV_MAXID(dom)
      ,  MCLV_CHECK_DEFAULT
      ,  RETURN_ON_FAIL
      )
   ;  else if
      (  dom->n_ivps
      && vec->n_ivps < dom->n_ivps / (log(dom->n_ivps) + 1)
      )
      {  long i, prev_idx = -1
      ;  mclp* ivp = NULL
      ;  for (i=0;i<vec->n_ivps;i++)
         {  long idx = vec->ivps[i].idx
         ;  if (!(ivp = mclvGetIvp(dom, idx, ivp)))
            {  mcxErr(me, "alien entry %ld in vid %ld", idx, vec->vid)
            ;  return STATUS_FAIL
         ;  }
            if (idx <= prev_idx)
            {  mcxErr(me, "no ascent from %ld to %ld", prev_idx, idx)
            ;  return STATUS_FAIL
         ;  }
            prev_idx = idx
      ;  }
      }
      else if ((ct = mcldCountSet(vec, dom, MCLD_CT_LDIFF)))
      {  mcxErr
         (  me
         ,  "%ld alien entries in vid %ld"
         ,  (long) ct
         ,  (long) vec->vid
         )
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }


static mclMatrix* mclxb_read_body_all
(  mcxIO* xf
,  mclVector* dom_cols
,  mclVector* dom_rows
,  mcxOnFail ON_FAIL
)
   {  mclMatrix* mx     =  NULL
   ;  long n_rows       =  0
   ;  long n_cols       =  0
   ;  int level         =  0
   ;  int szl           =  sizeof(long)
   ;  mcxstatus status  =  STATUS_FAIL
   ;  long n_mod        =  0
   ;  mcxbool  progress =     isatty(fileno(stderr))
                           && mclxIOgetQMode("MCLXIOVERBOSITY")

   ;  if (progress)
      fprintf(stderr, "[mclIO full] reading <%s> ", xf->fn->str)

   ;  while (1)
      {  n_cols = dom_cols->n_ivps
      ;  n_rows = dom_rows->n_ivps

      ;  n_mod = MAX(1+(n_cols-1)/40, 1)

      ;  if (!(mx = mclxAllocZero(dom_cols, dom_rows)))
         break

      ;  level++

      ;  {  long k      =  0              /* skip the offset array */
         ;  if (fseek(xf->fp, (1+n_cols)*szl, SEEK_CUR))
            break
         ;  level++

         ;  while (k < dom_cols->n_ivps)
            {  mclv* veck   = mx->cols+k

            ;  if (progress && (k+1) % n_mod == 0)
               fputc('.', stderr)

            ;  if (mclvEmbedRead(veck, xf, ON_FAIL))
               break

            ;  level++
            ;  if (veck->vid != dom_cols->ivps[k].idx)
               break

            ;  level++
            ;  if (mclIOvcheck(veck, dom_rows))
               break

            ;  level++
            ;  k++
         ;  }

            if (k != dom_cols->n_ivps)
            break
         ;  level++                         /*  ignore end of matrix offset */
      ;  }

         status = STATUS_OK
      ;  break
   ;  }

      if (progress)
      fputc('\n', stderr)

   ;  if (status)
      {  mcxErr
         (  "mclIO"
         ,  "failed to read native binary "
            "%ldx%ld matrix from stream <%s> at level <%ld>"
         ,  (long) N_ROWS(mx)
         ,  (long) N_COLS(mx)
         ,  xf->fn->str
         ,  (long) level
         )
      ;  mclxFree(&mx)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, "mclIO", "exiting")
   ;  }
      else if (mclxIOgetQMode("MCLXIOVERBOSITY"))
      tell_read_native(mx, "binary")

   ;  return mx
;  }


static mclMatrix* mclxb_read_body
(  mcxIO* xf
,  mclVector* dom_cols
,  mclVector* dom_rows
,  mclVector* colmask
,  mclVector* rowmask
,  mcxOnFail ON_FAIL
)
#define  BREAK_IF(clause)   if (clause) { break; } else { acc++; }
   {  mclMatrix* mx     =  NULL
   ;  long n_cols       =  dom_cols->n_ivps
   ;  int acc           =  0
   ;  int szl           =  sizeof(long)
   ;  mcxstatus status  =  STATUS_FAIL
   ;  long n_mod        =  0, n_mod_gauge = 0
   ;  long* oa          =  NULL
   ;  char* mode        =  "sparse"
   ;  mcxbool  progress =     isatty(fileno(stderr))
                           && mclxIOgetQMode("MCLXIOVERBOSITY")

   ;  if (!colmask && !rowmask)
      return mclxb_read_body_all(xf, dom_cols, dom_rows, ON_FAIL)

   ;  else if
      (  colmask
      && colmask->n_ivps > dom_cols->n_ivps / 16
      )
      {  if (!(oa = mcxAlloc((1+n_cols)*szl, ON_FAIL)))
         return NULL
      ;  mode = "dense"       /* prepare for reading all ofsets */
   ;  }

      if (progress)
      fprintf(stderr, "[mclIO %s] reading <%s> ", mode, xf->fn->str)

   ;  while (1)
      {  n_mod_gauge = colmask ? colmask->n_ivps : n_cols

      ;  n_mod = MAX(1+(n_mod_gauge-1)/40, 1)

      ;  if (!colmask)
         colmask = dom_cols
      ;  if (!rowmask)
         rowmask = dom_rows

      ;  BREAK_IF(!(mx = mclxAllocZero(colmask, rowmask)))

         {  long oa_start  =  ftell(xf->fp)     /* start of offset array */
         ;  long k =  0, vec_os  = -1, v_pos = 0
         ;
            BREAK_IF(oa && (1+n_cols) != fread(oa, szl, 1+n_cols, xf->fp))

            if (oa_start >= 0)
            while (k < colmask->n_ivps)
            {  long vec_vid = colmask->ivps[k].idx   /* MUST be sorted */
            ;  mclv* veck = mx->cols+k
            ;  vec_os = mclvGetIvpOffset(dom_cols, vec_vid, vec_os)

            ;  if (progress && (k+1) % n_mod == 0)
               fputc('.', stderr)

            ;  if (vec_os < 0)               /* mask entry not present */
               {  k++
               ;  continue
            ;  }
                                             /* fetch the offset */
               if (oa)
               v_pos = oa[vec_os]
            ;  else
               {  BREAK_IF(fseek(xf->fp, oa_start + vec_os * szl, SEEK_SET))
                  BREAK_IF(1 != fread(&v_pos, szl, 1, xf->fp))
               }

               BREAK_IF(fseek(xf->fp, v_pos, SEEK_SET))
               BREAK_IF(mclvEmbedRead(veck, xf, ON_FAIL))
               BREAK_IF(veck->vid != vec_vid)
               BREAK_IF(mclIOvcheck(veck, dom_rows))

               if (rowmask != dom_rows)
               mcldMeet(veck, rowmask, veck)

            ;  k++
         ;  }
            else
            break

         ;  BREAK_IF(k != colmask->n_ivps)
                                    /*  fetch end of matrix offset */
            BREAK_IF(fseek(xf->fp, oa_start + n_cols * szl, SEEK_SET))
            BREAK_IF(1 != fread(&v_pos, szl, 1, xf->fp))
            BREAK_IF(fseek(xf->fp, v_pos, SEEK_SET))
         }
         status = STATUS_OK
      ;  break
   ;  }

      if (progress)
      fputc('\n', stderr)

   ;  if (oa)
      mcxFree(oa)

   ;  if (colmask != dom_cols)
      mclvFree(&dom_cols)
   ;  if (rowmask != dom_rows)
      mclvFree(&dom_rows)

   ;  if (status)
      {  mcxErr
         (  "mclIO"
         ,  "failed to read native binary "
            "%ldx%ld matrix from stream <%s> at level <%ld>"
         ,  (long) N_ROWS(mx)
         ,  (long) N_COLS(mx)
         ,  xf->fn->str
         ,  (long) acc
         )
      ;  mclxFree(&mx)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, "mclIO", "exiting")
   ;  }
      else if (mclxIOgetQMode("MCLXIOVERBOSITY"))
      tell_read_native(mx, "binary")

   ;  return mx
;  }



static mcxstatus mclxb_read_dompart
(  mcxIO* xf
,  mclv* dom_cols
,  mclv* dom_rows
,  int* levelp
)
   {  long n_read =  0
   ;  int level   =  0
   ;  mcxstatus status  = STATUS_FAIL
   ;  mclxIOinfo  *info =  xf->usr
   ;  long n_cols =  info->n_cols
   ;  long n_rows =  info->n_rows
   ;  long flags  =  0
   ;  int szl     =  sizeof(long)

   ;  n_read += fread(&flags, szl, 1, xf->fp)

   ;  while (1)
      {  if (n_read != 1)
         break
      ;  level++

      ;  if (flags & 1)
            mclvCanonical(dom_cols, n_cols, 1.0)
         ,  level++
      ;  else if (mclvEmbedRead(dom_cols, xf, RETURN_ON_FAIL))
         break
      ;  level++

      ;  if (flags & 2)
            mclvCanonical(dom_rows, n_rows, 1.0)
         ,  level++
      ;  else if (mclvEmbedRead(dom_rows, xf, RETURN_ON_FAIL))
         break
      ;  level++

      ;  status = STATUS_OK
      ;  break
   ;  }
      if (levelp)
      *levelp += level
   ;  return status
;  }


mcxstatus mclxbWrite
(  const mclMatrix*  mx
,  mcxIO*            xf
,  mcxOnFail         ON_FAIL
)
#define  BREAK_IF(clause)   if (clause) { break; } else { acc++; }
   {  long      n_cols  =  N_COLS(mx)
   ;  long      n_rows  =  N_ROWS(mx)
   ;  long      flags   =  0
   ;  mclVector*vec     =  mx->cols
   ;  mcxstatus status  =  STATUS_FAIL
   ;  long      v_pos   =  0
   ;  int       acc     =  0
   ;  FILE*     fout    =  NULL
   ;  int       szl     =  sizeof(long)
   ;  long      n_mod   =  MAX(1+(n_cols-1)/40, 1)
   ;  mcxbool progress  =     isatty(fileno(stderr))
                           && mclxIOgetQMode("MCLXIOVERBOSITY")

#if 0
   ;  mcxIOclose(xf)
   ;  mcxIOrenew(xf, NULL, "wb")
#endif

   ;  if (progress)
      fprintf(stderr, "[mclIO] writing <%s> ", xf->fn->str)

   ;  if (mcldIsCanonical(mx->dom_cols))
      flags |= 1
   ;  if (mcldIsCanonical(mx->dom_rows))
      flags |= 2

   ;  while (1)
      {  BREAK_IF (xf->fp == NULL && (mcxIOopen(xf, ON_FAIL) != STATUS_OK))
         BREAK_IF (!mcxIOwriteCookie(xf, mclxCookie))

         fout = xf->fp
      ;  
         BREAK_IF (1 != fwrite(&n_cols, szl, 1, fout))
         BREAK_IF (1 != fwrite(&n_rows, szl, 1, fout))
         BREAK_IF (1 != fwrite(&flags, szl, 1, fout))
         BREAK_IF (!(flags & 1) && STATUS_FAIL == mclvEmbedWrite(mx->dom_cols, xf))
         BREAK_IF (!(flags & 2) && STATUS_FAIL == mclvEmbedWrite(mx->dom_rows, xf))

            /* Write vector offsets (plus one for end of matrix body)
             * offsets are written relative to beginning.
            */
         BREAK_IF ((v_pos = ftell(fout)) < 0)

         v_pos += (1 + n_cols) * szl

      ;  while (vec < mx->cols+n_cols)
         {  BREAK_IF (1 != fwrite(&v_pos, szl, 1, fout))
            v_pos += 2 * szl + sizeof(double)+ vec->n_ivps * sizeof(mclIvp)
                                        /* -^- vid, n_ivps, val, ivps */
         ;  vec++
         ;  if (progress && (vec-mx->cols) % n_mod == 0)
            fputc('.', stderr)
      ;  }
         BREAK_IF (vec != mx->cols+n_cols)
         BREAK_IF (1 != fwrite(&v_pos, sizeof(long), 1, fout))
                                       /* Write columns */   
         n_cols      =  N_COLS(mx)
      ;  vec         =  mx->cols

      ;  while (vec < mx->cols+n_cols)
         {  BREAK_IF (STATUS_FAIL == mclvEmbedWrite(vec++, xf))
         }

         BREAK_IF (vec != mx->cols+n_cols)

         status = STATUS_OK
      ;  break
   ;  }
      if (progress)
      fputc('\n', stderr)

   ;  if (STATUS_FAIL == status)
      {  mcxErr
         (  "mclIO"
         ,  "failed to write native binary %ldx%ld matrix to stream <%s>"
            " at level %d"
         ,  (long) N_ROWS(mx)
         ,  (long) N_COLS(mx)
         ,  xf->fn->str
         ,  acc
         )
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, "mclIO", "exiting")
   ;  }
      else if (mclxIOgetQMode("MCLXIOVERBOSITY"))
      tell_wrote_native(mx, "binary", xf)

   ;  return status
;  }
#undef  BREAK_IF


/* reads single required part, so does not read too far
 * This thing was coded way too heavy and cumbersome.
*/
static mcxstatus mclxa_read_dimpart
(  mcxIO*   xf
,  long     *pn_cols
,  long     *pn_rows
)
   {  mcxHash* header      =  mcxHashNew(4, mcxTingHash, mcxTingCmp)
   ;  mcxTing* txtmx       =  mcxTingNew("mcltype")
   ;  mcxTing* txtdim      =  mcxTingNew("dimensions")
   ;  mcxKV    *kvtp, *kvdim
   ;  mcxstatus status     =  STATUS_OK

   ;  if(mcxIOfind(xf, "(mclheader", RETURN_ON_FAIL) != STATUS_OK)
      {  mcxHashFree(&header, NULL, NULL) /* hash still empty */
      ;  return STATUS_FAIL
   ;  }

      mclxa_parse_dimpart(xf, header)  /* fills hash */
   /* fixme; check return status etc; (errors are noticed below though) */

   ;  kvtp  =  mcxHashSearch(txtmx, header, MCX_DATUM_FIND)
   ;  kvdim =  mcxHashSearch(txtdim, header, MCX_DATUM_FIND)

   ;  mcxTingFree(&txtmx)
   ;  mcxTingFree(&txtdim)

   ;  if (!kvtp)
      {  mcxErr(mclxar, "expected <mcltype matrix> specification not found")
      ;  mcxIOpos(xf, stderr)
      ;  status =  STATUS_FAIL
   ;  }
      else if
      (  !kvdim
      || (  sscanf
            (  ((mcxTing*) kvdim->val)->str
            ,  "%ldx%ld"
            ,  pn_rows
            ,  pn_cols
            )
            < 2
         )
      )
      {  mcxErr(mclxar, "expected <dimensions MxN> specification not found")
      ;  mcxIOpos(xf, stderr)
      ;  status =  STATUS_FAIL
   ;  }
      else if (*pn_rows < 0 || *pn_cols < 0)
      {  mcxErr
         (  mclxar
         ,  "each dimension must be nonnegative (found %ldx%ld pair)"
         ,  (long) *pn_rows
         ,  (long) *pn_cols
         )
      ;  status =  STATUS_FAIL
   ;  }

      mcxHashFree(&header, mcxTingFree_v, mcxTingFree_v)
   ;  return status
;  }


/* 
 * May read too far, hence line argument.
 * needs the pp indirection, because domains may not exist,
 * implying canonical domains.
 * This is conveyed by storing a NULL pointer.
*/

static mcxstatus mclxa_parse_dompart
(  mcxIO        *xf
,  mclVector**  dom_colspp             /* stores NULL if canonical */
,  mclVector**  dom_rowspp             /* stores NULL if canonical */
,  mcxTing*     line
)
   {  mclVector*  dom_cols =  NULL
   ;  mclVector*  dom_rows =  NULL
   ;  mcxstatus   status   =  STATUS_FAIL

   ;  line = mcxTingEmpty(line, 80)

   ;  while (STATUS_OK == mcxIOreadLine(xf, line, MCX_READLINE_CHOMP))
      {  if (strncmp(line->str, "(mcl", 4))
         continue

      ;  if (!strncmp(line->str, "(mclcols", 8))
         {  if (dom_cols || mclxa_parse_domain(xf, &dom_cols) == STATUS_FAIL)
            {  mcxErr(mclxar, "error parsing column domain")
            ;  break
         ;  }
         }
         else if (!strncmp(line->str, "(mclrows", 8))
         {  if (dom_rows || mclxa_parse_domain(xf, &dom_rows) == STATUS_FAIL)
            {  mcxErr(mclxar, "error parsing row domain")
            ;  break
         ;  }
         }
         else if (!strncmp(line->str, "(mcldoms", 8))
         {  if
            (  dom_cols
            || dom_rows
            || mclxa_parse_domain(xf, &dom_cols) == STATUS_FAIL
            )
            {  mcxErr(mclxar, "error parsing row domain")
            ;  break
         ;  }
            dom_rows = mclvClone(dom_cols)
         ;  status = STATUS_OK
         ;  break
      ;  }
         else if (!strncmp(line->str, "(mclmatrix", 10))
         {  status = STATUS_OK
         ;  break
      ;  }
         else
         {  mcxErr(mclxar, "unknown header type <%s>", line->str)
         ;  break
      ;  }
      }

      if (xf->ateof)       /* mcxassemble needs this */
      status = STATUS_OK

   ;  if (status)
      {  mclvFree(&dom_cols)
      ;  mclvFree(&dom_rows)
   ;  }

      *dom_colspp = dom_cols  /* possibly NULL */
   ;  *dom_rowspp = dom_rows  /* possibly NULL */
   ;  return status
;  }


/* This one is local. Must be used after mclxReadDimensions has had its way.
 * It may read too far, hence returns line.
 *    todo: rather try to fseek back ?
*/

static mcxstatus mclxa_read_dompart
(  mcxIO*   xf
,  mclv*    dom_cols
,  mclv*    dom_rows
,  mcxTing* line
)
   {  mclxIOinfo *info = xf->usr
   ;  long n_cols = info->n_cols
   ;  long n_rows = info->n_rows
   ;  mclv* tmp_col = NULL, *tmp_row = NULL
   ;  mcxstatus status = STATUS_FAIL

   ;  line = mcxTingEmpty(line, 80)

   ;  while (1)
      {  if (mclxa_parse_dompart(xf, &tmp_col, &tmp_row, line) != STATUS_OK)
         {  mcxErr(mclxar, "error constructing domains")
         ;  break
      ;  }

         if (!tmp_row)
         tmp_row = mclvCanonical(NULL, n_rows, 1.0)
      ;  else if (tmp_row->n_ivps != n_rows)
         {  mcxErr
            (  mclxar
            ,  "row domain count <%ld> != dimension <%ld>"
            ,  (long) tmp_row->n_ivps
            ,  (long) n_rows
            )
         ;  break
      ;  }

         if (!tmp_col)
         tmp_col = mclvCanonical(NULL, n_cols, 1.0)
      ;  else if (tmp_col->n_ivps != n_cols)
         {  mcxErr
            (  mclxar
            ,  "col domain count <%ld> != dimension <%ld>"
            ,  (long) tmp_col->n_ivps
            ,  (long) n_cols
            )
         ;  break
      ;  }
         status = STATUS_OK
      ;  break
   ;  }

      if (!status)
         mclvCopy(dom_cols, tmp_col)
      ,  mclvCopy(dom_rows, tmp_row)

   ;  mclvFree(&tmp_col)
   ;  mclvFree(&tmp_row)

   ;  return status
;  }


static mclMatrix* mclxa_read_body
(  mcxIO          *xf
,  mclv*          dom_cols
,  mclv*          dom_rows
,  mclv*          colmask
,  mclv*          rowmask
,  mcxOnFail      ON_FAIL
)
   {  mcxstatus   status   =  STATUS_FAIL
   ;  mclxIOinfo* info     =  xf->usr
   ;  mcxTing*    line     =  mcxTingNew(info->line->str)
   ;  mclMatrix*  mx       =  NULL
   ;  mcxbits     bits     =  MCLV_WARN_REPEAT

   ;  while (1)
      {  while
         (  strncmp(line->str, "(mclmatrix", 10)
         && STATUS_OK == mcxIOreadLine(xf, line, MCX_READLINE_CHOMP)
         )
         ;
         /* fixme should add section parsing [ delimited by ^(mcl .. ^) ] */

         if (!line->len)
         {  mcxErr(mclxar, "(mclmatrix section not found")
         ;  break
      ;  }

         if (mcxIOfind(xf, "begin", RETURN_ON_FAIL) == STATUS_FAIL)
         {  mcxErr(mclxar, "begin token not found in matrix specification")
         ;  break
      ;  }

                           /* fixedleak?: if col,rowmask must free dom_col,rows */
         if (!colmask)
         colmask  = dom_cols
      ;  if (!rowmask)
         rowmask  = dom_rows
      ;  mx = mclxAllocZero(colmask, rowmask)

      ;  if
         (  mclxaSubReadRaw
           (  xf, mx, dom_cols, dom_rows, ON_FAIL
           , ')', bits, NULL, mclpMergeLeft, fltMax
           )
           != STATUS_OK
         )
         {  mx = NULL      /* twas freed by mclxaSubReadRaw */
         ;  break
      ;  }
         status = STATUS_OK
      ;  break
   ;  }

      mcxTingFree(&line)

   ;  if (colmask != dom_cols)
      mclvFree(&dom_cols)
   ;  if (rowmask != dom_rows)
      mclvFree(&dom_rows)

   ;  if (status)
      {  mclxFree(&mx)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, "mclxa_read_body", "error occurred")
   ;  }
      else if (mclxIOgetQMode("MCLXIOVERBOSITY"))
      tell_read_native(mx, "interchange")

   ;  return mx
;  }


/* fixme; can't I make this more general,
 * with callback and void* argument ?
 *
 * fixme; remove offset == vid constraint [allready fixed?].
*/

mcxstatus mclxTaggedWrite
(  const mclMatrix*     mx
,  const mclMatrix*     el2dom
,  mcxIO                *xfout
,  int                  valdigits
,  mcxOnFail            ON_FAIL
)
   {  int   i
   ;  FILE* fp
   ;  const char* me = "mclxTaggedWrite"  

   ;  if (!xfout->fp && mcxIOopen(xfout, ON_FAIL) != STATUS_OK)
      {  mcxErr(me, "cannot open stream <%s>", xfout->fn->str)
      ;  return STATUS_FAIL
   ;  }

      fp =  xfout->fp
   ;  mclxa_write_header(mx, fp)

   ;  for (i=0;i<N_COLS(mx);i++)
      {  mclVector*  mvec  =  mx->cols+i
      ;  mclVector*  dvec  =  mclxGetVector
                              (  el2dom, mvec->vid, RETURN_ON_FAIL, NULL)
                             /*  fixme; make more efficient */
      ;  long tag = dvec && dvec->n_ivps ? dvec->ivps[0].idx : -1
      ;  int j

      ;  if (!mvec->n_ivps)
         continue

      ;  fprintf(fp, "%ld(%ld)  ", (long) mvec->vid, (long) tag)

      ;  for (j=0;j<mvec->n_ivps;j++)
         {  long  hidx  =  (mvec->ivps+j)->idx
         ;  double hval =  (mvec->ivps+j)->val

         ;  dvec  =  mclxGetVector(el2dom, hidx, RETURN_ON_FAIL, NULL)
         ;  tag   =  dvec && dvec->n_ivps ? dvec->ivps[0].idx : -1

         ;  if (valdigits > -1)
            fprintf
            (  fp
            ,  " %ld(%ld):%.*f"
            ,  (long) hidx
            ,  (long) tag
            ,  (int) valdigits
            ,  (double) hval
            )
         ;  else
            fprintf
            (  fp
            ,  " %ld(%ld)"
            ,  (long) hidx
            ,  (long) tag
            )
      ;  }
         fprintf(fp, " $\n")
   ;  }

      fprintf(fp, ")\n")
   ;  if (mclxIOgetQMode("MCLXIOVERBOSITY"))
      tell_wrote_native(mx, "interchange tagged", xfout)

   ;  return STATUS_OK
;  }


void mclxa_write_header
(  const mclMatrix* mx
,  FILE* fp
)
   {  int  leadwidth =  log10(MAXID_ROWS(mx)+1) + 2

   ;  fprintf
      (  fp
      ,  "(mclheader\nmcltype matrix\ndimensions %ldx%ld\n)\n"
      ,  (long) N_ROWS(mx)
      ,  (long) N_COLS(mx)
      )

   ;  if
      (  !mcldIsCanonical(mx->dom_rows)
      || !mcldIsCanonical(mx->dom_cols)
      )
      {  if (mcldEquate(mx->dom_rows, mx->dom_cols, MCLD_EQ_EQUAL))
         {  fputs("(mcldoms\n", fp)
         ;  mclvaDump
            (  mx->dom_cols
            ,  fp
            ,  leadwidth
            ,  MCLXIO_VALUE_NONE
            ,  FALSE
            )
         ;  fputs(")\n", fp)
      ;  }
         else
         {  if (!mcldIsCanonical(mx->dom_rows))
            {  fputs("(mclrows\n", fp)
            ;  mclvaDump
               (  mx->dom_rows
               ,  fp
               ,  leadwidth
               ,  MCLXIO_VALUE_NONE
               ,  FALSE
               )
            ;  fputs(")\n", fp)
         ;  }
            if (!mcldIsCanonical(mx->dom_cols))
            {  fputs("(mclcols\n", fp)
            ;  mclvaDump
               (  mx->dom_cols
               ,  fp
               ,  leadwidth
               ,  MCLXIO_VALUE_NONE
               ,  FALSE
               )
            ;  fputs(")\n", fp)
         ;  }
         }
      }

      fputs("(mclmatrix\nbegin\n", fp)
;  }


mcxstatus mclxWrite
(  const mclMatrix*        mx
,  mcxIO*                  xfout
,  int                     valdigits
,  mcxOnFail               ON_FAIL
)
   {  if (!xfout->fp && mcxIOopen(xfout, ON_FAIL) != STATUS_OK)
      return STATUS_FAIL
   ;  if (mclxIOgetQMode("MCLXIOFORMAT"))
      return mclxbWrite(mx, xfout, ON_FAIL)
   ;  return mclxaWrite(mx, xfout, valdigits, ON_FAIL)
;  }


mcxstatus mclxaWrite
(  const mclMatrix*        mx
,  mcxIO*                  xfout
,  int                     valdigits
,  mcxOnFail               ON_FAIL
)
   {  int   i
                  /* fixme; need more sanity checks on N_ROWS(mx) ? ? */
   ;  int   leadwidth   =  log10(MAXID_ROWS(mx)+1) + 2
   ;  const char* me    =  "mclxaWrite"
   ;  unsigned long flags =  get_env_flags("MCLXICFLAGS")
   ;  long n_mod        =  MAX(1+(N_COLS(mx)-1)/40, 1)
   ;  mcxbool progress  =     isatty(fileno(stderr))
                           && mclxIOgetQMode("MCLXIOVERBOSITY")
   ;  FILE* fp

   ;  valdigits = set_interchange_digits(valdigits)

   ;  if (progress)
      fprintf(stderr, "[mclIO] writing <%s> ", xfout->fn->str)

   ;  if (!xfout->fp && mcxIOopen(xfout, RETURN_ON_FAIL) != STATUS_OK)
      {  mcxErr(me, "cannot open stream <%s>", xfout->fn->str)
      ;  return STATUS_FAIL
   ;  }

      fp =  xfout->fp
   ;  mclxa_write_header(mx, fp)

   ;  for (i=0;i<N_COLS(mx);i++)
      {  if ((mx->cols+i)->n_ivps || flags & 1)
         mclvaDump
         (  mx->cols+i
         ,  fp
         ,  leadwidth
         ,  valdigits
         ,  FALSE
         )
      ;  if (progress && (i+1) % n_mod == 0)
         fputc('.', stderr)
   ;  }
      if (progress)
      fputc('\n', stderr)

   ;  fprintf(fp, ")\n")

   ;  if (mclxIOgetQMode("MCLXIOVERBOSITY"))
      tell_wrote_native(mx, "interchange", xfout)

   ;  return STATUS_OK
;  }


void mcxPrettyPrint
(  const mclMatrix*        mx
,  FILE*                   fp
,  int                     width
,  int                     digits
,  const char              msg[]
)
   {  int   i
   ;  char     bgl[]       =  " [ "
   ;  char     eol[]       =  "  ]"
   ;  mclMatrix*  tp       =  mclxTranspose(mx)
   ;  char  voidstring[20]

   ;  width                =  MAX(2, width)
   ;  width                =  MIN(width, 15)

   ;  memset(voidstring, ' ', width-2)
   ;  *(voidstring+width-2) = '\0'

   ;  for (i=0;i<N_COLS(tp);i++)
      {  mclVector*  rowVec   =  tp->cols+i
      ;  mclIvp*  domIvp      =  tp->dom_rows->ivps - 1
      ;  mclIvp*  domIvpMax   =  tp->dom_rows->ivps + tp->dom_rows->n_ivps

      ;  fprintf(fp, "%s", bgl)

      ;  while (++domIvp < domIvpMax)
         {  mclIvp* ivp = mclvGetIvp(rowVec, domIvp->idx, NULL)
         ;  if (!ivp)
            fprintf(fp, " %s--", voidstring)
         ;  else
            fprintf(fp, " %*.*f", (int) width, (int) digits, (double) ivp->val)
      ;  }
         fprintf(fp, "%s\n", eol)
   ;  }

      mclxFree(&tp)
   ;  if (msg)
      fprintf(fp, "^ %s\n", msg)
;  }


void mclxBoolPrint
(  mclMatrix*     mx
,  int            mode
)
   {  int      i, t                 
   ;  const char  *space   =  mode & 1 ? "" : " "
   ;  const char  *empty   =  mode & 1 ? " " : "  "

   ;  fprintf(stdout, "\n  ")        
   ;  for (i=0;i<N_ROWS(mx);i++)    
      fprintf(stdout, "%d%s", (int) i % 10, space)   
   ;  fprintf(stdout, "\n")

   ;  for (i=0;i<N_COLS(mx);i++)
      {  int         last        =  0
      ;  mclIvp*     ivpPtr      =  (mx->cols+i)->ivps
      ;  mclIvp*     ivpPtrMax   =  ivpPtr + (mx->cols+i)->n_ivps
      ;  fprintf(stdout, "%d ", (int) i%10)
                                    
      ;  while (ivpPtr < ivpPtrMax) 
         {  for (t=last;t<ivpPtr->idx;t++) fprintf(stdout, "%s", empty)
         ;  fprintf(stdout, "@%s", space)
         ;  last = (ivpPtr++)->idx + 1
      ;  }        
         for (t=last;t<N_ROWS(mx);t++) fprintf(stdout, "%s", empty)
      ;  fprintf(stdout, " %d\n", (int) i%10)   
   ;  }           

      fprintf(stdout, "  ")
   ;  for (i=0;i<N_ROWS(mx);i++)
      fprintf(stdout, "%d%s", (int) i%10, space)
   ;  fprintf(stdout, "\n")
;  }


void mclvaDump
(  const mclVector*  vec
,  FILE*    fp
,  int      leadwidth
,  int      valdigits
,  mcxbool  doHeader
)
   {  long vid = vec->vid
   ;  int nr_chars   =     0
   ;  const char* eov =    " $\n"
   ;  int n_converted = 0
   ;  int i

   ;  if (leadwidth > 20)
      leadwidth = 20
   ;  if (leadwidth < 0)
      leadwidth = 0

   ;  if (doHeader)
      {  fprintf(fp , "(mclheader\nmcltype vector\n)\n" "(mclvector\nbegin\n")
      ;  eov = " $\n)\n"
   ;  }

      if (vid>=0)
      {  fprintf(fp, "%ld%n", (long) vid, &n_converted)
      ;  nr_chars += n_converted
      ;  if (vec->val != 0.0)
            fprintf(fp, ":%.*f%n", valdigits, (double) vec->val, &n_converted)
         ,  nr_chars += n_converted
      ;  while (nr_chars < leadwidth -1)  /* we get one below */
         {  fputs(" ", fp)
         ;  nr_chars++
      ;  }
      }

      for (i=0; i<vec->n_ivps;i++)
      {  if (valdigits > -1)
         {  fprintf
            (  fp
            ,  " %ld:%.*f%n"
            ,  (long) (vec->ivps+i)->idx
            ,  (int) valdigits
            ,  (double) (vec->ivps+i)->val
            ,  &n_converted
            )
         ;  nr_chars += n_converted
      ;  }
         else if (valdigits == MCLXIO_VALUE_NONE)
         {  fprintf(fp, " %ld%n",  (long) (vec->ivps+i)->idx, &n_converted)
         ;  nr_chars += n_converted
      ;  }

                     /* assume leadwidth is correlated to index range */
         if (nr_chars > 70-leadwidth && i < vec->n_ivps-1)
         {  int j
         ;  fputc('\n', fp)
         ;  nr_chars = 0
         ;  if (vid >= 0)
            {  for (j=0;j<leadwidth-1;j++)     /* somewhat stupid */
                  fputc(' ', fp)
               ,  nr_chars++
         ;  }
         }
      }
      fputs(eov, fp)
;  }


static void report_vector_size
(  const char*             action
,  const mclVector*           vec
)
   {  char                 report[80]

   ;  snprintf
      (  report
      ,  sizeof report
      , "%s %ld pair%s\n"
      ,  action
      ,  (long) vec->n_ivps
      ,  vec->n_ivps == 1 ? "" : "s"
      )
   ;  mcxTell(NULL, report)
;  }


mcxstatus mclvEmbedRead
(  mclVector*     vec
,  mcxIO*         xf
,  mcxOnFail      ON_FAIL
)
   {  long n_ivps =  0      /* fixme; check vec             */
   ;  long n_read =  0
   ;  int level = 0
   ;  mcxstatus status = STATUS_FAIL

   ;  while (1)
      {  n_read += fread(&n_ivps, sizeof(long), 1, xf->fp)
      ;  n_read += fread(&(vec->vid), sizeof(long), 1, xf->fp)
      ;  n_read += fread(&(vec->val), sizeof(double), 1, xf->fp)

      ;  if (n_read != 3)
         break                                                 ;  level++

      ;  if (n_ivps)
         {  if (!mclvResize(vec, n_ivps))
            break                                              ;  level++

         ;  if
            (  n_ivps
            != (n_read = fread(vec->ivps, sizeof(mclIvp), n_ivps, xf->fp))
            )
            {  if (n_read >= 0)
               mclvResize(vec, n_read)
            ;  break
         ;  }
         }
         else
         mclvResize(vec, 0)

      ;  status = STATUS_OK
      ;  break
   ;  }

      if (status)
      {  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, "mclvEmbedRead", "failed to read vector")
      ;  else
         mcxErr("mclvEmbedRead", "failed at level %d",  level)
   ;  }

      return status
;  }



mclpAR* mclpReaDaList
(  mcxIO   *xf
,  mclpAR  *ar
,  mclpAR  *transform
,  int      fintok
)
   {  const char* me = "mclpReaDaList"
   ;  mcxbool ok = FALSE

   ;  if (!ar)
      ar = mclpARensure(NULL, 100)
   ;  else
      ar->n_ivps = 0

   ;  while (1)
      {  long idx
      ;  double val
      ;  int c = mcxIOskipSpace(xf)  /* c is ungotten */

      ;  if (c == fintok)
         {  mcxIOstep(xf)  /* discard '$' or EOF etc */
         ;  ok = TRUE
         ;  break
      ;  }
         else if (c == '#')
         {  mcxIOdiscardLine(xf)
         ;  continue
      ;  }

         if (mcxIOexpectNum(xf, &idx, RETURN_ON_FAIL) == STATUS_FAIL)
         {  mcxErr(me, "expected row index")
         ;  break
      ;  }
         else if (idx > PNUM_MAX)
         {  mcxErr
            (  me
            ,  "index <%ld> exceeds %s capacity"
            ,  idx
            ,  IVP_NUM_TYPE
            )
         ;  break
      ;  }
         else if (idx < 0)
         {  mcxErr(me, "found negative index <%ld>", (long) idx)
         ;  break
      ;  }

expect_val
      :  if (':' == (c = mcxIOskipSpace(xf)))
         {  mcxIOstep(xf) /* discard ':' */
         ;  if (mcxIOexpectReal(xf, &val, RETURN_ON_FAIL) == STATUS_FAIL)
            {  mcxErr(me, "expected value after row index <%ld>", (long) idx)
            ;  break
         ;  }
         }
         else if ('(' == c)
         {  if (mcxIOfind(xf, ")", RETURN_ON_FAIL) == STATUS_FAIL)
            {  mcxErr(me, "could not skip over s-expression <%ld>", (long) idx)
            ;  break             /* ^ that's a bit grandiloquent, yes */
         ;  }
            goto expect_val
      ;  }
         else
         val = 1.0

      ;  if (mclpARextend(ar, idx, val))
         {  mcxErr(me, "could not extend/insert ar-ivp")
         ;  break
      ;  }
         if (transform)
         {  mclp* ivp = ar->ivps+ar->n_ivps-1
         ;  ivp->val = mclpUnary(ivp, transform)
      ;  }
      }

      if (!ok)
      {  mclpARfree(&ar)
      ;  return NULL
   ;  }

      return ar
;  }


static mcxstatus mclxa_readavec
(  mcxIO*      xf
,  mclv*       dst
,  mclpAR*     ar
,  int         fintok
,  mcxbits     warn_repeat
,  mclpAR*     transform
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
,  double (*fltbinary)(pval val1, pval val2)
)
   {  mclpAR* arcp = ar

   ;  if (!(ar = mclpReaDaList(xf, ar, transform, fintok)))
      return STATUS_FAIL

   ;  mclvFromIvps_x
      (  dst
      ,  ar->ivps
      ,  ar->n_ivps
      ,  ar->sorted
      ,  warn_repeat
      ,  ivpmerge
      ,  fltbinary
      )

   ;  if (!arcp)
      mclpARfree(&ar)

   ;  return STATUS_OK
;  }


mcxstatus mclvEmbedWrite
(  const mclVector*     vec
,  mcxIO*               xf
)
   {  long sz     =  vec->n_ivps
   ;  long vid    =  vec->vid
   ;  double val  =  vec->val
   ;  long n_written = 0

   ;  n_written += fwrite(&sz, sizeof(long), 1, xf->fp)
   ;  n_written += fwrite(&vid, sizeof(long), 1, xf->fp)
   ;  n_written += fwrite(&val, sizeof(double), 1, xf->fp)

   ;  if (vec->n_ivps)
      n_written += fwrite(vec->ivps, sizeof(mclIvp), vec->n_ivps, xf->fp)

   ;  if (n_written != 3 + vec->n_ivps)
      return STATUS_FAIL

   ;  return STATUS_OK
;  }


/*
 * fixme this interface is obsolete
*/

mcxstatus mclvbWrite
(  const mclVector      *vec
,  mcxIO                *xfout
,  mcxOnFail            ON_FAIL
)
   {  mcxstatus         status

   ;  if (xfout->fp == NULL && mcxIOopen(xfout, ON_FAIL) != STATUS_OK)
      {  mcxErr("mclvbWrite", "cannot open stream <%s>", xfout->fn->str)
      ;  return STATUS_FAIL
   ;  }

      if (!mcxIOwriteCookie(xfout, mclvCookie))
      return STATUS_FAIL

   ;  if (STATUS_OK == (status = mclvEmbedWrite(vec, xfout)))
      report_vector_size("wrote", vec)

   ;  return status
;  }


static mcxstatus mclxa_parse_domain
(  mcxIO        *xf
,  mclVector**  dompp
)
   {  mclVector *dom = *dompp

   ;  if (!dom)
      dom = mclvInit(NULL)

   ;  *dompp = dom

   ;  if
      (  mclxa_readavec
         (  xf
         ,  dom
         ,  NULL
         ,  '$'
         ,  MCLV_WARN_REPEAT
         ,  NULL
         ,  mclpMergeLeft
         ,  NULL
         )
         == STATUS_OK
      )
      {  if (')' == mcxIOskipSpace(xf))
         {  mcxIOstep(xf) /* discard ')' */
         ;  return STATUS_OK
      ;  }
         return STATUS_FAIL
   ;  }
      return STATUS_FAIL
;  }


static mcxstatus mclxa_parse_dimpart
(  mcxIO        *xf
,  mcxHash      *header
)
   {  int  n
   ;  mcxTing   *keyTxt  =   mcxTingEmpty(NULL, 30)
   ;  mcxTing   *valTxt  =   mcxTingEmpty(NULL, 30)
   ;  mcxTing   *line    =   mcxTingEmpty(NULL, 30)

   ;  while (STATUS_OK == mcxIOreadLine(xf, line, MCX_READLINE_CHOMP))
      {  if (*(line->str+0) == ')')
         break

      ;  mcxTingEnsure(keyTxt, line->len)
      ;  mcxTingEnsure(valTxt, line->len)

      ;  n = sscanf(line->str, "%s%s", keyTxt->str, valTxt->str)

      ;  if (n < 2)
         continue
      ;  else
         {  mcxTing* key   =  mcxTingNew(keyTxt->str)
         ;  mcxTing* val   =  mcxTingNew(valTxt->str)
         ;  mcxKV*   kv    =  mcxHashSearch(key, header, MCX_DATUM_INSERT)
         ;  kv->val        =  val
      ;  }
      }

      mcxTingFree(&line)
   ;  mcxTingFree(&valTxt)
   ;  mcxTingFree(&keyTxt)
   ;  return STATUS_OK
;  }


mclMatrix* mclxReadx
(  mcxIO*      xf
,  mcxOnFail   ON_FAIL
,  mcxbits     bits
)  
   {  return mclxSubReadx(xf, NULL, NULL, ON_FAIL, bits)
;  }


mclMatrix* mclxRead
(  mcxIO       *xf
,  mcxOnFail   ON_FAIL
)  
   {  return mclxSubReadx(xf, NULL, NULL, ON_FAIL, 0)
;  }


void mclFlowPrettyPrint
(  const mclMatrix*  mx
,  FILE*             fp
,  int               digits
,  const char        msg[]
)
   {  mcxPrettyPrint
      (  mx
      ,  fp
      ,  digits+2
      ,  digits
      ,  msg
      )
;  }


void mclvaWrite               /* fixme should use set_interchange_digits */
(  const mclVector*  vec
,  FILE*             fp
,  int               valdigits
)  
   {  mclvaDump
      (  vec
      ,  fp
      ,  0
      ,  valdigits
      ,  TRUE
      )
;  }



mclpAR *mclpaReadRaw
(  mcxIO       *xf
,  mcxOnFail   ON_FAIL
,  int         fintok     /* e.g. EOF or '$' */
)
   {  mclpAR* ar = mclpReaDaList(xf, NULL, NULL, fintok)
   ;  if (!ar && ON_FAIL != RETURN_ON_FAIL)
      mcxExit(1)
   ;  return ar
;  }



/* fixme; add expect_vid argument */
/* fixme;? add ar buffer argument */
mclVector* mclvaReadRaw
(  mcxIO          *xf
,  mclpAR*        ar
,  mcxOnFail      ON_FAIL
,  int            fintok     /* e.g. EOF or '$' */
,  mcxbits        warn_repeat
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
)
   {  mclVector* vec = mclvInit(NULL)  /* cannot use create; vec must be ok */
   ;  if
      (  mclxa_readavec(xf, vec, ar, fintok, warn_repeat, NULL, ivpmerge, NULL)
      != STATUS_OK
      )
      {  mcxErr("mclvaReadRaw", "read failed in <%s>", xf->fn->str)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxExit(1)
      ;  return NULL
   ;  }
      if (0 && mclxIOgetQMode("MCLXIOVERBOSITY"))
      mcxTell
      (  "mclIO"
      ,  "read raw interchange <%ld> vector from stream <%s>"
      ,  (long) vec->n_ivps
      ,  xf->fn->str
      )
   ;  return vec
;  }


/* fixme/todo:
 * should caller free mx on error?
*/

mcxstatus mclxaSubReadRaw
(  mcxIO          *xf
,  mclMatrix      *mx            /* fit raw matrix onto domains of mx   */
,  mclv           *tst_cols      /* raw matrix must satisfy this domain */
,  mclv           *tst_rows      /* raw matrix must satisfy this domain */
,  mcxOnFail      ON_FAIL
,  int            fintok         /* e.g. EOF or ')' */
,  mcxbits        bits
,  mclpAR*        transform
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
,  double (*fltbinary)(pval val1, pval val2)
)
   {  const char* me       =  "mclxaSubReadRaw"
   ;  mclpAR*     ar       =  mclpARensure(NULL, 100)
   ;  mclv*       discardv =  mclvNew(NULL, 0)

   ;  int         N_cols   =  N_COLS(mx)
   ;  int         n_cols   =  0
   ;  int         n_mod    =  MAX(1+(N_cols-1)/40, 1)
   ;  mcxstatus   status   =  STATUS_FAIL
   ;  mcxbool     progress =     isatty(fileno(stderr))
                              && mclxIOgetQMode("MCLXIOVERBOSITY")

   ;  if (progress)
      fprintf(stderr, "[mclIO] reading <%s> ", xf->fn->str)

   ;  if (xf->fp == NULL && (mcxIOopen(xf, ON_FAIL) != STATUS_OK))
      mcxErr(me, "cannot open stream <%s>", xf->fn->str)
   ;  else
      while (1)
      {  long        cidx     =  -1
      ;  double      cval     =  0.0
      ;  mclVector*  vec      =  NULL
      ;  int         a        =  mcxIOskipSpace(xf)
      ;  int         warnmask = ~0

      ;  if (a == fintok)
         {  status = STATUS_OK
         ;  break
      ;  }
         else if (a == '#')
         {  mcxIOdiscardLine(xf)
         ;  continue
      ;  }

         if (mcxIOexpectNum(xf, &cidx, RETURN_ON_FAIL) == STATUS_FAIL)
         {  mcxErr(me, "expected column index")
         ;  break
      ;  }
         else if (cidx > PNUM_MAX)
         {  mcxErr
            (  me
            ,  "column index <%ld> exceeds %s capacity"
            ,  cidx
            ,  IVP_NUM_TYPE
            )
         ;  break
      ;  }

         if (':' == (a = mcxIOskipSpace(xf)))
         {  mcxIOstep(xf) /* discard ':' */
         ;  if (mcxIOexpectReal(xf, &cval, RETURN_ON_FAIL) == STATUS_FAIL)
            {  mcxErr
               (me, "expected value after column identifier <%ld>", (long) cidx)
            ;  break
         ;  }
         }

         if (mclvGetIvp(tst_cols, cidx, NULL))
         {  if (!(vec = mclxGetVector(mx, cidx, RETURN_ON_FAIL, NULL)))
               vec = discardv
            ,  warnmask = 0
            /* could be submatrix read */
      ;  }
         else
         {  mcxErr(me, "found alien col index <%ld> (discarding)", (long) cidx)
         ;  vec = discardv
         ;  warnmask = 0
      ;  }

         vec->val = cval

         /* In case of alien col index, still use mclxa_readavec to
          * plough through the file.
         */

      ;  if
         (  mclxa_readavec
            (xf,vec,ar,'$', bits & warnmask, transform, ivpmerge, fltbinary)
         != STATUS_OK
         )
         {  mcxErr(me, "vector read failed for column <%ld>", (long) cidx)
         ;  break
      ;  }

         /* now vector should be strictly ascending */

         if (vec != discardv)
         {  mclv* ldif = NULL
         ;  if (mclIOvcheck(vec, tst_rows))
            {  mclvCleanse(vec)
            ;  ldif = mcldMinus(vec, tst_rows, NULL)
            ;  mcxErr
               (  me
               ,  "alien row indices in column <%ld> - (a total of %ld)"
               ,  (long) cidx
               ,  (long) ldif->n_ivps
               )
            ;  mcxErr(me, "the first is <%ld> (discarding all)", (long) ldif->ivps[0].idx)
            ;  mclvFree(&ldif)
            ;  mcldMeet(vec, tst_rows, vec)
         ;  }
            if (tst_rows != mx->dom_rows)    /* fixme document or improve */
            mcldMeet(vec, mx->dom_rows, vec)
      ;  }

         n_cols++
      ;  if (progress && n_cols % n_mod == 0)
         fputc('.', stderr)
   ;  }
      if (progress)
      fputc('\n', stderr)

                        /* hack: if fintok == ')' then caller is verbose */
   ;  if (fintok == EOF && mclxIOgetQMode("MCLXIOVERBOSITY"))
      mcxTell
      (  "mclIO"
      ,  "read raw interchange %ldx%ld matrix from stream <%s>"
      ,  (long) N_ROWS(mx)
      ,  (long) N_COLS(mx)
      ,  xf->fn->str
      )
   ;  mclpARfree(&ar)
   ;  mclvFree(&discardv)
   ;

      if (status)
      {  if (ON_FAIL == RETURN_ON_FAIL)
         {  mclxFree(&mx)  
         ;  return STATUS_FAIL
      ;  }
         else
         mcxExit(1)
   ;  }

      return STATUS_OK
;  }


void mclvaDump2
(  const mclVector*  vec
,  FILE*    fp
,  int      n_per_line        /* 0: all on a single line */
,  int      valdigits
,  const char* sep
,  mcxbits  opts           /* header_yes/eov_no/val_no/vid_yes */
)
   {  long vid             =  vec->vid
   ;  const char* eov      =  " $\n"

   ;  mcxbool print_value  =  valdigits >= 0 && !(opts & MCLVA_DUMP_VALUE_NO)
   ;  mcxbool print_eov    =  !(opts & MCLVA_DUMP_EOV_NO)
   ;  mcxbool print_vid    =  vid >= 0 && !(opts & MCLVA_DUMP_VID_NO)
   ;  mcxbool print_header =  opts & MCLVA_DUMP_HEADER_YES
   ;  mcxbool print_trail  =  opts & MCLVA_DUMP_TRAIL_YES
   ;  int i

   ;  if (!sep)
      sep = " "

   ;  if (print_header)
      {  fprintf(fp , "(mclheader\nmcltype vector\n)\n" "(mclvector\nbegin\n")
      ;  eov = " $\n)\n"
   ;  }

      if (print_vid)
      {  fprintf(fp, "%ld", (long) vid)
      ;  if (vec->val != 0.0 && print_value)
         fprintf(fp, ":%.*f", valdigits, (double) vec->val)
   ;  }

      for (i=0; i<vec->n_ivps;i++)
      {  if (i || print_vid)
         fputs(sep, fp)

      ;  if (print_value)
         fprintf
         (  fp
         ,  "%ld:%.*f"
         ,  (long) (vec->ivps+i)->idx
         ,  (int) valdigits
         ,  (double) (vec->ivps+i)->val
         )
      ;  else
         fprintf(fp, "%ld",  (long) (vec->ivps+i)->idx)
   ;  }

      if (print_trail)
      fputs(sep, fp)
   ;  if (print_eov)
      fputs(eov, fp)
;  }



void mclxIOdumpSet
(  mclxIOdumper*  dump
,  mcxbits        modes
,  const char*    sep_lead
,  const char*    sep_row
,  const char*    sep_val
)
   {  dump->modes    =  modes
   ;  dump->sep_lead =  sep_lead ? sep_lead: "\t"
   ;  dump->sep_row  =  sep_row  ? sep_row : "\t"
   ;  dump->sep_val  =  sep_val  ? sep_val : ":"
   ;  dump->threshold = 0.0
;  }


static void  dump_label
(  mcxIO* xf
,  const mclTab* tab
,  const char* label
,  long idx
)
   {  if (tab)
      {  if (label == tab->na->str)
         fprintf(xf->fp, "?_%ld", idx)
      ;  else
         fputs(label, xf->fp)
   ;  }
      else
      fprintf(xf->fp, "%ld", idx)
;  }


mcxstatus mclxIOdump
(  mclx*       mx
,  mcxIO*      xf_dump
,  mclxIOdumper* dumper
,  const mclTab*  tabc
,  const mclTab*  tabr
,  mcxOnFail   ON_FAIL
)
   {  mcxbits modes = dumper->modes
   ;  if (mcxIOtestOpen(xf_dump, ON_FAIL))
      return STATUS_FAIL

   ;  if
      (  (modes & (MCLX_DUMP_LOOP_FORCE | MCLX_DUMP_LOOP_NONE))
      && mclxIsGraph(mx)
      )
      {  double (*op)(mclv* vec, long r, void* data)
         =     modes & MCLX_DUMP_LOOP_NONE
            ?  loop_adjust_discard
            :  loop_adjust_force
      ;  mclxAdjustLoops(mx, op, NULL)
   ;  }
   
      if (modes & MCLX_DUMP_PAIRS)
      {  long i, j, labelc_o = -1
      ;  char* labelc = "", *labelr = ""

      ;  for (i=0;i<N_COLS(mx);i++)
         {  mclv* vec = mx->cols+i
         ;  long labelr_o = -1

         ;  if (tabc)
            labelc = mclTabGet(tabc, vec->vid, &labelc_o)

         ;  for (j=0;j<vec->n_ivps;j++)
            {  mclIvp* ivp = vec->ivps+j
            ;  if (ivp->val < dumper->threshold)
               continue

            ;  if (tabr)
               labelr = mclTabGet(tabr, ivp->idx, &labelr_o)

            ;  dump_label(xf_dump, tabc, labelc, vec->vid)
            ;  fputs(dumper->sep_row, xf_dump->fp)
            ;  dump_label(xf_dump, tabr, labelr, ivp->idx)

            ;  if (modes & MCLX_DUMP_VALUES)
               fprintf(xf_dump->fp, "%s%f", dumper->sep_row, ivp->val)
            ;  fputc('\n', xf_dump->fp)
         ;  }
         }
      }
      else if (modes & (MCLX_DUMP_LINES | MCLX_DUMP_RLINES))
      {  long i, j, labelc_o = -1
      ;  char* labelc = "", *labelr = ""

      ;  for (i=0;i<N_COLS(mx);i++)
         {  mclv* vec = mx->cols+i
         ;  long labelr_o = -1
         ;  mcxbool busy = FALSE

         ;  if (tabc)
            labelc = mclTabGet(tabc, vec->vid, &labelc_o)

         ;  if (modes & MCLX_DUMP_LINES)
            dump_label(xf_dump, tabc, labelc, vec->vid)

         ;  for (j=0;j<vec->n_ivps;j++)
            {  const char* sep
               =     ((modes & MCLX_DUMP_RLINES) && !busy)
                  ?  ""
                  :     (j && !(modes & MCLX_DUMP_RLINES))
                     ?  dumper->sep_row
                     :  dumper->sep_lead
            ;  mclIvp* ivp = vec->ivps+j
            ;  if (ivp->val < dumper->threshold)
               continue

            ;  busy = TRUE
            ;  if (tabr)
               labelr = mclTabGet(tabr, ivp->idx, &labelr_o)

            ;  fputs(sep, xf_dump->fp)
            ;  dump_label(xf_dump, tabr, labelr, ivp->idx)

            ;  if (modes & MCLX_DUMP_VALUES)
               fprintf(xf_dump->fp, "%s%f", dumper->sep_val, ivp->val)
         ;  }
            if ((modes & MCLX_DUMP_LINES) || vec->n_ivps) /* sth was printed */
            fputc('\n', xf_dump->fp)
      ;  }
      }
      return STATUS_OK
;  }



mclpAR* mclpTFparse
(  mcxLink*    encoding_link
,  mcxTing*    encoding_ting
)
   {  mcxLink* lk = encoding_link
   ;  mclpAR* par = mclpARensure(NULL, 10)
   ;  int n_args
   ;  const char* me = "mclpTFparse"

   ;  if
      (  !lk
      &&!(  lk
         =  mcxTokArgs
            (  encoding_ting->str
            ,  encoding_ting->len
            ,  &n_args
            ,  MCX_TOK_DEL_WS
      )  )  )
      return NULL

   ;  if (!par)
      return NULL

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
         ;  char* onw = NULL
         ;  double d = strtod(val, &onw)
         ;  int mode = -1
         ;  mcxbool nought = FALSE

         ;  if (!val || !strlen(val))
            nought = TRUE
         ;  else if (val == onw)
            {  mcxErr(me, "failed to parse number <%s>", val)
            ;  break
         ;  }

            if (!strcmp(key, "gq"))
            mode = MCLX_UNARY_GQ
         ;  else if (!strcmp(key, "gt"))
            mode = MCLX_UNARY_GT
         ;  else if (!strcmp(key, "lt"))
            mode = MCLX_UNARY_LT
         ;  else if (!strcmp(key, "lq"))
            mode = MCLX_UNARY_LQ
         ;  else if (!strcmp(key, "rand"))
            mode = MCLX_UNARY_RAND
         ;  else if (!strcmp(key, "mul"))
            mode = MCLX_UNARY_MUL
         ;  else if (!strcmp(key, "scale"))
            mode = MCLX_UNARY_SCALE
         ;  else if (!strcmp(key, "add"))
            mode = MCLX_UNARY_ADD
         ;  else if (!strcmp(key, "ceil"))
            mode = MCLX_UNARY_CEIL
         ;  else if (!strcmp(key, "floor"))
            mode = MCLX_UNARY_FLOOR
         ;  else if (!strcmp(key, "pow"))
            mode = MCLX_UNARY_POW
         ;  else if (!strcmp(key, "exp"))
            mode = MCLX_UNARY_EXP
         ;  else if (!strcmp(key, "log"))
            mode = MCLX_UNARY_LOG
         ;  else if (!strcmp(key, "neglog"))
            mode = MCLX_UNARY_NEGLOG

         ;  if (mode < 0)
            {  mcxErr(me, "unknown value transform <%s>", key)
            ;  break
         ;  }

            if (nought)
            {  if (mode == MCLX_UNARY_LOG || mode == MCLX_UNARY_NEGLOG)
               d = 0.0
            ;  else if (mode == MCLX_UNARY_EXP)
               d = 0.0
            ;  else
               {  mcxErr(me, "transform <%s> needs value", key)
               ;  break
            ;  }
         ;  }
            mclpARextend(par, mode, d)
      ;  }
         else
         break
   ;  }

      if (lk)
      mclpARfree(&par)

   ;  return par
;  }


