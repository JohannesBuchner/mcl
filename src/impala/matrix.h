/*  (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* TODO
 *    make callback/accumulator mechanism to obtain characteristic
 *    vectors for matrix (columns sum, max, self value).
 *
 *    think about mclvTernary in conjunction with sophisticated
 *    mask/merge operations.
 *
 *    cut down on subroutines. unify mclxAccomodate and mclxAllocClone
 *    and possibly others.
*/


#ifndef impala_matrix_h
#define impala_matrix_h

#include <stdio.h>

#include "ivp.h"
#include "vector.h"
#include "pval.h"

#include "util/types.h"



typedef struct
{  mclv*     cols
;  mclv*     dom_cols
;  mclv*     dom_rows
;
}  mclMatrix      ;

#define mclx mclMatrix


/* INTEGRITY
 *    Remember the constraint on a vector: The idx members of the ivp array
 *    need to be nonnegative and ascending.  The val members are usually
 *    positive, but this is not mandated.
 *
 *    cols is an array of vectors. The count of vectors is equal to
 *    dom_cols->n_ivps (but always use the macro N_COLS(mx)). The successive
 *    vids (the vid member) of the vectors in cols are the indices (the idx
 *    members of the ivps) in dom_cols. These vids must be ascending and
 *    nonnegative.  Obviously there is duplication of data here; the vid
 *    members of the vectors in cols are exactly the indices of the dom_cols
 *    member.  This duplication is quite useful however, as many matrix
 *    operations require domain access, checking, and manipulation that can
 *    easily be formulated in terms of base vector methods.  Only when doing
 *    non-standard stuff with matrices one must take care to maintain data
 *    integrity; i.e. when adding a column to a matrix.  An example of this is
 *    found in mcl/clm.c
 *
 *    The vectors accessible via cols have entries (i.e. ivp idx members) that
 *    must be present as indices (of the ivps in) in dom_rows.
 *
 *    The setting cols == NULL, dom_cols->n_ivps == 0, dom_cols->ivps == NULL
 *    is obviously legal.
 *
 *    In normal mode of operation, all matrix operations are simply carried out
 *    by routines in this interface, and they will ensure the integrity of the
 *    matrix structure. If you need to do something out of the ordinary, beware
 *    of the constraints just given.
 *
 * NOTE
 *    All unary and binary operations remove resulting entries that are zero.
 *    E.g. mclxAdd, mclxHadamard (both binary).  Unary examples are currently
 *    perhaps not present.
 *
 *    This means it is dangerous to use those operations on matrices
 *    representing value tables that contain zero-valued entries -
 *    one might want to combine two different such matrices on different
 *    domains simply by using mclxAdd, but the zero-valued entries will
 *    be removed.
 *
 *    There is currently no easy way around this. See also ../README .
*/

 /* only allowed as rvalue */
#define N_COLS(mx) ((long) ((mx)->dom_cols->n_ivps * 1))
#define N_ROWS(mx) ((long) ((mx)->dom_rows->n_ivps * 1))
#define MAXID_COLS(mx) (N_COLS(mx) ? ((mx)->dom_cols->ivps)[N_COLS(mx)-1].idx : 0)
#define MAXID_ROWS(mx) (N_ROWS(mx) ? ((mx)->dom_rows->ivps)[N_ROWS(mx)-1].idx : 0)

#define mclxRowCanonical(mx) mcldIsCanonical((mx)->dom_rows)
#define mclxColCanonical(mx) mcldIsCanonical((mx)->dom_cols)
#define mclxIsGraph(mx)  mcldEquate((mx)->dom_rows, (mx)->dom_cols, MCLD_EQ_EQUAL)

/* args become members of object */

mclx* mclxAllocZero
(  mclv*  dom_cols
,  mclv*  dom_rows
)  ;

mclx* mclxAllocClone
(  const mclx* mx
)  ;


/* args become members of object
*/

mclx* mclxCartesian
(  mclv*  dom_cols
,  mclv*  dom_rows
,  double  val
)  ;


/*    All arguments remain owned by caller.
 *    The select domains need not be subdomains of the matrix domains.
 *    They can take any form; mclxSub will do the appropriate thing
 *    (keeping those entries that are in line with the new domains).
 *
 *    The domains of the new matrix will always be clones of the selection
 *    domains as supplied by caller, even if those are not subdomains of the
 *    corresponding matrix domains.
 *    In this respect, the routine acts just like mclxChangeDomains,
 *    which is effectively an in-place variant of mclxSub.
 *
 *    If the select domains are wider than the matrix domains, nothing
 *    will be removed and only the domains are changed.
*/

mclx*  mclxSub
(  const mclx*     mx
,  const mclv*     colSelect
,  const mclv*     rowSelect
)  ;

/* TODO
 * create

   mclx*  mclxMaskedMerge
   (  const mclx*  mx
   ,  const mclv*  col_select
   ,  const mclv*  row_select
   ,  double (*f)(pval, pval)
   )  ;

 * which will use mclvTernary to avoid spurious mallocing.
*/


/*  gives back extended sub: all nodes to and from
 *  these two domains
*/

mclx*  mclxExtSub
(  const mclx*  mx
,  const mclv*  col_select
,  const mclv*  row_select
)  ;



/*
 *    Make sure that mx domains include dom_cols and dom_rows
*/

void mclxAccomodate
(  mclx* mx
,  const mclv* dom_cols
,  const mclv* dom_rows
)  ;


/*    Change the domains of a matrix. This routine is effectively
 *    an in-place variant of mclxSub. Like mclxSub, it can widen or narrow
 *    or partly change the domains. If some entries of the matrix
 *    are conflicting with the new domain definitions, those entries
 *    will be removed.
 *    The domain arguments are assimilated; don't use them anymore.
 *    It is allowed to pass NULL arguments, meaning that the corresponding
 *    domain is not changed.
*/

void mclxChangeDomains
(  mclx* mx
,  mclv* dom_cols
,  mclv* dom_rows
)  ;


/*    Return the union of all blocks defined by columns in dom.
 *    If the columns of dom induce a partition, this will be
 *    a block diagonal matrix.
 *    The domains of the returned matrix are both equal
 *    to the row domain of the dom matrix.
*/

mclx*  mclxBlocks
(  const mclx*     mx
,  const mclx*     dom
)  ;

mclx*  mclxBlocks2
(  const mclx*     mx
,  const mclx*     domain
)  ;


/*    Change the row domain of a matrix. If some entries of the old domain
 *    are not present in the new domain, the corresponding entries will be
 *    removed from the matrix.
 *    The domain argument is assimilated; don't use it anymore.
*/

void mclxChangeRDomain
(  mclx* mx
,  mclv* domain
)  ;


/*    Change the column domain of a matrix. If some entries of the old domain
 *    are not present in the new domain, the corresponding columns will be
 *    removed from the matrix.
 *    The domain argument is assimilated; don't use it anymore.
*/

void mclxChangeCDomain
(  mclx* mx
,  mclv* domain      /* fixme; check consistency, increasing order */
)  ;


double  mclxSelectValues
(  mclx*  mx
,  double      *lft        /* NULL for turning of lft comparison        */
,  double      *rgt        /* NULL for turning of rgt comparison        */
,  mcxbits     equate      /*  0,1, or 2 of { MCLX_EQT_GT, MCLX_EQT_LT } [1] */
)  ;
                           /*  [1] Default, LQ and/or GQ are assumed    */


mclx* mclxIdentity
(  mclv* vec
)  ;


mclx* mclxCopy
(  const mclx*        mx
)  ;


void mclxFree
(  mclx**    mx
)  ;


void mclxScaleDiag
(  mclx* mx
,  double fac
)  ;


mclx* mclxDiag
(  mclv* vec
)  ;


mclx* mclxConstDiag
(  mclv* vec
,  double  c
)  ;


void mclxMakeSparse
(  mclx*    mtx
,  int      min
,  int      max
)  ;


void mclxMakeStochastic
(  mclx*    mx
)  ;


mclx* mclxTranspose
(  const mclx*   src
)  ;


void mclxMakeCharacteristic
(  mclx*     mtx
)  ;

long mclxNrofEntries
(  const mclx*        m
)  ;

double mclxMass
(  const mclx*  m
)  ;



/* MCL_VECTOR_SPARSE or MCL_VECTOR_COMPLETE */

mclv* mclxColNums
(  const mclx*  m
,  double           (*f)(const mclv * vec)
,  mcxenum           mode
)  ;


mclx* mclxMax
(  const mclx*  m1
,  const mclx*  m2
)  ;


mclx* mclxMinus
(  const mclx*  m1
,  const mclx*  m2
)  ;


mclx* mclxAdd
(  const mclx*  m1
,  const mclx*  m2
)  ;


void mclxAddTranspose
(  mclx* mx
,  double diagweight
)  ;


mclx* mclxHadamard
(  const mclx*  m1
,  const mclx*  m2
)  ;


void mclxInflate
(  mclx*  mx
,  double                power
)  ;


/* result has col domain: the union of col domains
 * and row domain: the union of row domains
*/

mclx* mclxBinary
(  const mclx*  m1
,  const mclx*  m2
,  double (*operation)(pval, pval)
)  ;


/* inline merge; m1 is modified and returned.
 * result has
 *    col domain: the union of col domains
 *    row domain: the union of row domains
*/

mclx* mclxMerge
(  mclx* m1
,  const mclx* m2
,  double (*operation)(pval, pval)
) ;


size_t mclxNEntries
(  const mclx*     mx
)  ;


double mclxMaxValue
(  const mclx*  m
)  ;


/* mode one of
 *    MCL_VECTOR_COMPLETE or
 *    MCL_VECTOR_SPARSE
*/

mclv* mclxColSums
(  const mclx*  m
,  mcxenum           mode
)  ;

mclv* mclxDiagValues
(  const mclx*  m
,  mcxenum           mode
)  ;


mclv* mclxColSizes
(  const mclx*  m
,  mcxenum           mode
)  ;


int mclxGetVectorOffset
(  const mclx*  mx
,  long              vid
,  mcxOnFail         ON_FAIL
,  long              offset
)  ;


mclv* mclxGetVector
(  const mclx*  mx
,  long              vid
,  mcxOnFail         ON_FAIL
,  const mclv*  offset
)  ;


mclv* mclxGetNextVector
(  const mclx*  mx
,  long              vid
,  mcxOnFail         ON_FAIL
,  const mclv*  offset
)  ;


/* vids-column association is disrupted! */

void  mclxColumnsRealign
(  mclx*        m
,  int              (*cmp)(const void* vec1, const void* vec2)
)  ;


/* These can be used to map or permute domains.
 *
 * Permutation or mapping of a matrix can also be achieved using matrix
 * multiplication.  These two methods do in-place modification.
*/

mcxstatus mclxMapRows
(  mclx   *mx
,  mclx  *map
)  ;


mcxstatus mclxMapCols
(  mclx  *mx
,  mclx  *map
)  ;



/*************************************
 * *
 **
 *
*/

void mclxAdjustLoops
(  mclx*    mx
,  double (*op)(mclv* vec, long r, void* data)
,  void* data
)  ;


/*************************************/


mclx* mclxMakeMap
(  mclv*  dom_cols
,  mclv*  new_dom_cols
)  ;



/* Shortest-Simple-Paths between x and y.
 *
 * -  Nodes must be different.
 * -  Will fail mysteriously or rudely on directed graphs.
 * -  Returns participating nodes including x and y as a vector.
 * -  Optionally sets the associated track matrix in trackpp.
*/

mclv* mclxSSPxy
(  const mclx* graph
,  long x
,  long y
,  mclx** trackpp
)  ;


/* All nodes participating in all shortest simple paths between all
 *       nodes in domain.
 * -  Nodes must be different.
 * -  Will fail mysteriously or rudely on directed graphs.
 * -  Current implemementation is not run-time efficient.
*/

mclv* mclxSSPd
(  const mclx* graph
,  const mclv* set
)  ;


/* TODO move mcxsub disc implementation to this file.
*/

/* return union of columns with vid in dom.
 * fixme fixme fixme *VERY* ugly hack:
 *   idx -1 denotes cols
 *   idx -2 denotes rows
*/

mclv* mclxUnionv
(  const mclx* mx
,  const mclv* dom
,  mclv* dst
)  ;


#define MCLX_WEED_COLS 1
#define MCLX_WEED_ROWS 2
#define MCLX_WEED_GRAPH 4

void mclxWeed
(  mclx* mx
,  mcxbits bits
)  ;



/* operation's second arg should be double */

void mclxUnary
(  mclx*  m1
,  double  (*operation)(pval, void*)
,  void*  arg           /* double*  */
)  ;

long mclxUnaryList
(  mclx*    mx
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)  ;


#endif

