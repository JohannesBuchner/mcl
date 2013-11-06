/* (c) Copyright 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "rand.h"

unsigned int mcxSeed
(  unsigned int i
)
   {  pid_t   p  = getpid()
   ;  pid_t   pp = getppid()

   ;  time_t  t  = time(NULL)

   ;  unsigned int  s  =      (p ^ p << 4 ^ p << 16 ^ p << 28)
                           ^  (pp ^ pp << 8 ^ pp << 24)
                           ^  (t ^ t << 12 ^ t << 20)
                           ^  (i ^ i << 3 ^ i << 23 ^ i << 26)

         /* I have no solid evidence backing up the usefulness of the xors.
          * They won't increase entropy anyway of course, and this function is
          * the worst to use if you want to have a cryptographically strong
          * seed.  Anyway, the xors do seem useful in order to spread input
          * bits out over the output space, as seen from some hashing
          * experiments.
         */
   ;  return s
;  }


