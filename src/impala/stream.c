
/* TODO NOW
 *
 *    with use-tab, what should the result domain be? extended tab presumably.
 *       only applies to abc data.
 *    with 123 data, one might want to set domains a priori.
 *       danger of interface spaghetti?
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

#include "stream.h"
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

const char* strin = "mclxIOstreamIn";

static mcxstatus write_u32_be
(  unsigned long l
,  FILE* fp
)
   {  int i = 0
   ;  u8 bytes[4]

   ;  for (i=0;i<4;i++)
      {  bytes[3-i] = l & 0377   /* least significant last */
      ;  l >>= 8
   ;  }

      fwrite(&bytes, sizeof bytes[0], 4, fp)
   ;  return STATUS_OK
;  }


static mcxstatus read_u32_be
(  unsigned long *lp
,  FILE* fp
)
   {  int i = 0
   ;  u8 bytes[4]
   ;  unsigned long idx = 0

   ;  *lp = 0
   ;
      {  if (fread(bytes, sizeof bytes[0], 4, fp) != 4)
         return STATUS_FAIL
      ;  for (i=0;i<4;i++)
         idx += bytes[3-i] << (8*i) 
   ;  }

      *lp = idx
   ;  return STATUS_OK
;  }


#define SENTINEL_PACKED (2+4+32+128)

static mcxstatus read_packed
(  FILE* fp
,  unsigned long* s
,  unsigned long* x
,  unsigned long* y
,  double* value
)
   {  if
      (  read_u32_be(s, fp)
      || read_u32_be(x, fp)
      || read_u32_be(y, fp)
      )
      return STATUS_DONE      /* fixme, more stringent EOF check */

;if(0)fprintf(stderr, "read %lu %lu %lu %.3f\n", *s, *x, *y, (double) *value)

   ;  if (*s >> 24 != SENTINEL_PACKED)
      {  mcxErr(strin, "no sentinel!")
      ;  return STATUS_FAIL
   ;  }

      if (1 != fread(value, sizeof value, 1, fp))
      return STATUS_FAIL

   ;  return STATUS_OK
;  }


/* keypp; if we free it caller can still free it as well
 * (because *keypp is set to NULL).
*/

mcxstatus handle_label
(  mcxTing** keypp
,  mcxHash* map
,  unsigned long* z
,  long* z_max_seen        /* ummyes, fix|document (signedness) */
,  mcxbits bits
,  const char* mode
)
   {  mcxbool strict =  bits & MCLXIO_STREAM_TAB_STRICT
   ;  mcxbool warn   =  bits & MCLXIO_STREAM_WARN
   ;  mcxbool ro = bits & (MCLXIO_STREAM_TAB_STRICT | MCLXIO_STREAM_TAB_RESTRICT)
   ;  mcxbool debug  = bits & MCLXIO_STREAM_DEBUG

   ;  mcxstatus status = STATUS_OK
   ;  mcxKV* kv = mcxHashSearch(*keypp, map, ro ? MCX_DATUM_FIND : MCX_DATUM_INSERT)

   ;  if (!kv)      /* ro and not found */
      {  if (strict)
         {  mcxErr(strin, "label <%s> not found (%s strict)", (*keypp)->str, mode)
         ;  status = STATUS_FAIL
      ;  }
         else if (warn)  /* restrict */
         mcxTell(strin, "label <%s> not found (%s restrict)", (*keypp)->str, mode)
   ;  }
      else if (kv->key != *keypp)         /* seen */
      {  mcxTingFree(keypp)
      ;  *z = VOID_TO_UINT kv->val     /* fixme theoretical signedness issue */
   ;  }
      else                             /* INSERTed */
      {  if (debug)
         mcxTell
         (  strin
         ,  "label %s not found (%s extend %lu)"
         ,  (*keypp)->str
         ,  mode
         ,  (*z_max_seen + 1)
         )
      ;  (*z_max_seen)++
      ;  kv->val = UINT_TO_VOID *z_max_seen
      ;  *z = *z_max_seen              /* fixme theoretical signedness issue */
   ;  }
      return status
;  }



/*
 *    possibly implement ':value'
*/

static mcxstatus read_etc
(  mcxIO* xf
,  mcxHash* mapc
,  mcxHash* mapr           /* note: can be same as mapc */
,  unsigned long* x
,  unsigned long* y        /* note: can be same as x  */
,  long* x_max_seen
,  long* y_max_seen
,  double* value
,  mcxbits bits
)
   {  static int etcbuf_check =  0
   ;  static int etcbuf_ofs   =  0
   ;  static long x_prev      =  ULONG_MAX
   ;  static mcxTing* etcbuf  =  NULL
   ;  mcxbool ignore = TRUE
   ;  mcxstatus status = STATUS_OK
   ;  mcxTing* ykey = NULL
   ;  mcxTing* xkey = NULL
   ;  const char* printable

   ;  mcxbool label_cbits = bits & (MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_CTAB_RESTRICT)
   ;  mcxbool label_rbits = bits & (MCLXIO_STREAM_RTAB_STRICT | MCLXIO_STREAM_RTAB_RESTRICT)
   ;  mcxbool label_dbits = bits & (MCLXIO_STREAM_WARN | MCLXIO_STREAM_DEBUG)

   ;  *value = 1

   ;  if (!etcbuf)
      etcbuf = mcxTingEmpty(NULL, 100)

   ;  while (1)
      {  if (etcbuf->len != etcbuf_check)
         {  mcxErr(strin, "read_etc sanity check failed %d %d", etcbuf->len, etcbuf_check)
         ;  status = STATUS_FAIL
         ;  break
      ;  }
                                             /* do we need to read a line ? */
         if (etcbuf_ofs == etcbuf->len)      /* works first time around too */
         {  etcbuf_ofs = 0
         ;  if ((status = mcxIOreadLine(xf, etcbuf, MCX_READLINE_CHOMP)))
            break
         ;  etcbuf_check = etcbuf->len

         ;  if
            (  !(printable = mcxStrChrAint(etcbuf->str, isspace, -1))
            || (unsigned char) *printable == '#'
            )
            break

         ;  xkey = mcxTingEmpty(NULL, etcbuf->len)
         ;  if (bits & MCLXIO_STREAM_ETC_AI)
               *x = x_prev + 1         /* works first time around */
            ,  *x_max_seen = x_prev + 1
         ;  else
            {  int n_read = 0
            ;  if (1 != sscanf(etcbuf->str, "%s%n", xkey->str, &n_read))
               break
            ;  etcbuf_ofs += n_read
            ;  if (!mcxStrChrAint(etcbuf->str+etcbuf_ofs, isspace, -1))
               break
            ;  xkey->len = strlen(xkey->str)
            ;  xkey->str[xkey->len] = '\0'
;if (0) fprintf(stderr, "found col %s\n", xkey->str)
            ;  if
               (  (  status
                  =  handle_label
                     (&xkey, mapc, x, x_max_seen, label_cbits | label_dbits, "col")
               )  )
               break
         ;  }
         }
         else
         *x = x_prev

      ;  if
         (  !(printable = mcxStrChrAint(etcbuf->str+etcbuf_ofs, isspace, -1))
         || (unsigned char) *printable == '#'
         )
         break
      ;  else
         {  int n_read = 0
         ;  ykey = mcxTingEmpty(NULL, etcbuf->len)
         ;  if (1 != sscanf(etcbuf->str+etcbuf_ofs, "%s%n", ykey->str, &n_read))
            break
         ;  ykey->len = strlen(ykey->str)
         ;  ykey->str[ykey->len] = '\0'
         ;  etcbuf_ofs += n_read
;if (0) fprintf(stderr, "found row %s\n", ykey->str)
         ;  if
            (  (  status
               =  handle_label
                  (&ykey, mapr, y, y_max_seen, label_rbits | label_dbits, "row")
            )  )
            break
         ;  if (!mcxStrChrAint(etcbuf->str+etcbuf_ofs, isspace, -1))
            break
      ;  }

         status = STATUS_OK
      ;  ignore = FALSE
      ;  x_prev = *x
      ;  break
   ;  }
   
      if (!status && ignore)
      etcbuf_ofs = etcbuf->len

   ;  if (status)
      {  mcxTingFree(&xkey)      /* fixmefixmefixme xkey might be in use in hash */
      ;  mcxTingFree(&ykey)
   ;  }
      return status
;  }



#define MCLXIO_STREAM_CTAB ((MCLXIO_STREAM_CTAB_EXTEND | MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_CTAB_RESTRICT))
#define MCLXIO_STREAM_RTAB ((MCLXIO_STREAM_RTAB_EXTEND | MCLXIO_STREAM_RTAB_STRICT | MCLXIO_STREAM_RTAB_RESTRICT))

static mcxstatus read_abc
(  mcxIO* xf
,  mcxTing* buf         /* perhaps better make this static */
,  mcxHash* mapc
,  mcxHash* mapr           /* note: can be same as mapc */
,  unsigned long* x
,  unsigned long* y        /* note: can be same as x  */
,  long* x_max_seen
,  long* y_max_seen
,  double* value
,  mcxbits bits
)
   {  mcxstatus status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)
   ;  mcxTing* xkey = mcxTingEmpty(NULL, buf->len)
   ;  mcxTing* ykey = mcxTingEmpty(NULL, buf->len)
   ;  const char* printable
   ;  int cv = 0
   ;  mcxbool strict = bits & MCLXIO_STREAM_STRICT
   ;  mcxbool warn   = bits & MCLXIO_STREAM_WARN

   ;  mcxbool label_cbits = bits & (MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_CTAB_RESTRICT)
   ;  mcxbool label_rbits = bits & (MCLXIO_STREAM_RTAB_STRICT | MCLXIO_STREAM_RTAB_RESTRICT)
   ;  mcxbool label_dbits = bits & (MCLXIO_STREAM_WARN | MCLXIO_STREAM_DEBUG)

   ;  while (1)
      {  int xlen = 0
      ;  int ylen = 0

      ;  if (status)
         break

      ;  printable = mcxStrChrAint(buf->str, isspace, buf->len)
      ;  if (printable && (unsigned char) printable[0] == '#')
         {  status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)
         ;  continue
      ;  }

         mcxTingEnsure(xkey, buf->len)    /* fixme, bit wasteful */
      ;  mcxTingEnsure(ykey, buf->len)    /* fixme, bit wasteful */

      ;  cv = sscanf(buf->str, "%s%s%lf", xkey->str, ykey->str, value)

      /* WARNING: [xy]key->len have to be set.
       * we first check sscanf return value
      */

      ;  if (cv == 2)
         *value = 1.0

      ;  else if (cv != 3)
         {  if (warn || strict)
            mcxErr
            (  "mcxIOstreamIn"
            ,  "abc-parser chokes at line %ld [%s]"
            ,  (long) xf->lc
            ,  buf->str
            )
         ;  if (strict)
            {  status = STATUS_FAIL
            ;  break
         ;  }
            status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)
         ;  continue
      ;  }
         else if (!(*value <= FLT_MAX))  /* should catch nan, inf */
         *value = 1.0

      ;  xlen = strlen(xkey->str)
      ;  ylen = strlen(ykey->str)

      ;  xkey->len = xlen
      ;  ykey->len = ylen

      ;  if (handle_label(&xkey, mapc, x, x_max_seen, label_cbits | label_dbits, "col"))
         {  status = STATUS_FAIL
         ;  break
      ;  }
         if (handle_label(&ykey, mapr, y, y_max_seen, label_rbits | label_dbits, "row"))
         {  status = STATUS_FAIL
         ;  break
      ;  }
         break
   ;  }
               /* NOTE handle_label might have set either to NULL but
                * that's OK.  This is needed because handle_label(&xkey)
                * might succeed and free xkey (because already present in
                * mapc); then when handle_label(&key) fails we need to
                * clean up.
                *
                * FIXMEfixmefixme if handle label inserted xkey.
                * it should not be freed.
               */
      if (status)
      {  mcxTingFree(&xkey)
      ;  mcxTingFree(&ykey)
   ;  }

      return status
;  }



static mcxstatus read_123
(  mcxIO* xf
,  mcxTing* buf         /* perhaps better make this static */
,  unsigned long* x
,  unsigned long* y
,  double* value
,  mcxbits bits
)
   {  mcxstatus status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)
   ;  int cv = 0
   ;  const char* printable
   ;  const char* me = strin
   ;  mcxbool strict = bits & MCLXIO_STREAM_STRICT
   ;  mcxbool warn   = bits & MCLXIO_STREAM_WARN

   ;  while (1)
      {  if (status)
         break

      ;  status = STATUS_FAIL

      ;  printable = mcxStrChrAint(buf->str, isspace, buf->len)
      ;  if (printable && (unsigned char) printable[0] == '#')
         {  status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)
         ;  continue
      ;  }

         cv = sscanf(buf->str, "%lu%lu%lf", x, y, value)

      ;  if (*x > LONG_MAX || *y > LONG_MAX)
         {  mcxErr
            (me, "negative values in input stream? unsigned %lu %lu", *x, *y)
         ;  break
      ;  }

         if (cv == 2)
         *value = 1.0

      ;  else if (cv != 3)
         {  if (strict || warn)
            mcxErr
            (  "mcxIOstreamIn"
            ,  "123-parser chokes at line %ld [%s]"
            ,  (long) xf->lc
            ,  buf->str
            )
         ;  if (strict)
            break

         ;  status = mcxIOreadLine(xf, buf, MCX_READLINE_CHOMP)
         ;  continue
      ;  }
         else if (!(*value < FLT_MAX))
         *value = 1.0

      ;  status = STATUS_OK
      ;  break
   ;  }

      return status
;  }


void* my_par_init_v
(  void* arp_v
)
   {  mclpAR* ar = arp_v
   ;  mclpARinit(ar)
   ;  mclpARensure(ar, 10)
   ;  return ar
;  }


#define use_tab_bits \
(  MCLXIO_STREAM_CTAB_EXTEND | MCLXIO_STREAM_RTAB_EXTEND \
|  MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_RTAB_STRICT \
|  MCLXIO_STREAM_CTAB_RESTRICT | MCLXIO_STREAM_RTAB_RESTRICT  )

mcxbits mclxio_stream_usebits
(  mcxbool symmetric
,  mcxbits bits
)
   {  int n = 0
   ;  mcxbits newbits = 0
   ;  if (symmetric && (bits & use_tab_bits))
      {  if (bits & (MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_RTAB_STRICT))
         {  newbits = MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_RTAB_STRICT
         ;  n++
      ;  }
         if (bits & (MCLXIO_STREAM_CTAB_RESTRICT | MCLXIO_STREAM_RTAB_RESTRICT))
         {  newbits = MCLXIO_STREAM_CTAB_RESTRICT | MCLXIO_STREAM_RTAB_RESTRICT
         ;  n++
      ;  }
         if (bits & (MCLXIO_STREAM_CTAB_EXTEND | MCLXIO_STREAM_RTAB_EXTEND))
         {  newbits = MCLXIO_STREAM_CTAB_EXTEND | MCLXIO_STREAM_RTAB_EXTEND
         ;  n++
      ;  }
         if (n > 1)
         mcxErr(strin, "conflicting tab directives found (taking most lax)")
      ;  else if (!n)
            mcxErr(strin, "no tab directives found (entering strict mode)")
         ,  newbits = MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_RTAB_STRICT
   ;  }
      else
      newbits = bits & use_tab_bits

   ;  BIT_OFF(bits, use_tab_bits)
   ;  BIT_ON(bits, newbits)
   ;  return bits
;  }



/* Needs documentation and splitting up.
 * it does a lot in different modes.
 * Its length is *very* depressing - although largely caused by spacing.
*/


mclx* mclxIOstreamIn
(  mcxIO*   xf
,  mcxbits  bits
,  mclpAR*  transform
,  void    (*ivpmerge)(void* ivp1, const void* ivp2)
,  mclTab** tabcpp
,  mclTab** tabrpp
,  mcxstatus ON_FAIL
)
   {  long c_max_alloc  =  10
   ;  mcxstatus status  =  STATUS_FAIL
   ;  const char* me    =  strin
   ;  mcxbool mirror    =  bits & MCLXIO_STREAM_MIRROR
   ;  mcxbool packed    =  bits & MCLXIO_STREAM_PACKED ? TRUE : FALSE
   ;  mcxbool one23     =  bits & MCLXIO_STREAM_123    ? TRUE : FALSE
   ;  mcxbool abc       =  bits & MCLXIO_STREAM_ABC    ? TRUE : FALSE
   ;  mcxbool etc       =  bits & MCLXIO_STREAM_ETC    ? TRUE : FALSE
   ;  mcxbool etcai     =  bits & MCLXIO_STREAM_ETC_AI ? TRUE : FALSE
   ;  mcxbool symmetric =  !(bits & MCLXIO_STREAM_TWODOMAINS) || mirror
   ;  mcxTing* buf      =  mcxTingEmpty(NULL, 100)

   ;  mcxHash* mapc     =  NULL
   ;  mcxHash* mapr     =  NULL
   ;  mclpAR* pars      =  NULL
   ;  mclTab* tabc      =  NULL
   ;  mclTab* tabr      =  NULL

   ;  long c_max_seen = -1, r_max_seen = -1, c = 0

   ;  long* c_max_seenp = &c_max_seen
   ;  long* r_max_seenp = symmetric ? &c_max_seen : &r_max_seen

   ;  unsigned long n_ite = 0
   ;  mclx* mx = NULL

   ;  if (!ivpmerge)
      ivpmerge = mclpMergeMax

   ;  bits = mclxio_stream_usebits(symmetric, bits)

         /* pars can now be alloced short of c_max_seen in abc mode */

   ;  while (1)
      {  if (abc + one23 + packed + etc > TRUE)   /* OUCH */
         {  mcxErr(strin, "multiple stream formats specified")
         ;  break
      ;  }
      
         if ((!packed && !one23 && !abc && !etc) || (abc && !(tabcpp || tabrpp)))
         {  mcxErr(strin, "not enough to get going")
         ;  break
      ;  }

         if (abc || etc)
         {  if (tabcpp && *tabcpp != NULL)
            {  tabc = *tabcpp
            ;  if (!mcldIsCanonical(tabc->domain))
               {  mcxErr(me, "only canonical tabc supported")
               ;  break
            ;  }
               mapc = mclTabHash(tabc)

            ;  if (!(bits & MCLXIO_STREAM_CTAB))
                  mcxErr
                  (  strin
                  ,  "PBD suggest explicit ctab mode (now extending)"
                  )
               ,  bits |= MCLXIO_STREAM_CTAB_EXTEND
            ;  c_max_seen = tabc->domain->n_ivps - 1
         ;  }
            else
            mapc = mcxHashNew(1024, mcxTingDPhash, mcxTingCmp)

         ;  if (!symmetric)
            {  if (tabrpp && *tabrpp != NULL)
               {  tabr = *tabrpp
               ;  if (!mcldIsCanonical(tabr->domain))
                  {  mcxErr(me, "only canonical tabr supported (ignoring)")
                  ;  break
               ;  }
                  else
                  mapr = mclTabHash(tabr)

               ;  if (!(bits & MCLXIO_STREAM_RTAB))
                     mcxErr
                     (  strin
                     ,  "PBD suggest explicit rtab mode (now extending)"
                     )
                  ,  bits |= MCLXIO_STREAM_RTAB_EXTEND
               ;  r_max_seen = tabr->domain->n_ivps - 1
            ;  }
               else
               mapr = mcxHashNew(1024, mcxTingDPhash, mcxTingCmp)
         ;  }
            else
            mapr = mapc
      ;  }

         pars
         =  mcxNAlloc
            (  MAX(c_max_alloc, (c_max_seen+1))
            ,  sizeof pars[0]
            ,  my_par_init_v
            ,  RETURN_ON_FAIL
            )
      ;  if (!pars)
         break

      ;  if (xf->fp == NULL && (mcxIOopen(xf, ON_FAIL) != STATUS_OK))
         {  mcxErr(me, "cannot open stream <%s>", xf->fn->str)
         ;  break
      ;  }
         status = STATUS_OK
      ;  break
   ;  }

      if (!status)
      while (1)
      {  unsigned long s = 0, x = 0, y = 0
      ;  double value = 0
      ;  n_ite++

      ;  if (n_ite % 100000 == 0)
         fputc('.', stderr)
      ;  if (n_ite % 5000000 == 0)
         fprintf(stderr, " %ldM\n", (long) (n_ite / 1000000))

      ;  status
         =     packed
            ?     read_packed(xf->fp, &s, &x, &y, &value)
            :  one23
            ?     read_123(xf, buf, &x, &y, &value, bits)
            :  etc
            ?     read_etc
                  (  xf
                  ,  mapc,mapr, &x, &y, c_max_seenp, r_max_seenp
                  ,  &value, bits
                  )
            :  abc
            ?     read_abc
                  (  xf, buf
                  ,  mapc,mapr, &x, &y, c_max_seenp, r_max_seenp
                  ,  &value, bits
                  )
            :  STATUS_FAIL

      ;  if (status == STATUS_IGNORE)     /* maybe restrict mode */
         continue
      ;  else if (status)                 /* FAIL or DONE */
         break

                              /* needed for 123 and packed case */
      ;  if ((x+1) > (c_max_seen+1))   /* because of signedness */
         c_max_seen = x
      ;  if ((y+1) > (r_max_seen+1))   /* because of signedness */
         r_max_seen = y

      ;  status = STATUS_FAIL

      ;  if
         (  x >= c_max_alloc
         || (symmetric && y >= c_max_alloc)
         )
         {  long c_old_alloc  =  c_max_alloc
         ;  long z            =  symmetric ? MAX(x, y) : x
         ;  c_max_alloc       =  MAX(z+1000, c_old_alloc* 1.44)
         ;  if
            ( !(  pars
               =  mcxNRealloc
                  (  pars
                  ,  c_max_alloc
                  ,  c_old_alloc
                  ,  sizeof pars[0]
                  ,  my_par_init_v
                  ,  RETURN_ON_FAIL
                  )
            ) )
            {  mcxErr(strin, "realloc failed")
            ;  break
         ;  }
         }

         if (bits & MCLXIO_STREAM_LOGTRANSFORM)
         value = log(value)
      ;  else if (bits & MCLXIO_STREAM_NEGLOGTRANSFORM)
         value = -log(value)

      ;  if (mclpARextend(pars+x, y, value))
         {  mcxErr(me, "x-extend fails")
         ;  break
      ;  }
         if (transform)
         {  mclp* ivpx = pars[x].ivps+pars[x].n_ivps-1
         ;  ivpx->val = mclpUnary(ivpx, transform)
      ;  }

         if (mirror)
         {  if (mclpARextend(pars+y, x, value))
            {  mcxErr(me, "y-extend fails")
            ;  break
         ;  }
            if (transform)
            {  mclp* ivpy = pars[y].ivps+pars[y].n_ivps-1
            ;  ivpy->val = mclpUnary(ivpy, transform)
         ;  }
         }

         status = STATUS_OK
   ;  }

      if (n_ite >= 1000000 && n_ite % 5000000)
      fputc('\n', stderr)

   ;  if ((status != STATUS_FAIL) && !ferror(xf->fp))
      while (1)
      {  long dc_max_seen = symmetric ? MAX(c_max_seen, r_max_seen) : c_max_seen
      ;  long dr_max_seen = symmetric ? MAX(c_max_seen, r_max_seen) : r_max_seen

      ;  status = STATUS_FAIL

      ;  if (c_max_seen < 0 || (!symmetric && r_max_seen < 0))
         {  if (c_max_seen < -1 || r_max_seen < -1)
            {  mcxErr(me, "bad apples %ld %ld", c_max_seen, r_max_seen)
            ;  break
         ;  }
            else
            mcxTell(me, "no assignments yield void/empty matrix")
      ;  }

         if (! (  mx =  mclxAllocZero
                  (  mclvCanonical(NULL, dc_max_seen+1, 1.0)
                  ,  mclvCanonical(NULL, dr_max_seen+1, 1.0)
            )  )  )
         break

      ;  for (c=0;c<=dc_max_seen;c++)
         {  mclvFromIvps_x
            (  mx->cols+c
            ,  pars[c].ivps
            ,  pars[c].n_ivps
            ,  pars[c].sorted
            ,  0
            ,  ivpmerge
            ,  NULL
            )
;if (0) fprintf(stderr, "sortbits %lu\n", pars[c].sorted)
         ;  mcxFree(pars[c].ivps)     /* dangerous, s.e's property */
      ;  }

         for (c=dc_max_seen+1;c<c_max_alloc;c++)
         mcxFree(pars[c].ivps)
      ;  mcxFree(pars)

      ;  status = STATUS_OK
      ;  break
   ;  }
      else if (status == STATUS_FAIL || ferror(xf->fp))
      mcxErr
      (me, "error occurred (status %d lc %d)", (int) status, (int) xf->lc)

   ;  mcxTingFree(&buf)

   ;  if (status == STATUS_FAIL)       /* STATUS_DONE otherwise */
      {  mclxFree(&mx)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxDie(1, me, "fini")
   ;  }

   /* fixmefixmefixme mem leak with hashes */

                        /* with etcai there is simply no column tab*/
                        /* perhaps create a dummy one (integers* */
      else if (abc || etc)
      {  if (!etcai)
         {  if (!tabc || tabc->domain->n_ivps <= c_max_seen)
            {  tabc = mclTabFromMap(mapc)
            ;  *tabcpp = tabc
            ;  mcxHashFree(&mapc, mcxTingAbandon, NULL)
         ;  }
         }
         else
         mcxHashFree(&mapc, mcxTingFree_v, NULL)

      ;  if (!symmetric)
         {  if (!tabr || tabr->domain->n_ivps <= r_max_seen)
            {  tabr = mclTabFromMap(mapr)
            ;  *tabrpp = tabr
            ;  mcxHashFree(&mapr, mcxTingAbandon, NULL)
         ;  }
            else
            mcxHashFree(&mapr, mcxTingFree_v, NULL)
      ;  }
      }

   ;  return mx
;  }



/* fixme wrongly named
*/

mcxstatus mclxIOstreamOut
(  const mclx* mx
,  mcxIO* xf
,  mcxstatus ON_FAIL
)
   {  unsigned long sentinel = SENTINEL_PACKED << 24
   ;  long c

   ;  if (xf->fp == NULL && (mcxIOopen(xf, ON_FAIL) != STATUS_OK))
      {  mcxErr("mclxIOstreamOut", "cannot open stream <%s>", xf->fn->str)
      ;  return STATUS_FAIL
   ;  }

      for (c=0;c<N_COLS(mx);c++)
      {  mclv* col   =  mx->cols+c
      ;  long  cidx  =  col->vid
      ;  long j
      ;  for (j=0;j<col->n_ivps;j++)
         {  int ridx    =  col->ivps[j].idx
         ;  float val   =  col->ivps[j].val
         ;  write_u32_be(sentinel, xf->fp)
         ;  write_u32_be(cidx, xf->fp)
         ;  write_u32_be(ridx, xf->fp)
         ;  fwrite(&val, sizeof val, 1, xf->fp)
      ;  }
      }

   ;  return STATUS_OK
;  }



