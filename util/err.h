/* (c) Copyright 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_err_h
#define util_err_h


void  mcxTell
(  const char  *caller
,  const char  *fmt
,  ...
)  
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
   ;


void  mcxTellf
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  ...
)  
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
   ;

void  mcxWarn
(  const char  *caller
,  const char  *fmt
,  ...
)
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
   ;

void  mcxErr
(  const char  *caller
,  const char  *fmt
,  ...
)  
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
   ;

void  mcxErrf
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  ...
)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
   ;

void  mcxDie
(  int status
,  const char  *caller
,  const char  *fmt
,  ...
)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
   ;

void mcxTellFile
(  FILE* fp
)  ;

void mcxWarnFile
(  FILE* fp
)  ;

void mcxErrorFile
(  FILE* fp
)  ;

void mcxFail
(  void
)  ;

void mcxExit
(  int val
)  ;

#endif

