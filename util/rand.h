/* (c) Copyright 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_duck_h
#define util_duck_h

#define RANDOM_MAX (2*((1<<30)-1)+1)


/*   This is for weak seeding, to obtain fresh seeds which will definitely
 *   *not* be suitable for cryptographic needs
*/

unsigned int mcxSeed
(  unsigned int seedlet
)  ;


#endif

