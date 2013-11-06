/* (c) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>


#include "io.h"
#include "types.h"
#include "ting.h"
#include "ding.h"
#include "err.h"
#include "alloc.h"
#include "getpagesize.h"


static void mcxIOreset
(  mcxIO*    xf
)  ;


int begets_stdio
(  const char* name
,  const char* mode
)
   {  if
      (  (  strstr(mode, "r")
         && !strcmp(name, "-")
         )
      || (  (strstr(mode, "w") || strstr(mode, "a"))
         && !strcmp(name, "-")
         )
      || (!strcmp(name, "stderr"))
      )
      return 1
   ;  return 0
;  }


static int mcxIOwarnOpenfp
(  mcxIO*    xf
,  const char* who
)
   {  if (xf->fp && !xf->stdio)
      {  mcxIOerr(xf, who, "has open file pointer")
      ;  return 1
   ;  }
      return 0
;  }


void mcxIOclose
(  mcxIO*    xf
)
   {  fflush(xf->fp)
   ;  if (xf->fp && !xf->stdio)
      {  fclose(xf->fp)
      ;  xf->fp = NULL
   ;  }
      else if (xf->stdio)
      {  int fe = ferror(xf->fp)
      ;  if (fe)
         mcxErr("mcxIOclose", "error [%d] for [%s] stdio", fe, xf->mode)
      ;  if (xf->ateof || feof(xf->fp))
         clearerr(xf->fp)
   ;  }
      mcxIOreset(xf)
;  }


/* note: does not touch all members, notably
 *    usr
 *    fn
 *    mode
 *    fp
*/

static void mcxIOreset
(  mcxIO*    xf
)
   {  xf->lc      =  0
   ;  xf->lo      =  0
   ;  xf->lo_     =  0
   ;  xf->bc      =  0
   ;  xf->ateof   =  0
                     /*  xf->fp not touched; promote user care */
                     /*  xf->stdio not touched */
   ;  if (xf->usr && xf->usr_reset)
      xf->usr_reset(xf->usr)
;  }


void mcxIOerr
(  mcxIO*   xf
,  const char     *complainer
,  const char     *complaint
)
   {  if (!xf)
      return

   ;  mcxErr
      (  complainer
      ,  "%s stream <%s> %s"
      ,  xf->mode
      ,  xf->fn->str
      ,  complaint
      )  
;  }


mcxIO* mcxIOnew
(  const char*       str
,  const char*       mode
)
   {  if (!str || !mode)
      {  mcxErr("mcxIOnew PBD", "void string or mode argument")
      ;  return NULL
   ;  }

      return mcxIOrenew(NULL, str, mode)
;  }


/* fixme: the code below is not so clear.
*/

mcxIO* mcxIOrenew
(  mcxIO*            xf
,  const char*       name
,  const char*       mode
)
   {  mcxbool twas_stdio = xf && xf->stdio
   ;  if
      (  mode
      && !strstr(mode, "w") && !strstr(mode, "r") && !strstr(mode, "a")
      )
      {  mcxErr ("mcxIOrenew PBD", "unsupported open mode <%s>", mode)
      ;  return NULL
   ;  }
      if (!xf)
      {  if (!name || !mode)
         {  mcxErr ("mcxIOrenew PBD", "too few arguments")
         ;  return NULL
      ;  }

         if (!(xf = (mcxIO*) mcxAlloc(sizeof(mcxIO), RETURN_ON_FAIL)))
         return NULL

      ;  if (!(xf->fn = mcxTingEmpty(NULL, 20)))
         {  mcxFree(xf)
         ;  return NULL
      ;  }

         xf->usr  =  NULL
      ;  xf->usr_reset =  NULL
      ;  xf->fp   =  NULL
      ;  xf->mode =  NULL
   ;  }
      else if (xf->stdio)
   ;  else if (mcxIOwarnOpenfp(xf, "mcxIOrenew"))
      mcxIOclose(xf)

   ;  mcxIOreset(xf)

   ;  if (name && !mcxTingWrite(xf->fn, name))
      return NULL    /* fixme; shd clean up */

   ;  if (mode)
      {  if (xf->mode)
         mcxFree(xf->mode)
      ;  xf->mode = mcxStrDup(mode)
   ;  }

      xf->stdio = begets_stdio(xf->fn->str, xf->mode)

   ;  if (twas_stdio && !xf->stdio)
      xf->fp = NULL

   ;  return xf
;  }


mcxstatus  mcxIOopen
(  mcxIO*   xf
,  mcxOnFail      ON_FAIL
)
   {  const char* treat    =  ""
   ;  const char* fname    =  xf->fn->str
   ;  if (!xf)
      {  mcxErr("mcxIOnew PBD", "received void object")
      ;  if (ON_FAIL == RETURN_ON_FAIL)
         return STATUS_FAIL
      ;  exit(1)
   ;  }

      if (mcxIOwarnOpenfp(xf, "mcxIOopen PBD"))
      return STATUS_OK

   ;  if (strstr(xf->mode, "r"))
      treat   =  "reading"
   ;  else if (strstr(xf->mode, "w"))
      treat   =  "writing"
   ;  else if (strstr(xf->mode, "a"))
      treat   =  "appending"

   ;  if
      (  strstr(xf->mode, "r")
      && !strcmp(fname, "-")
      )
      xf->fp      =  stdin

   ;  else if
      (  (strstr(xf->mode, "w") || strstr(xf->mode, "a"))
      && !strcmp(fname, "-")
      )
      xf->fp      =  stdout

   ;  else if (!strcmp(fname, "stderr"))
      xf->fp      =  stderr

   ;  else if ((xf->fp = fopen(fname, xf->mode)) == NULL)
      {  if (ON_FAIL == RETURN_ON_FAIL)
         return STATUS_FAIL
      ;  mcxIOerr(xf, "mcxIOopen", "can not be opened")
      ;  mcxExit(1)
   ;  }
      return STATUS_OK
;  }


mcxstatus  mcxIOtestOpen
(  mcxIO*         xf
,  mcxOnFail      ON_FAIL
)
   {  if (!xf->fp && mcxIOopen(xf, ON_FAIL) != STATUS_OK)
      {  mcxErr
         ("mcxIO", "cannot open %s file <%s> for", xf->mode, xf->fn->str)
      ;  return STATUS_FAIL
   ;  }
      return  STATUS_OK
;  }



void mcxIOrelease
(  mcxIO*  xf
)
   {  if (xf)
      {  mcxIOclose(xf)

      ;  if (xf->fn)
         mcxTingFree(&(xf->fn))
      ;  if (xf->mode)
         mcxFree(xf->mode)
   ;  }
   }


mcxstatus mcxIOappendName
(  mcxIO*         xf
,  const char*    suffix
)
   {  if (xf->fp)
      mcxErr
      (  "mcxIOappendName PBD"
      ,  "stream open while request for name change from <%s> to <%s>"
      ,  xf->fn->str
      ,  suffix
      )
   ;  else if (!mcxTingAppend(xf->fn, suffix))
      return STATUS_FAIL

   ;  return STATUS_OK
;  }


mcxstatus mcxIOnewName
(  mcxIO*         xf
,  const char*    newname
)
   {  if (!mcxTingEmpty(xf->fn, 0))
      return STATUS_FAIL

   ;  return mcxIOappendName(xf, newname)
;  }


int mcxIOstepback
(  int c
,  mcxIO*    xf
)
   {  if (c == EOF)
      return EOF
   ;  else
      {  if (ungetc(c, xf->fp) == EOF)
         {  mcxErr
            (  "mcxIOstepback"
            ,  "failed to push back <%d> on stream <%s>\n"
            ,  c
            ,  xf->fn->str
            )
         ;  return EOF
      ;  }

         xf->bc--
      ;  if (c == '\n')
            xf->lc--
         ,  xf->lo = xf->lo_
         ,  xf->lo_ = 0
      ;  else
         xf->lo--
   ;  }
      return c
;  }


int mcxIOstep
(  mcxIO*    xf
)
   {  int c = xf->ateof ? EOF : fgetc(xf->fp)
   ;  switch(c)
      {
      case '\n'
      :  xf->lc++
      ;  xf->bc++
      ;  xf->lo_  =  xf->lo
      ;  xf->lo   =  0
      ;  break
      ;

      case EOF
      :  xf->ateof =  1
      ;  break
      ;

      default
      :  xf->bc++
      ;  xf->lo++
      ;  break
   ;  }
      return c
;  }


/* fixme todo
 *
 * look at bc; substract it from sz (we might have read a part already).
 *
 * support growing files. and look at other items in the grep source code.
*/

mcxstatus  mcxIOreadFile
(  mcxIO    *xf
,  mcxTing   *filetxt
)
   {  struct stat mystat
   ;  size_t sz = 4096
   ;  ssize_t r
   ;  mcxTingEmpty(filetxt, 0)

   ;  if (!xf->stdio)
      {  if (stat(xf->fn->str, &mystat))
         mcxIOerr(xf, "mcxIOreadFile", "can not stat file")
      ;  else
         sz = mystat.st_size
   ;  }

      if (!xf->fp && mcxIOopen(xf, RETURN_ON_FAIL))
      {  mcxIOerr(xf, "mcxIOreadFile", "can not open file")
      ;  return STATUS_FAIL
   ;  }

      if (xf->ateof)
      return STATUS_OK
                                          /* fixme; ting count overflow */
   ;  if (!(filetxt = mcxTingEmpty(filetxt, sz)))
      return STATUS_NOMEM

   ;  while ((r = mcxIOappendChunk(xf, filetxt, sz, 0)) > 0 && !xf->ateof)

   ;  if (r <0)
      return STATUS_FAIL                  /* fixme; look closer at error */

   ;  return STATUS_OK
;  }


static int  mcxIO__rl_fillbuf__
(  mcxIO*   xf
,  char*    buf
,  int      size
,  int*     last
)
   {  int   a  = 0
   ;  int   ct = 0
   ;  while(ct<size && EOF != (a = mcxIOstep(xf)))
      {  buf[ct++] = a
      ;  if (a == '\n')
         break
   ;  }
      *last =  a
   ;  return ct
;  }


#define IO_MEM_ERROR (EOF-1)

static int mcxIO__rl_rl__
(  mcxIO    *xf
,  mcxTing  *lineTxt
)
#define MCX_IORL_BSZ 513
   {  char  cbuf[MCX_IORL_BSZ]
   ;  int   z

   ;  if (!mcxTingEmpty(lineTxt, 1))
      return IO_MEM_ERROR
     /* todo/fixme; need to set errno so that caller can know */

   ;  while (1)
      {  int ct = mcxIO__rl_fillbuf__(xf, cbuf, MCX_IORL_BSZ, &z)
      ;  if (ct && !mcxTingNAppend(lineTxt, cbuf, ct))
         return IO_MEM_ERROR
      ;  if (z == '\n' || z == EOF)
         break
   ;  }
      if (z == EOF)
      xf->ateof = 1
   ;  return z
#undef MCX_IORL_BSZ
;  }


int mcxIOdiscardLine
(  mcxIO       *xf
)
   {  int a, ct = 0

   ;  if (!xf->fp)
      {  mcxIOerr(xf, "mcxIOdiscardLine", "is not open")
      ;  return -1
   ;  }

      while(((a = mcxIOstep(xf)) != '\n') && a != EOF)
      ct++

   ;  return ct
;  }



ssize_t mcxIOappendChunk
(  mcxIO        *xf
,  mcxTing      *dst
,  ssize_t      sz
,  mcxbits      flags
)
   {  unsigned long psz =  getpagesize()
   ;  ssize_t k   =  sz / psz       /* fixme: size checks? */
   ;  ssize_t rem =  sz % psz
   ;  ssize_t r   =  1              /* pretend in case k == 0 */
  /*  mcxbool  account = flags & MCX_CHUNK_ACCOUNT ? TRUE : FALSE */
   ;  size_t   offset = dst->len
   ;  char* p

   ;  if (!dst || !xf->fp || !mcxTingEnsure(dst, dst->len + sz))
      return -1
      /* fixme set some (new) errno */

   ;  while (k-- && (r = read(fileno(xf->fp),  dst->str+dst->len, psz)) > 0)
      dst->len += r

   ;  if
      (  r > 0
      && rem > 0
      && (r = read(fileno(xf->fp),  dst->str+dst->len, rem)) > 0
      )
      dst->len += r

   ;  dst->str[dst->len] = '\0'
   ;  xf->bc += dst->len - offset

   ;  for (p = dst->str+offset; p<dst->str+dst->len; p++)
      {  if (*p == '\n')
         {  xf->lc++
         ;  xf->lo_ = xf->lo
         ;  xf->lo = 0
      ;  }
         else
         xf->lo++
   ;  }
                           /* fixme; what if k == 0, rem == 0 ? */
      if (!r)                    /* fixme; other possibilities? */
      xf->ateof = 1

   ;  return  r < 0 ? r : dst->len
;  }


mcxstatus  mcxIOreadLine
(  mcxIO    *xf
,  mcxTing  *dst
,  mcxbits  flags
)
   {  int      a, ll
   ;  mcxbool  chomp    =  flags & MCX_READLINE_CHOMP
   ;  mcxbool  skip     =  flags & MCX_READLINE_SKIP_EMPTY
   ;  mcxbool  par      =  flags & MCX_READLINE_PAR
   ;  mcxbool  dot      =  flags & MCX_READLINE_DOT
   ;  mcxbool  bsc      =  flags & MCX_READLINE_BSC
   ;  mcxbool  repeat   =  dot || par || bsc
   ;  mcxbool  continuation   =  FALSE
   ;  mcxTing* line
   ;  mcxstatus stat    =  STATUS_OK

   ;  if (!xf->fp && mcxIOopen(xf, RETURN_ON_FAIL))
      {  mcxIOerr(xf, "mcxIOreadLine", "is not open")
      ;  return STATUS_FAIL
   ;  }

      if (xf->ateof)
      return STATUS_DONE

   ;  if (!(dst = mcxTingEmpty(dst, 1)))
      return STATUS_NOMEM

   ;  if (skip || par)
      {  while((a = mcxIOstep(xf)) == '\n')
      /* NOTHING */
      ;  if (xf->ateof)
         return STATUS_DONE
      ;  else
         mcxIOstepback(a, xf)
   ;  }

      if (!(line = repeat ? mcxTingEmpty(NULL, 1) : dst))
      return STATUS_NOMEM

   ;  while (1)
      {  if (IO_MEM_ERROR == (a = mcxIO__rl_rl__(xf, line)))
         {  stat = STATUS_NOMEM
         ;  break
      ;  }

         ll = line->len

      ;  if (!repeat)
         break
      ;  else  /* must append line to dst */
         {  if
            (  dot
            && !continuation
            && line->str[0] == '.'
            && (  ll == 2
               || (ll == 3 && line->str[1] == '\r')
               )
            )
            break
                  /* do not attach the single-dot-line */

         ;  if (par && !continuation && ll == 1)
            break
                  /* do not attach the second newline */

         ;  if (!mcxTingNAppend(dst, line->str, line->len))
            {  stat = STATUS_NOMEM
            ;  break
         ;  }

            continuation = bsc && (ll > 1 && *(line->str+ll-2) == '\\')

         ;  if (continuation)
            mcxTingShrink(dst, -2)

         ;  if (!par && !dot && (bsc && !continuation))
            break

         ;  if (xf->ateof)
            break
      ;  }
      }

      if (repeat)
      mcxTingFree(&line)

   ;  if (stat)
      return stat    /* fixme; should we not check chomp first ? */

   ;  if (chomp && dst->len && *(dst->str+dst->len-1) == '\n')
      mcxTingShrink(dst, -1)

   ;  if (xf->ateof && !dst->len)
      return STATUS_DONE

   ;  return STATUS_OK
;  }


void mcxIOlistParmodes
(  void
)
   {  fprintf
      (  stdout
      ,  "%5d  vanilla mode, fetch next line\n"
      ,  MCX_READLINE_DEFAULT
      )
   ;  fprintf(stdout, "%5d  chomp trailing newline\n", MCX_READLINE_CHOMP)
   ;  fprintf(stdout, "%5d  skip empty lines\n", MCX_READLINE_SKIP_EMPTY)
   ;  fprintf(stdout, "%5d  paragraph mode\n", MCX_READLINE_PAR)
   ;  fprintf(stdout, "%5d  backslash escapes newline\n", MCX_READLINE_BSC)
   ;  fprintf
      (  stdout
      ,  "%5d  section mode, ended by singly dot on a single line\n"
      ,  MCX_READLINE_DOT
      )
;  }


void mcxIOpos
(  mcxIO*    xf
,  FILE*          channel
)
   {  const char* ateof =  xf->ateof ? "at EOF in " : ""
   ;  fprintf
      (  channel
      ,  "[mclIO] %sstream <%s>, line <%ld>, character <%ld>\n"
      ,  ateof
      ,  xf->fn->str
      ,  (long) xf->lc
      ,  (long) xf->lo
      )
;  }


void mcxIOfree_v
(  void*  xfpp
)  
   {  mcxIOfree((mcxIO**) xfpp)
;  }


void mcxIOfree
(  mcxIO**  xfpp
)  
   {  if (*xfpp)
      {  mcxIO* xf = *xfpp
      ;  mcxIOrelease(xf)
      ;  if (xf->usr && xf->usr_free)
         xf->usr_free(xf->usr)
      ;  mcxFree(*xfpp)
      ;  *xfpp =  NULL
   ;  }
   }



mcxstatus mcxIOexpectReal
(  mcxIO*        xf
,  double*       dblp
,  mcxOnFail     ON_FAIL
)
   {  int   n_read   =  0
   ;  int   n_conv   =  0

   ;  mcxIOskipSpace(xf)      /* keeps accounting correct */

   ;  n_conv   =  fscanf(xf->fp, " %lf%n", dblp, &n_read)

   ;  xf->bc += n_read  /* fixme do fscanf error handling */
   ;  xf->lo += n_read

   ;  if (1 != n_conv)
      {  if (ON_FAIL == EXIT_ON_FAIL)
         {  mcxIOpos(xf, stderr)
         ;  mcxErr("parseReal", "parse error: expected to find real")
         ;  mcxExit(1)
      ;  }
         return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }


mcxstatus mcxIOexpectNum
(  mcxIO*        xf
,  long*         lngp
,  mcxOnFail     ON_FAIL
)
   {  int   n_read   =  0
   ;  int   n_conv   =  0
   ;  mcxstatus status = STATUS_OK

   ;  mcxIOskipSpace(xf)      /* keeps accounting correct */

   ;  n_conv   =  fscanf(xf->fp, "%ld%n", lngp, &n_read)

   ;  xf->bc += n_read  /* fixme do fscanf error handling */
   ;  xf->lo += n_read

   ;  if (1 != n_conv)
         mcxErr("mcxIOexpectNum", "parse error: expected to find integer")
      ,  status = STATUS_FAIL
   ;  else if (errno == ERANGE)
         mcxErr("mcxIOexpectNum", "range error: not in allowable range")
      ,  status = STATUS_FAIL

   ;  if (status)
      {  mcxIOpos(xf, stderr)
      ;  if (ON_FAIL == EXIT_ON_FAIL)
         mcxExit(1)
   ;  }

      return status
;  }



int mcxIOskipSpace
(  mcxIO*               xf
)
   {  int c
   ;  while ((c = mcxIOstep(xf)) != EOF && isspace(c))
      ;

      return  mcxIOstepback(c, xf)
;  }



   /* fixme; can't be sure to rewind on stdin */
mcxbool mcxIOtryCookie
(  mcxIO* xf
,  unsigned  number_expected
)
   {  int number_found
   ;  int n_read = fread(&number_found, sizeof(int), 1, xf->fp)

   ;  if (n_read != 1)
      return FALSE
   ;  else if (number_found != number_expected)
      {  if (fseek(xf->fp, -sizeof(int), SEEK_CUR))
         mcxErr
         (  "mcxIOtryCookie"
         ,  "beware: could not rewind %d bytes for cookie %x"
         ,  (int) sizeof(unsigned)
         ,  (unsigned) number_expected
         )
      ;  return FALSE
   ;  }

      return TRUE
;  }


mcxbool mcxIOwriteCookie
(  mcxIO* xf
,  unsigned number
)
   {  int n_written = fwrite(&number, sizeof(int), 1, xf->fp)
   ;  if (n_written != 1)
      {  mcxErr("mcxIOwriteCookie", "failed to write <%d>", number)
      ;  return FALSE
   ;  }

      return TRUE
;  }



/* fixme: newlines in str thrash correct lc/lo counting */

int mcxIOexpect
(  mcxIO          *xf
,  const char     *str
,  mcxOnFail      ON_FAIL
)
   {  const char* s  =  str
   ;  int         c  =  0
   ;  int         d  =  0
   ;  int         n_trailing

   ;  xf->pos = ftell(xf->fp)
  /*  
   *  no functional behaviour yet attached to this state change
  */

   ;  while
      (  c = (unsigned char) s[0]
      ,  
         (  c
         && (  d = mcxIOstep(xf)
            ,  c == d
            )
         )
      )
      s++

   ;  n_trailing = strlen(s)

   ;  if (c && ON_FAIL == EXIT_ON_FAIL)
      {  mcxErr("mcxIOexpect", "parse error: expected to see <%s>", str)
      ;  mcxIOpos(xf, stderr)
      ;  mcxExit(1)
   ;  }
      return n_trailing
;  }



typedef struct
{  int            tbl[256]
;  int*           circle      /* circular buffer */
;  const char*    pat
;  int            patlen
;  int            last        /* circle bumper */
;
}  mcxIOpat       ;


static void mcxIOnewpat
(  mcxIOpat* md
,  const char* pattern
)
   {  int i
   ;  int *tbl = md->tbl
   ;  const char* pat
   ;  int patlen = strlen(pattern)

   ;  md->circle = mcxAlloc(patlen * sizeof(int), EXIT_ON_FAIL)
   ;  md->pat = pattern
   ;  md->patlen = patlen

   ;  pat = md->pat
                                 /* initialize */
   ;  for (i = 0; i < 256; i ++)
      tbl[i] = patlen

   ;  for (i = 0; i < patlen-1; i++)
      tbl[(unsigned char) pat[i]] = patlen -i -1

   ;  if (0)
      for (i = 0; i < patlen; i++)
      fprintf(stderr, "shift value for %c is %d\n", pat[i], tbl[(unsigned char) pat[i]])

   ;  md->last = patlen -1
;  }


static void mcxIOcleanpat
(  mcxIOpat* md
)
   {  mcxFree(md->circle)
;  }



static int fillpatbuf
(  mcxIO* xfin
,  int shift
,  mcxIOpat* md
)
   {  int c = 0
   ;  int z = 0
   ;  int patlen = md->patlen

   ;  while (z < shift && (c = mcxIOstep(xfin)) != EOF)
      {  int q = (md->last+z+1) % patlen
      ;  md->circle[q] = c
      ;  z++
   ;  }

      md->last = (md->last+shift) % patlen

   ;  return c
;  }


mcxstatus mcxIOfind
(  mcxIO*      xfin
,  const char* pat
,  mcxOnFail   ON_FAIL  
)
   {  int j, k, c
   ;  int shift, patlen
   ;  int* tbl
   ;  int* circle
   ;  mcxIOpat md
   ;  int found = 0

   ;  mcxIOnewpat(&md, pat)

   ;  patlen   =  md.patlen
   ;  tbl      =  md.tbl
   ;  circle   =  md.circle
   ;  shift    =  patlen

     /*
      * hum. This means that empty pattern matches on empty string ..
      * Need to fix fillpatbuf if this should be reversed.
     */
   ;  if (!patlen)
      found = 1
   ;  else
      do
      {  if ((c = fillpatbuf(xfin, shift, &md) == EOF))
         break
      ;  for
         (  j=md.last+patlen, k=patlen-1
         ;  j>md.last && circle[j%patlen] == (unsigned char) pat[k]
         ;  j--, k--
         )
         ;

      ;  if (j == md.last)
         {  found = 1
         ;  break  
      ;  }
        /* 
         * if more matches are needed, do something in this branch
         * and then simply continue.
        */

         shift = tbl[circle[md.last % patlen]]

      ;  if (0)
         fprintf
         (  stderr
         ,  "___ last[%d] index[%d] pivot[%d] shift[%d]\n"
         ,  (int) md.last
         ,  (int) md.last % patlen
         ,  (int) circle[md.last % patlen]
         ,  (int) shift
         )
   ;  }
      while (1)

   ;  mcxIOcleanpat(&md)

   ;  if (!found && ON_FAIL == RETURN_ON_FAIL)
      return STATUS_FAIL
   ;  else if (!found)
      exit(EXIT_FAILURE)
   ;  else
      return STATUS_OK
;  }

