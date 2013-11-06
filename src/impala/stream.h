/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_stream_h
#define impala_stream_h

#include "ivp.h"
#include "vector.h"
#include "matrix.h"
#include "tab.h"

#include "util/io.h"
#include "util/types.h"
#include "util/ting.h"

mcxstatus mclxIOstreamOut
(  const mclx* mx
,  mcxIO* xf
,  mcxstatus ON_FAIL
)  ;


#define MCLXIO_STREAM_PACKED        1 <<  0
#define MCLXIO_STREAM_ABC           1 <<  1
#define MCLXIO_STREAM_123           1 <<  2
#define MCLXIO_STREAM_ETC           1 <<  3

#define MCLXIO_STREAM_READ  (MCLXIO_STREAM_PACKED | MCLXIO_STREAM_ABC | MCLXIO_STREAM_123 | MCLXIO_STREAM_ETC)

#define MCLXIO_STREAM_WARN          1 <<  4     /* a/1 warn on parse miss */
#define MCLXIO_STREAM_STRICT        1 <<  5     /* a/1 fail on parse miss */

#define MCLXIO_STREAM_MIRROR        1 <<  6     /* seeing x-y-f, add y-x-f */
#define MCLXIO_STREAM_TWODOMAINS    1 <<  7     /* construct bipartite graph  */
#define MCLXIO_STREAM_DEBUG         1 <<  8     /* debug */

#define MCLXIO_STREAM_CTAB_EXTEND   1 <<  9     /* on miss extend tab */
#define MCLXIO_STREAM_CTAB_STRICT   1 << 10     /* on miss fail */
#define MCLXIO_STREAM_CTAB_RESTRICT 1 << 11     /* on miss ignore */
#define MCLXIO_STREAM_RTAB_EXTEND   1 << 12     /* on miss extend tab */
#define MCLXIO_STREAM_RTAB_STRICT   1 << 13     /* on miss fail */
#define MCLXIO_STREAM_RTAB_RESTRICT 1 << 14     /* on miss ignore */

#define MCLXIO_STREAM_LOGTRANSFORM     1 << 15
#define MCLXIO_STREAM_NEGLOGTRANSFORM  1 << 16

#define MCLXIO_STREAM_ETC_AI        1 << 17     /* autoincrement: no column labels */


#define MCLXIO_STREAM_TAB_EXTEND (MCLXIO_STREAM_CTAB_EXTEND | MCLXIO_STREAM_RTAB_EXTEND)
#define MCLXIO_STREAM_TAB_STRICT (MCLXIO_STREAM_CTAB_STRICT | MCLXIO_STREAM_RTAB_STRICT)
#define MCLXIO_STREAM_TAB_RESTRICT (MCLXIO_STREAM_CTAB_RESTRICT | MCLXIO_STREAM_RTAB_RESTRICT)


/* If *tabcpp != NULL, the tab is taken as the starting point.
 * if mclxIOstreamIn is allowed to expand this tab, it will
 * write *ANOTHER* tab back into this argument, but only
 * if it really expanded the tab.
 *
 * Callers that ship a tab should thus have code like this:
 *
 *       mclTab* tab = myHappyTab()
 *       mclTab* tabxch = tab
 *
 *       mclxIOstreamIn(... , &tabxch , ...)
 *
 *       if (tabxch != tab)     // new tab
 *       else                   // same tab
 *
*/

mclx* mclxIOstreamIn
(  mcxIO* xf
,  mcxbits  bits
,  mclpAR*  transform
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
,  mclTab** tabcpp
,  mclTab** tabrpp
,  mcxstatus ON_FAIL
)  ;

#endif

