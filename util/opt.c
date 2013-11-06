/* (c) Copyright 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "opt.h"
#include "types.h"
#include "err.h"
#include "equate.h"
#include "minmax.h"


mcxHash* mcxOptHash
(  mcxOptAnchor*  opts
,  mcxHash*       hash
)
   {  mcxOptAnchor*  anch  =  opts ? opts+0 : NULL

   ;  hash  =  hash
               ?  hash
               :  mcxHashNew
                  (  8
                  ,  mcxStrHash
                  ,  (int (*)(const void*, const void*))strcmp
                  )
   ;  if (!hash)
      return NULL

   ;  while (anch && anch->tag)
      {  mcxKV*  kv =  mcxHashSearch(anch->tag, hash, MCX_DATUM_INSERT)

      ;  if (!kv)
         {  mcxHashFree(&hash, NULL, NULL)
         ;  return NULL
      ;  }

         if (kv->val)
         mcxErr
         (  "mcxOptHash"
         ,  "warning: option <%s> already present"
         ,  anch->tag
         )
      ;  kv->val = anch
      ;  anch++
   ;  }
      return hash
;  }


void mcxOptHashFree
(  mcxHash**   hashpp
)
   {  mcxHashFree(hashpp, NULL, NULL)
;  }


void* mcxOptInit
(  void* opt
)
   {  mcxOption* option = opt
   ;  option->anch = NULL
   ;  option->val = NULL
   ;  return option
;  }


mcxOptAnchor* mcxOptFind
(  char*  tag
,  mcxHash*    hopts  
)
   {  mcxKV* kv = mcxHashSearch(tag, hopts, MCX_DATUM_FIND)
   ;  return kv ? (mcxOptAnchor*) kv->val : NULL
;  }


mcxOption* mcxOptParse__
(  mcxHash*       opthash
,  char**         argv
,  int            argc
,  int            prefix   /* skip these */
,  int            suffix   /* skip those too */
,  int*           n_elems_read
,  mcxstatus*     status
)
   {  char**   argp  =  argv+prefix
   ;  char**   argl  =  argv+argc-suffix     /* arg last */
   ;  mcxbool  do_exhaust = n_elems_read ? TRUE : FALSE
                                  /* fixme: very ugly internal iface*/

   ;  mcxOption* opts
         =  mcxNAlloc(argc+1, sizeof(mcxOption), mcxOptInit, RETURN_ON_FAIL)
   ;  mcxOption* opt = opts

   ;  if (!opts)
      {  *status = MCX_OPT_STATUS_NOMEM
      ;  return NULL
   ;  }

      *status = MCX_OPT_STATUS_OK

   ;  if (do_exhaust)
      *n_elems_read = 0

   ;  while (argp < argl)
      {  char* arg            =  *argp
      ;  const char* embedded_val =  NULL
      ;  mcxKV* kv            =  mcxHashSearch(arg, opthash, MCX_DATUM_FIND)
      ;  mcxOptAnchor* anch   =  kv ? (mcxOptAnchor*) kv->val : NULL
      ;  const char* eq       =  strchr(arg, '=')

      ;  if (!kv && eq)
         {  char argcpy[201]
         ;  if (eq - arg < 200)
            {  strncpy(argcpy, arg, eq-arg)
            ;  argcpy[eq-arg] = '\0'
            ;  if
               (  (kv = mcxHashSearch(argcpy, opthash, MCX_DATUM_FIND))
               && (anch = (mcxOptAnchor*) kv->val)
               && (anch->flags & MCX_OPT_EMBEDDED)
               )
               embedded_val = eq+1
            ;  else if
               (  !strncmp(argcpy, "--", 2)
               && (kv = mcxHashSearch(argcpy+1, opthash, MCX_DATUM_FIND))
               && (anch = (mcxOptAnchor*) kv->val)
               )
               embedded_val = eq+1
            ;  else
               kv = NULL
               ;
            }
         }
         else if
         (  !strcmp(arg, "--")
         && do_exhaust
         )
         (*n_elems_read)++

      ;  if (kv)
         {  opt->anch = anch
         ;  if (do_exhaust)
            (*n_elems_read)++

         ;  if (embedded_val)
            opt->val = embedded_val
         ;  else if
            (  anch->flags & MCX_OPT_HASARG
            && anch->flags & MCX_OPT_EMBEDDED
            )
            {  mcxErr("mcxOptParse", "option <%s> takes =value", anch->tag)
            ;  *status  =  MCX_OPT_STATUS_NOARG
            ;  break
         ;  }
            else if (anch->flags & MCX_OPT_HASARG) 
            {  argp++
            ;  if (argp >= argl)
               {  mcxErr("mcxOptParse", "option <%s> takes value", anch->tag)
               ;  *status  =  MCX_OPT_STATUS_NOARG
               ;  break
            ;  }
               opt->val = *argp    /* mq note: shallow copy */
            ;  if (do_exhaust)
               (*n_elems_read)++
         ;  }
         }
         else
         {  if (do_exhaust)
            break
         ;  else
            {  mcxErr("mcxOptParse", "unsupported option <%s>", arg)
            ;  *status  =  MCX_OPT_STATUS_UNKNOWN
            ;  break
         ;  }
         }
         argp++
      ;  opt++
   ;  }

      if (*status)
      mcxOptFree(&opts)

   ;  return opts
;  }


mcxOption* mcxOptExhaust
(  mcxOptAnchor  *anch
,  char         **argv
,  int            argc
,  int            prefix   /* skip these */
,  int           *n_elems_read
,  mcxstatus     *status
)
   {  mcxHash* opthash  =  mcxOptHash(anch, NULL)
   ;  mcxOption* opts   =  mcxOptParse__
                           (opthash, argv, argc, prefix, 0, n_elems_read, status)
   ;  mcxOptHashFree(&opthash)
   ;  return opts
;  }


mcxOption* mcxOptParse
(  mcxOptAnchor  *anch
,  char         **argv
,  int            argc
,  int            prefix   /* skip these */
,  int            suffix   /* skip those too */
,  mcxstatus     *status
)
   {  mcxHash* opthash  =  mcxOptHash(anch, NULL)
   ;  mcxOption* opts   =  mcxOptParse__(opthash, argv, argc, prefix, suffix, NULL, status)
   ;  mcxOptHashFree(&opthash)
   ;  return opts
;  }


mcxOption* mcxHOptExhaust
(  mcxHash       *opthash
,  char         **argv
,  int            argc
,  int            prefix   /* skip these */
,  int           *n_elems_read
,  mcxstatus     *status
)
   {  return mcxOptParse__
      (opthash, argv, argc, prefix, 0, n_elems_read, status)
;  }


mcxOption* mcxHOptParse
(  mcxHash       *opthash
,  char         **argv
,  int            argc
,  int            prefix   /* skip these */
,  int            suffix   /* skip those too */
,  mcxstatus     *status
)
   {  return mcxOptParse__(opthash, argv, argc, prefix, suffix, NULL, status)
;  }


void mcxOptFree
(  mcxOption** optpp
)
   {  if (*optpp)
      mcxFree(*optpp)
   ;  *optpp = NULL
;  }


void mcxUsage
(  FILE* fp
,  const char*  caller
,  const char** lines
)
   {  int i =  0

   ;  while(lines[i])
      {  fprintf(fp, "%s\n", lines[i])
      ;  i++
   ;  }
      fprintf(fp, "[%s] Printed %d lines\n", caller, i+1)
;  }



static int (*rltFunctions[8])(const void* f1, const void* f2) =  
{  intGt  ,  intGq  ,  fltGt,  fltGq
,  intLt  ,  intLq  ,  fltLt,  fltLq
}  ;


int mcxOptPrintDigits       =  5;


const char* rltSigns[8] =
{  "(",  "[",  "(",  "["
,  ")",  "]",  ")",  "]"
}  ;

/* 
 * todo: NULL lftbound argument & non-NULL lftRelate argument
 * idem for rgt
*/

static int checkBoundsUsage
(  char        type
,  void*       var
,  int         (*lftRlt) (const void*, const void*)
,  void*       lftBound
,  int         (*rgtRlt) (const void*, const void*)
,  void*       rgtBound
)
   {  int  i
   ;  const char* me  = "checkBoundsUsage PBD"

   ;  if (type != 'f' && type != 'i')
      {  mcxErr(me, "unsupported checkbound type <%c>", type)
      ;  return 1
   ;  }

      if ((lftRlt && !lftBound)||(!lftRlt && lftBound))
      {  mcxErr(me, "abusive lftRlt lftBound combination")
      ;  return 1
   ;  }
      if ((rgtRlt && !rgtBound)||(!rgtRlt && rgtBound))
      {  mcxErr(me, "abusive rgtRlt rgtBound combination")
      ;  return 1
   ;  }

      if (lftRlt)
      {  for(i=0;i<4;i++) if (lftRlt == rltFunctions[i]) break
      ;  if (i == 4)
         {  mcxErr(me, "lftRlt should use gt or gq arg")
         ;  return 1
      ;  }
   ;  }

      if (rgtRlt)
      {  for(i=4;i<8;i++) if (rgtRlt == rltFunctions[i]) break
      ;  if (i==8)
         {  mcxErr(me, "rgtRlt should use lt or lq arg")
         ;  return 1
      ;  }
   ;  }
      return 0
;  }



/*    returns
 *    STATUS_OK      for matching bounds
 *    STATUS_FAIL    for non-matching bounds
 *    STATUS_CB_PBD  for internal error.
*/

enum { STATUS_CB_PBD = STATUS_UNUSED + 1 } ;

static mcxstatus checkBounds
(  char        type
,  void*       var
,  int         (*lftRlt) (const void*, const void*)
,  void*       lftBound
,  int         (*rgtRlt) (const void*, const void*)
,  void*       rgtBound
)
   {  int lftOk, rgtOk
   ;  if (checkBoundsUsage(type, var, lftRlt, lftBound, rgtRlt, rgtBound))
      {  mcxErr("checkBounds PBD", "internal error -- cannot validate")
      ;  return STATUS_CB_PBD
   ;  }
      lftOk =   !lftRlt || lftRlt(var, lftBound)
   ;  rgtOk =   !rgtRlt || rgtRlt(var, rgtBound)
   ;  return (lftOk && rgtOk) ? STATUS_OK : STATUS_FAIL
;  }



static mcxTing* checkBoundsRange
(  char        type
,  void*       var
,  int         (*lftRlt) (const void*, const void*)
,  void*       lftBound
,  int         (*rgtRlt) (const void*, const void*)
,  void*       rgtBound
)
   {  mcxTing*  textRange  =  mcxTingEmpty(NULL, 40)
   ;  char* lftToken       =  (char *) "<?"
   ;  char* rgtToken       =  (char *) "?>"
   ;  int i

   ;  if (!textRange)
      return NULL

   ;  if (lftRlt)
      {  for(i=0;i<4;i++)
         if (lftRlt == rltFunctions[i])
         break
      ;  if (i<4)
         lftToken = (char *) rltSigns[i]
   ;  }
      else
      lftToken = (char *) "("

   ;  mcxTingPrint(textRange, "%s", lftToken)
     /*
      * This might fail due to mem shortage;
      * we ignore and simply keep plodding on with more mcxTingPrint-s
      * below. It's actually pretty inconceivable, since we alloced
      * 40 bytes - it depends on mcxOptPrintDigits.
     */

   ;  if (lftBound)
      {  if (type == 'f')
         mcxTingPrintAfter
         (textRange, "%.*f", mcxOptPrintDigits, *((float*)lftBound))
      ;  else if (type == 'i')  
         mcxTingPrintAfter(textRange, "%d", *((int*)lftBound))
   ;  }
      else
      mcxTingPrintAfter(textRange, "%s", "<-")

   ;  mcxTingPrintAfter(textRange, "%s", ",")

   ;  if (rgtBound)
      {  if (type == 'f')
         mcxTingPrintAfter
         (textRange, "%.*f", mcxOptPrintDigits, *((float*)rgtBound))
      ;  else if (type == 'i')
         mcxTingPrintAfter(textRange, "%d", *((int*)rgtBound))
   ;  }
      else
      mcxTingPrintAfter(textRange, "%s", "->")

   ;  if (rgtRlt)
      {  for(i=4;i<8;i++)
         if (rgtRlt == rltFunctions[i])
         break
      ;  if (i<8)
         rgtToken = (char *) rltSigns[i]
   ;  }
      else
      rgtToken = (char *) ")"

   ;  mcxTingPrintAfter(textRange, "%s", rgtToken)
   ;  return textRange
;  }


mcxbool mcxOptCheckBounds
(  const char*    caller
,  const char*    flag
,  char           type
,  void*          var
,  int            (*lftRlt) (const void*, const void*)
,  void*          lftBound
,  int            (*rgtRlt) (const void*, const void*)
,  void*          rgtBound
)
   {  mcxTing* textRange
   ;  mcxstatus stat
      =  checkBounds(type, var, lftRlt, lftBound, rgtRlt, rgtBound)

   ;  if (stat == STATUS_CB_PBD)
      {  mcxErr("mcxOptCheckBounds", "cannot validate option %s", flag)
      ;  return FALSE
   ;  }
      else if (stat == STATUS_FAIL)
      {  if
         (! (  textRange
            =  checkBoundsRange
               (  type
               ,  var
               ,  lftRlt
               ,  lftBound
               ,  rgtRlt
               ,  rgtBound
         )  )  )
         return FALSE

      ;  mcxErr
         (  caller
         ,  "%s argument to %s should be in range %s"
         ,  type == 'i' ? "integer" : type == 'f' ? "float" : "MICANS"
         ,  flag
         ,  textRange->str
         )
      ;  mcxTingFree(&textRange)
      ;  return FALSE
   ;  }
      return TRUE
;  }


int mcxOptAnchorCmpId
(  const void *a1
,  const void *a2
)
   {  return (((mcxOptAnchor*) a1)->id - ((mcxOptAnchor*) a2)->id)
;  }


int mcxOptAnchorCmpTag
(  const void *a1
,  const void *a2
)
   {  const mcxOptAnchor* A1 = a1
   ;  const mcxOptAnchor* A2 = a2
   ;  return   A1->tag && A2->tag
            ?  strcmp(A1->tag, A2->tag) 
            :     A1->tag
               ?  -1
               :  1
;  }


void mcxOptAnchorSortByTag
(  mcxOptAnchor *anchors
,  int n_anchors
)
   {  qsort(anchors, n_anchors, sizeof(mcxOptAnchor), mcxOptAnchorCmpTag)
;  }


void mcxOptAnchorSortById
(  mcxOptAnchor *anchors
,  int n_anchors
)
   {  qsort(anchors, n_anchors, sizeof(mcxOptAnchor), mcxOptAnchorCmpId)
;  }


void parse_descr
(  const char* field
,  const char** descrp
,  const char** markp
,  int* mark_width
)
   {  const char* m = strstr(field, "\tM")
   ;  const char* n = m ? strstr(m+2, "\t") : NULL
   ;  const char* d = strstr(field, "\tD")

   ;  if (m && n)
         *mark_width = n - m - 2
      ,  *markp = m + 2
   ;  else
         *markp = ""
      ,  *mark_width = 0

   ;  *descrp = d ? d + 2 : field
;  }


void mcxOptApropos
(  FILE* fp
,  const char* me                /* unused currently */
,  const char* syntax
,  int width
,  mcxbits display
,  const mcxOptAnchor opt[]
)
   {  const mcxOptAnchor *baseopt = opt
   ;  mcxTing* scr = mcxTingEmpty(NULL, 100)
   ;  int id_prev = -1
   ;  const char* descr_usage, *descr_mark
   ;  int mark_width = 0, mark_width_max = 0
   ;  int mywidth = 0

   ;  if (syntax)
      fprintf(fp, "%s\n\n", syntax)

   ;  for (opt = baseopt; opt->tag; opt++)
      {  int thislen = strlen(opt->tag)
      ;  if (opt->descr_arg)
         thislen += 1 + strlen(opt->descr_arg)
      ;  if
         (  !(opt->flags & MCX_OPT_HIDDEN)
         || display & MCX_OPT_DISPLAY_HIDDEN
         )
         mywidth = MAX(mywidth, thislen)

      ;  if (opt->descr_usage)
         {  parse_descr
            (opt->descr_usage, &descr_usage, &descr_mark, &mark_width)
         ;  mark_width_max = MAX(mark_width_max, mark_width)
      ;  }
      }

      if (!width)
      width = mywidth

   ;  for (opt = baseopt; opt->tag; opt++)
      {  const char* skip = ""
      ;  if
         (  opt->flags & MCX_OPT_HIDDEN
         && !(display & MCX_OPT_DISPLAY_HIDDEN)
         )
         {  id_prev = opt->id
         ;  continue
      ;  }

         if (display & MCX_OPT_DISPLAY_SKIP && opt->id - id_prev > 1)
         skip = "\n"
      ;  id_prev = opt->id
      
      ;  if
         (  (  opt->flags & MCX_OPT_HASARG
            || opt->flags & MCX_OPT_EMBEDDED
            )
            && opt->descr_arg
         )
         mcxTingPrint
         (  scr
         ,  "%s%c%s"
         ,  opt->tag
         ,  opt->flags & MCX_OPT_EMBEDDED ? '=' : ' '
         ,  opt->descr_arg
         )
      ;  else
         mcxTingPrint(scr, "%s", opt->tag)

      ;  fputs(skip, fp)

      ;  if (!opt->descr_usage)
         {  fprintf(fp, "%s\n", scr->str)
         ;  continue
      ;  }

         if (mark_width_max)
         {  parse_descr
            (opt->descr_usage, &descr_usage, &descr_mark, &mark_width)
         ;  fprintf(fp, "%-*s", width, scr->str)
         ;  fprintf(fp, " %-*.*s  ",(int) mark_width_max,mark_width, descr_mark)
         ;  fprintf(fp, "%s\n", descr_usage)
      ;  }
         else
         {  fprintf(fp, "%-*s", width, scr->str)
         ;  fprintf(fp, " %s\n", opt->descr_usage)
      ;  }
      }
   }



