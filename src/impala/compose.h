/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

#ifndef impala_compose_h__
#define impala_compose_h__

#include "matrix.h"

typedef struct mclxComposeHelper mclxComposeHelper;

mclxComposeHelper* mclxComposePrepare
(  const mclMatrix*  mx1
,  const mclMatrix*  mx2
)  ;

void mclxComposeRelease
(  mclxComposeHelper **chpp
)  ;


mclVector* mclxVectorCompose
(  const mclMatrix*        mx
,  const mclVector*        vecs
,  mclVector*              vecd
,  mclxComposeHelper*      ch
)  ;


/* result has dom_rows same as m1, dom_cols same as m2 It is *not* required
 * that identical(m1->dom_cols, m2->dom_rows) be true.  Instead, the routine
 * simply looks at the meet of those two domains.
*/

mclMatrix* mclxCompose
(  const mclMatrix*        m1
,  const mclMatrix*        m2
,  int                     maxDensity
)  ;


#endif
