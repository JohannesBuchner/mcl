
#include "app.h"
#include "version.h"

#include <stdio.h>

void app_report_version
(  const char* me
)
   {  fprintf(stdout, "%s %s, %s\n", me, mclNumTag, mclDateTag)
;  }


