/*   (C) Copyright 2005, 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "io.h"
#include "version.h"

#include "util/ting.h"
#include "util/err.h"

void app_report_version
(  const char* me
)
   {  fprintf
      (  stdout
      ,  "%s %s, %s\n"
"Copyright (c) 1999-2008, Stijn van Dongen. mcl comes with NO WARRANTY\n"
"to the extent permitted by law. You may redistribute copies of mcl under\n"
"the terms of the GNU General Public License.\n"
      ,   me
      ,  mclDateTag
      ,  mclNumTag
      )
;  }


void mclxSetBinaryIO
(  void
)
   {  mcxTing* tmp = mcxTingPrint(NULL, "MCLXIOFORMAT=8")
   ;  char* str = mcxTinguish(tmp)
   ;  putenv(str)
;  }


void mclxSetInterchangeIO
(  void
)
   {  mcxTing* tmp = mcxTingPrint(NULL, "MCLXIOFORMAT=2")
   ;  char* str = mcxTinguish(tmp)
   ;  putenv(str)
;  }


void mclx_app_init
(  FILE* fp
)
   {  mcxLogSetFILE(fp, TRUE)
;  }


