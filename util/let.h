/*   Copyright (C) 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_let
#define util_let

#include "types.h"
#include "ting.h"

#define TRM_FAIL   0
#define TRM_ISNUM  1
#define TRM_ISREAL 2
#define TRM_ISINF  3
#define TRM_ISNAN  4

typedef struct telRaam telRaam;

telRaam* trmInit
(  const char* str
)  ;

mcxstatus trmExit
(  telRaam*  raam
)  ;

mcxstatus trmParse
(  telRaam* raam
)  ;

int trmEval
(  telRaam* raam
,  long* lp
,  double* fp
)  ;

void trmRegister
(  telRaam* raam
,  int      (user_parse)(mcxTing* txt, int offset)
,  mcxenum  (user_eval)(const char* token, long *ival, double *fval)
,  char     user_char
)  ;

mcxbool trmError
(  int  flags
)  ;

mcxbool trmIsReal
(  int  flags
)  ;

mcxbool trmIsNum
(  int  flags
)  ;

mcxbool trmIsNan
(  int  flags
)  ;

mcxbool trmIsInf
(  int  flags
)  ;

void trmDump
(  telRaam* raam
,  const char* msg
)  ;

void trmDebug
(  void
)  ;

#endif

