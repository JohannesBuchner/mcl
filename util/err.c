/* (c) Copyright 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "err.h"

/* TODO unify code.
*/

static FILE*   fperr   =  NULL;
static FILE*   fpwarn  =  NULL;
static FILE*   fptell  =  NULL;


void  mcx_err_f
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  va_list     *args
)
   {  if (caller)
      fprintf(fp, "___ [%s] ", caller)
   ;  else
      fprintf(fp, "___ ")

   ;  vfprintf(fp, fmt, *args)
   ;  fprintf(fp, "\n")

   ;  return
;  }


void mcxFail
(  void
)
   {  while(1) sleep(1000)
;  }


void mcxDie
(  int status
,  const char* caller
,  const char* fmt
,  ...
)
   {  va_list  args
   ;  va_start(args, fmt)
   ;  mcx_err_f(stderr, caller, fmt, &args)
   ;  va_end(args)
   ;  mcxExit(status)
;  }


void mcxExit
(  int val
)
   {  exit(val)
;  }


void mcxErrorFile
(  FILE* fp
)
   {  fperr = fp
;  }

void mcxTellFile
(  FILE* fp
)
   {  fptell = fp
;  }

void mcxWarnFile
(  FILE* fp
)
   {  fpwarn = fp
;  }


void  mcxErrf
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  ...
)
   {  va_list  args
   ;  va_start(args, fmt)
   ;  mcx_err_f(fp, caller, fmt, &args)
   ;  va_end(args)
;  }


void  mcxErr
(  const char  *caller
,  const char  *fmt
,  ...
)
   {  FILE* fp = fperr ? fperr : stderr  
   ;  va_list  args
   ;  va_start(args, fmt)
   ;  mcx_err_f(fp, caller, fmt, &args)
   ;  va_end(args)
;  }


void  mcxWarn
(  const char  *caller
,  const char  *fmt
,  ...
)
   {  va_list  args

   ;  if (caller)
      fprintf(stderr, "[%s] ", caller)

   ;  va_start(args, fmt)
   ;  vfprintf(stderr, fmt, args)
   ;  fprintf(stderr, "\n")
   ;  va_end(args)

   ;  return
;  }


void  mcx_tell_f
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  va_list     *args
,  ...
)
   {  if (caller)
      fprintf(fp, "[%s] ", caller)

   ;  vfprintf(fp, fmt, *args)
   ;  fprintf(fp, "\n")

   ;  return
;  }


void  mcxTellf
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  ...
)
   {  va_list  args
   ;  va_start(args, fmt)
   ;  mcx_tell_f(fp, caller, fmt, &args)
   ;  va_end(args)
;  }


void  mcxTell
(  const char  *caller
,  const char  *fmt
,  ...
)
   {  FILE* fp = fptell ? fptell : stderr  
   ;  va_list  args
   ;  va_start(args, fmt)
   ;  mcx_tell_f(fp, caller, fmt, &args)
   ;  va_end(args)
;  }


