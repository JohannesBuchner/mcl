/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004 Stijn van Dongen
 *   (C) Copyright 2005, 2006, 2007, 2008, 2009  Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#include <string.h>
#include <stdio.h>

#include "mcx.h"

#include "mcxconvert.h"
#include "mcxquery.h"
#include "mcxdiameter.h"
#include "mcxerdos.h"
#include "mcxclcf.h"
#include "mcxcollect.h"
#include "mcxtab.h"
#include "mcxalter.h"

#include "impala/matrix.h"
#include "impala/io.h"
#include "impala/iface.h"
#include "impala/compose.h"
#include "impala/ivp.h"
#include "impala/app.h"
#include "taurus/ilist.h"
#include "taurus/la.h"

#include "util/types.h"
#include "util/err.h"
#include "util/opt.h"
#include "util/ding.h"


const char* mcx_syntax =
   "Usage: mcx <mode> [options] [files]\n"
   "       mcx <mode> -h  (for mode specific options)"
   ;


enum
{  ID_CONVERT = 0
,  ID_QUERY
,  ID_ERDOS
,  ID_CTTY
,  ID_DIAMETER
,  ID_CLCF
,  ID_COLLECT
,  ID_TAB
,  ID_ALTER
,  ID_UNUSED
}  ;


mcxDispEntry mcx_dir[] =
{  {  ID_CONVERT  ,  mcxDispHookConvert      }
,  {  ID_QUERY    ,  mcxDispHookquery        }
,  {  ID_DIAMETER ,  mcxDispHookDiameter     }
,  {  ID_CLCF     ,  mcxDispHookClcf         }
,  {  ID_ERDOS    ,  mcxDispHookErdos        }
,  {  ID_CTTY     ,  mcxDispHookCtty         }
,  {  ID_COLLECT  ,  mcxDispHookCollect      }
,  {  ID_ALTER    ,  mcxDispHookAlter        }
,  {  ID_TAB      ,  mcxDispHookTab          }
,  {  -1          ,  NULL                    }
}  ;


mcxOptAnchor mcxSharedOptions[] =
{  {  "--version"
   ,  MCX_OPT_DEFAULT
   ,  MCX_DISP_VERSION
   ,  NULL
   ,  "output version information, exit"
   }
,  {  "--test"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MCX_DISP_TEST
   ,  NULL
   ,  "test"
   }
,  {  "-progress"
   ,  MCX_OPT_HASARG
   ,  MCX_DISP_PROGRESS
   ,  "<int>"
   ,  "set progress interval size"
   }
,  {  "--progress"
   ,  MCX_OPT_DEFAULT
   ,  MCX_DISP_PROGRESS2
   ,  NULL
   ,  "turn on progress reporting"
   }
,  {  "-debug"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MCX_DISP_DEBUG
   ,  "<int>"
   ,  "set debug level or bits"
   }
,  {  "--debug"
   ,  MCX_OPT_DEFAULT | MCX_OPT_HIDDEN
   ,  MCX_DISP_DEBUG2
   ,  NULL
   ,  "turn on debugging"
   }
,  {  "-set"
   ,  MCX_OPT_HASARG
   ,  MCX_DISP_SET
   ,  "key=val"
   ,  "set key to val in ENV"
   }
,  {  "--nop"
   ,  MCX_OPT_DEFAULT
   ,  MCX_DISP_NOP
   ,  NULL
   ,  "this option has no affect but changing the argument count"
   }
,  {  "-h"
   ,  MCX_OPT_DEFAULT
   ,  MCX_DISP_HELP
   ,  NULL
   ,  "this"
   }
,  {  "--help"
   ,  MCX_OPT_DEFAULT
   ,  MCX_DISP_APROPOS
   ,  NULL
   ,  "this"
   }
,  {  "--amoixa"
   ,  MCX_OPT_HIDDEN
   ,  MCX_DISP_AMOIXA
   ,  NULL
   ,  "show hidden options too"
   }
,  {  NULL ,  0 ,  0 ,  NULL, NULL}
}  ;


mcxbits  mcx_debug_g     =  0;
unsigned mcx_progress_g  =  0;
mcxbool  mcx_test_g      =  FALSE;


mcxstatus sharedArgHandle
(  int optid
,  const char* val
,  mcxDispHook*   hook
,  mcxDispBundle* bundle
)
   {  mcxTing* full_syntax = mcxTingPrint(NULL, "%s %s", "mcx", hook->syntax)
   ;  switch(optid)
      {  case MCX_DISP_HELP
      :  case MCX_DISP_APROPOS
      :  case MCX_DISP_AMOIXA
      :  mcxOptApropos
         (  stdout
         ,  hook->name
         ,  full_syntax->str
         ,  15
         ,  0
         ,  bundle->disp_shared
         )
      ;  mcxOptApropos
         (  stdout
         ,  hook->name
         ,  NULL
         ,  15
         ,     MCX_OPT_DISPLAY_SKIP
            |  (  optid == MCX_DISP_AMOIXA
               ?  MCX_OPT_DISPLAY_HIDDEN
               :  0
               )
         ,  hook->options
         )
      ;  mcxExit(0)
      ;  break
      ;

         case MCX_DISP_VERSION
      :  bundle->disp_version(hook->name)
      ;  mcxExit(0)
      ;  break
      ;

         case MCX_DISP_NOP
      :  NOTHING
      ;  break
      ;

         case MCX_DISP_TEST
      :  mcx_test_g = TRUE
      ;  break
      ;

         case MCX_DISP_PROGRESS
      :  mcx_progress_g = atoi(val)
      ;  break
      ;

         case MCX_DISP_PROGRESS2
      :  mcx_progress_g = 1
      ;  break
      ;

         case MCX_DISP_DEBUG
      :  mcx_debug_g = atoi(val)
      ;  break
      ;

         case MCX_DISP_DEBUG2
      :  mcx_debug_g = -1u
      ;  break
      ;

         case MCX_DISP_SET
      :  mcxSetenv(val)
      ;  break
      ;

         default
      :  break
      ;
      }
      mcxTingFree(&full_syntax)
   ;  return STATUS_OK
;  }


int main
(  int                  argc
,  const char*          argv[]
)
   {  mcxDispBundle bundle
   ;  mclxIOsetQMode("MCLXIOVERBOSITY", MCL_APP_VB_YES)

   ;  mcxLogLevel =
      MCX_LOG_AGGR | MCX_LOG_MODULE | MCX_LOG_IO | MCX_LOG_GAUGE | MCX_LOG_WARN

   ;  mclx_app_init(stderr)

   ;  bundle.disp_argc     =  argc
   ;  bundle.disp_argv     =  argv
   ;  bundle.disp_name     =  "mcx"
   ;  bundle.disp_syntax   =  mcx_syntax
   ;  bundle.disp_shared   =  mcxSharedOptions
   ;  bundle.n_disp_shared =  sizeof(mcxSharedOptions) / sizeof(mcxOptAnchor) -1;
   ;  bundle.disp_table    =  mcx_dir
   ;  bundle.disp_version  =  app_report_version
   ;  bundle.shared_handler=  sharedArgHandle
      
   ;  return mcxDispatch(&bundle)
;  }


