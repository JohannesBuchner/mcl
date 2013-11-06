
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "io.h"
#include "version.h"

#include "util/ting.h"

void app_report_version
(  const char* me
)
   {  fprintf(stdout, "%s %s, %s\n", me, mclNumTag, mclDateTag)
;  }


void mclxSetBinaryIO
(  void
)
   {  mcxTing* tmp = mcxTingPrint(NULL, "MCLXIOFORMAT=8")
   ;  const char* str = mcxTinguish(tmp)
   ;  putenv(str)
;  }


void mclxSetInterchangeIO
(  void
)
   {  mcxTing* tmp = mcxTingPrint(NULL, "MCLXIOFORMAT=2")
   ;  const char* str = mcxTinguish(tmp)
   ;  putenv(str)
;  }


