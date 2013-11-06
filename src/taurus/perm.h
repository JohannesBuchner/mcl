/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#ifndef taurus_perm_h
#define taurus_perm_h

#include "ilist.h"


typedef struct
{  int      n              /* the size of the permutation               */
;  mcxIL*   next           /* the image of a number                     */
;  int      n_cycle           
;  mcxIL*   i_cycle        /* the cycle index of a number, may be NULL  */
;  mcxIL*   cycles         /* decomposition into cycles; may be NULL    */
;  
}  Pmt      ;


Pmt*  pmtNew
(  int N
)  ;

void pmtFree
(  Pmt** pmt
)  ;

Pmt*  pmtRand
(  int N
)  ;

Pmt*  pmtGetCycles
(  Pmt* pmt
)  ;

mcxIL*  pmtGetCycleSizes
(  Pmt* pmt
)  ;

void  pmtPrint
(  Pmt* pmt
)  ;

#endif

