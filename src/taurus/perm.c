/*  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "perm.h"

#include "util/alloc.h"
#include "util/types.h"


Pmt*  pmtNew
(  int      N
)
   {  Pmt*     pnew

   ;  if ((!(pnew = mcxAlloc(sizeof(Pmt), EXIT_ON_FAIL))))
      return NULL

   ;  pnew->n        =     N
   ;  pnew->next     =     ilNew(N, NULL)
   ;  pnew->i_cycle  =     NULL
   ;  pnew->n_cycle  =     0
   ;  pnew->cycles   =     NULL

   ;  if (!pnew->next)
      pmtFree(&pnew)
   ;  else
      ilSet(pnew->next, -1)

   ;  return pnew
;  }


void pmtFree
(  Pmt      **pmt
)
   {  Pmt  *this  =  *pmt
   ;  int      i

   ;  if (this->next != (void*) 0)
      ilFree(&(this->next))

   ;  if (this->n_cycle)
      {  for (i=0;i<this->n_cycle;i++)
         {  mcxFree((this->cycles+i)->L)
      ;  }

      ;  mcxFree(this->cycles)
   ;  }

      if (this->i_cycle != (void*) 0)
      {  ilFree(&(this->i_cycle))
   ;  }

   ;  mcxFree(*pmt)
   ;  *pmt = (void*) 0
;  }


Pmt*  pmtRand
(  int      N
)
   {  Pmt*     pmt         =  pmtNew(N)

   ;  pmt->next            =  ilRandPermutation(0, N)
   ;  pmtGetCycles(pmt)
   ;  return pmt
;  }


mcxIL*  pmtGetCycleSizes
(  Pmt*     pmt
)
   {  mcxIL *il
   ;  int i  

   ;  if (!(il = ilNew(pmt->n_cycle, NULL)))
      return NULL

   ;  for (i=0;i<pmt->n_cycle;i++)
      *(il->L+i) = (pmt->cycles+i)->n

   ;  return il  
;  }


/* fixme, a mess, memwise
*/

Pmt*  pmtGetCycles
(  Pmt*     pmt
)
   {  int   l_uncl, i
   ;  int   N              =  pmt->n
   ;  int   n_cycle        =  0              /* index of current cycle     */
   ;  mcxIL*   i_cycle
   ;  mcxIL*   il_next     =  pmt->next
   ;  mcxIL*   cyclebuf    =  ilNew(N, NULL)

   ;  ilSet(cyclebuf, -1)

   ;  pmt->i_cycle         =  ilNew(N, NULL)
   ;  ilSet(pmt->i_cycle, -1)

   ;  i_cycle              =  pmt->i_cycle
                                             /* over-allocate cycle structs */
   ;  pmt->cycles          =  (mcxIL*) mcxAlloc
                              (  N * sizeof(mcxIL)
                              ,  RETURN_ON_FAIL
                              )

   ;  if (N && !pmt->cycles)
         mcxMemDenied(stderr, "pmtGetCycles", "mcxIL", N)
      ,  exit(1)

   ;  for(i=0;i<N;i++)
      ilInit(pmt->cycles+i)
                                             /* initialize cycle structs   */

   ;  l_uncl   =  0
                                             /* least unclassified node    */

   ;  while (l_uncl < N)
      {  int   next                 =  *(il_next->L + l_uncl)

                                             /*    index of current cycle,
                                              *    buffer initialization,
                                              *    buffer size.
                                              */
      ;  *(i_cycle->L+l_uncl)    =  n_cycle
      ;  *(cyclebuf->L+0)        =  l_uncl
      ;  cyclebuf->n                =  1

      ;  if (next < 0 || next >= N)
         {  fprintf
            (  stderr
            ,  "[pmtGetCycles fatal] index %d out of bounds [0, %d)\n"
            ,  next
            ,  N
            )
         ;  exit(1)
      ;  }

      ;  while (*(i_cycle->L+next) < 0)    /* not in previous cycle      */
         {
            *(cyclebuf->L + cyclebuf->n) = next
         ;  cyclebuf->n++
         ;  *(i_cycle->L+next)       =  n_cycle

         ;  next = *(il_next->L+next)

         ;  if (next < 0 || next >= N)
            {  fprintf
               (  stderr
               ,  "[pmtGetCycles fatal] index %d out of bound [0, %d)\n"
               ,  next
               ,  N
               )
            ;  exit(1)
         ;  }
      ;  }

                                             /* current cycle completed */
         if (*(i_cycle->L+next) == n_cycle)
         {  ilInstantiate(pmt->cycles+n_cycle, cyclebuf->n, cyclebuf->L)
         ;  n_cycle++
      ;  }
                                             /*    closing element belongs to
                                              *    other cycle
                                             */
         else if (*(i_cycle->L+next) != n_cycle)
         {  
            fprintf(stderr, "[pmtGetCycles fatal] permutation not 1-1\n")
         ;  ilPrint(il_next, "")
         ;  exit(1)
      ;  }

      ;  while (*(i_cycle->L+l_uncl) >= 0 && l_uncl < N)  
            l_uncl++
   ;  }

   ;  ilFree(&cyclebuf)
   ;  pmt->n_cycle = n_cycle
   ;  return pmt
;  }


void  pmtPrint
(  Pmt*     pmt
)
   {  int   i
   ;  fprintf(stdout, "Dimension %d Cycles %d\n", pmt->n, pmt->n_cycle)
   ;  ilPrint(pmt->next, "bijection")

   ;  for (i=0;i<pmt->n_cycle;i++)
      ilPrint(pmt->cycles+i, "cycle")
;  }



