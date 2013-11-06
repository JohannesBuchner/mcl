/*   (C) Copyright 2005, 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_app_h__
#define impala_app_h__

void app_report_version
(  const char* me
)  ;

void mclxSetBinaryIO
(  void
)  ;

void mclxSetInterchangeIO
(  void
)  ;

void mclx_app_init
(  FILE* fp
)  ;


#endif
