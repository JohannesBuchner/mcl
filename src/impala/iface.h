/*   (C) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_iface_h
#define impala_iface_h

#include "util/io.h"

extern   int            mclTrackImpalaPruning;
extern   int            mclTrackImpalaPruningInterval;
extern   int            mclTrackImpalaPruningOffset;
extern   int            mclTrackImpalaPruningBound;

extern   int            mclWarningImpala;

extern   mcxIO*         mclTrackStreamImpala;

#endif

