/*   (C) Copyright 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdlib.h>
#include <stdio.h>

#include "iface.h"

#include "util/io.h"

int   mclWarningImpala                 =  1;

/*
 * all mclTrackImpalaPruning stuff is obsolete, kept because it might
 * be transfered and reinstated in mcl/expand.c
*/
int   mclTrackImpalaPruning            =  0;
int   mclTrackImpalaPruningInterval    =  1;
int   mclTrackImpalaPruningOffset      =  0;
int   mclTrackImpalaPruningBound       =  0;

mcxIO *mclTrackStreamImpala            =  NULL;

