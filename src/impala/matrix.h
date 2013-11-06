/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006, 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* TODO
 *    Unify select routines, mclgMakeSparse.
 *    Unify mclxColNums as mclxColSelect ?
 *
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
 *    positive, but this is not mandated. A val member that is zero
 *    will cause removal of the ivp it is contained in when a vector
 *    is submitted to mclvUnary or mclvBinary. These routines are indirectly
 *    called by many of the routines in this interface.
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
 *    integrity; i.e. when adding a column to a matrix. mclxAccomodate
 *    can be used for such a transformation.
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
#define N_COLS(mx) ((dim) ((mx)->dom_cols->n_ivps * 1))
#define N_ROWS(mx) ((dim) ((mx)->dom_rows->n_ivps * 1))

#define MAXID_COLS(mx) (N_COLS(mx) ? ((mx)->dom_cols->ivps)[N_COLS(mx)-1].idx : 0)
#define MAXID_ROWS(mx) (N_ROWS(mx) ? ((mx)->dom_rows->ivps)[N_ROWS(mx)-1].idx : 0)

#define mclxRowCanonical(mx) MCLV_IS_CANONICAL((mx)->dom_rows)
#define mclxColCanonical(mx) MCLV_IS_CANONICAL((mx)->dom_cols)

#define mclxDomCanonical(mx)     (mclxRowCanonical(mx) && mclxColCanonical(mx))
#define mclxGraphCanonical(mx)   (mclxDomCanonical(mx) && N_ROWS(mx) == N_COLS(mx))

#define mclxIsGraph(mx) (  mclxGraphCanonical(mx)           \
                        || mcldEquate((mx)->dom_rows, (mx)->dom_cols, MCLD_EQT_EQUAL)  \
                        )


#define     MCLX_PRODUCE_DOMTREE       1 <<  0     /* */
#define     MCLX_PRODUCE_DOMSTACK      1 <<  1
#define     MCLX_REQUIRE_DOMTREE       1 <<  2
#define     MCLX_REQUIRE_DOMSTACK      1 <<  3
#define     MCLX_REQUIRE_NESTED        1 <<  4      /* with stack format */
#define     MCLX_ENSURE_ROOT           1 <<  5
#define     MCLX_REQUIRE_PARTITION     1 <<  6
#define     MCLX_REQUIRE_CANONICALC    1 <<  7
#define     MCLX_REQUIRE_CANONICALR    1 <<  8
#define     MCLX_REQUIRE_CANONICAL     (MCLX_REQUIRE_CANONICALC | MCLX_REQUIRE_CANONICALR)
#define     MCLX_REQUIRE_GRAPH         1 <<  9
#define     MCLX_MODE_UNUSED           1 << 10


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


/*    All arguments remain owned by caller.  The select domains need not be
 *    subdomains of the matrix domains.  They can take any form; mclxSub will
 *    do the appropriate thing (keeping those entries that are in line with the
 *    new domains).

 *    The domains of the new matrix will always be clones of the selection
 *    domains as supplied by caller, even if those are not subdomains of the
 *    corresponding matrix domains.  In this respect, the routine acts just
 *    like mclxChangeDomains, which is effectively an in-place variant of
 *    mclxSub.

 *    If the select domains are wider than the matrix domains, nothing will be
 *    removed and only the domains are changed.
 *
 *    If a select domain is NULL it is interpreted as an empty vector.  NOTE
 *    This is different from mclxSubRead[x] in io.h
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
 *    to the col domain of the mx matrix.
*/

mclx*  mclxBlocks
(  const mclx*     mx
,  const mclx*     dom
)  ;

mclx*  mclxBlocks2
(  const mclx*     mx
,  const mclx*     domain
)  ;

mclMatrix*  mclxBlocksC
(  const mclMatrix*     mx
,  const mclMatrix*     domain
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


void mclxMakeStochastic
(  mclx*    mx
)  ;


      /* Selects on column domain, but removes from row domain as well.
       * Use this for undirected graphs.
       * Returns the domain that was kept (note that domains of m
       * are not touched)
      */
mclv* mclgMakeSparse
(  mclMatrix* m
,  dim        sel_gq
,  dim        sel_lq
)  ;


mclx* mclxTranspose
(  const mclx*    m
)  ;


void mclxMakeCharacteristic
(  mclx*          m
)  ;

dim mclxNrofEntries
(  const mclx*    m
)  ;

double mclxMass
(  const mclx*    m
)  ;



   /* mode:
    *    MCL_VECTOR_SPARSE
    *    MCL_VECTOR_COMPLETE
   */

mclv* mclxColNums
(  const mclx*    m
,  double        (*f_cb)(const mclv * vec)
,  mcxenum        mode
)  ;


   /* Returns a domain vector only including those (column) indices
    * for which f_cb returned nonzero.
   */
mclv* mclxColSelect
(  const mclx*    m
,  double        (*f_cb)(const mclv*, void*)
,  void*          arg_cb
)  ;


dim mclxSelectUpper
(  mclx*  mx
)  ;

dim mclxSelectLower
(  mclx*  mx
)  ;


mclx* mclxMax
(  const mclx*    m1
,  const mclx*    m2
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
,  double (*f_cb)(pval, pval)
)  ;


/* Inline merge; m1 is modified and returned.  domains of m1 are *not* changed.
 * any entries in m2 not in the domains of m1 is discarded.
 *
 * Note: this uses mclvBinary internally, which is inefficient for meet/diff
 * type operations.
*/

void mclxMerge
(  mclx* m1
,  const mclx* m2
,  double (*f_cb)(pval, pval)
) ;


/* inline add; m1 is modified.
 * domains of m1 are *not* changed.
 * any entries in m2 not in the domains of m1
 * is discarded.
*/

void mclxAddto
(  mclMatrix* m1
,  const mclMatrix* m2
)  ;


dim mclxNEntries
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
(  const mclx*    m
,  mcxenum        mode
)  ;

mclv* mclxDiagValues
(  const mclx*    m
,  mcxenum        mode
)  ;


mclv* mclxColSizes
(  const mclx*    m
,  mcxenum        mode
)  ;


ofs mclxGetVectorOffset
(  const mclx*    mx
,  long           vid
,  mcxOnFail      ON_FAIL
,  ofs            offset
)  ;


mclv* mclxGetVector
(  const mclx*    mx
,  long           vid
,  mcxOnFail      ON_FAIL
,  const mclv*    offset
)  ;


mclv* mclxGetNextVector
(  const mclx*    mx
,  long           vid
,  mcxOnFail      ON_FAIL
,  const mclv*    offset
)  ;


/* vids-column association is disrupted! */

void  mclxColumnsRealign
(  mclx*          m
,  int          (*cmp)(const void* vec1, const void* vec2)
)  ;



/* ************************************************************************* */

/* Map (necessarily) preserves ordering
 * Use e.g. to canonify domains.
*/

mclx* mclxMakeMap
(  mclv*  dom_cols
,  mclv*  new_dom_cols
)  ;


/* Uses scratch in SCRATCH_READY mode
*/

mcxbool mclxMapTest
(  mclx*    map
)  ;

#define mclxMapInvert(map) (mclxMapTest(map) ? mclxTranspose(map) : NULL)

/* dom should be subset of map->dom_cols.
 *    if successful
 *    -  returns the image of dom under map as an ordered set.
 *    -  *ar_dompp contains the image of dom.
*/

mclv* mclxMapVectorPermute
(  mclv  *dom
,  mclx  *map
,  mclpAR** ar_dompp
)  ;


/* These can be used to map domains (and the corresponding
 * matrix entries accordingly).
 *
 * Mapping of a matrix can also be achieved using matrix
 * multiplication. These two methods do in-place modification.
 * In matrix algrebra the mapping of a matrix is known as 
 * a matrix permutation.
*/

mcxstatus mclxMapRows
(  mclx     *mx
,  mclx     *map
)  ;


mcxstatus mclxMapCols
(  mclx     *mx
,  mclx     *map
)  ;


/* ************************************************************************* */



/*************************************
 * *
 **
 * returns number of columns that had zero entries.
*/

dim mclxAdjustLoops
(  mclx*    mx
,  double (*op)(mclv* vec, long r, void* data)
,  void* data
)  ;


double mclxLoopCBremove
(  mclv  *vec
,  long r
,  void*data
)  ;


double mclxLoopCBifEmpty
(  mclv  *vec
,  long r
,  void*data
)  ;


/* returns 1.0 if vector has no entries */

double mclxLoopCBmax
(  mclv  *vec
,  long r
,  void*data
)  ;


/*************************************/


#define MCLX_SCRUB_COLS 1
#define MCLX_SCRUB_ROWS 2
#define MCLX_SCRUB_GRAPH 4

void mclxScrub
(  mclx* mx
,  mcxbits bits
)  ;



/* operation's second arg should be double */

void mclxUnary
(  mclx*  m1
,  double  (*f_cb)(pval, void*)
,  void*  arg           /* double*  */
)  ;

dim mclxUnaryList
(  mclx*    mx
,  mclpAR*  ar       /* idx: MCLX_UNARY_mode, val: arg */
)  ;



enum
{  SCRATCH_READY
,  SCRATCH_BUSY
,  SCRATCH_UPDATE
,  SCRATCH_DIRTY
}  ;

/* return union of columns with vid in dom.
 *
 * SCRATCH_READY:    mx->dom_rows is characteristic, will be used and reset.
 * SCRATCH_BUSY:     do not use scratch.
 * SCRATCH_DIRTY:    reset and use scratch, then leave it dirty.
 * SCRATCH_UPDATE:   ignore nodes in scratch, add unseen nodes, do not reset.
 *
 *
 * NOTE --- relevant for flood code that uses SCRATCH_UPDATE ---
 *    mclgUnionv does not update scratch for the indices in dom_cols.
 *    An example is clew/clm.c/clmComponents. The initial annotation,
 *    if needed, is provided by mclgUnionvInitNode or mclgUnionvInitList
 *
 * NOTE --- added mclgUnionv2 variants, that pass in a scratch area
 *    of their own (so that mclgUnionv2 can be used in a thread-safe way).
 *    This means the interface looks a bit kludgy by virtue of duplication. 
 *    This observation may lead to more changes, possibly mclgUnionv2
 *    will take over entirely (at the moment, mclgUnionv dispatches
 *    to mclgUnionv2).
 *
*/

#define MCLG_UNIONV_SENTINEL 1.5

#define  mclgUnionvInitNode(mx, node) \
         mclvInsertIdx(mx->dom_rows, node, MCLG_UNIONV_SENTINEL)

#define  mclgUnionvInitList(mx, vec) \
         mclvUpdateMeet(mx->dom_rows, vec, flt1p5)

#define  mclgUnionvResetList(mx, vec) \
         mclvUpdateMeet(mx->dom_rows, vec, flt1p0)

#define  mclgUnionvReset(mx) \
         mclvMakeCharacteristic(mx->dom_rows)

mclv* mclgUnionv
(  mclx* mx                   /*  mx->dom_rows used as scratch area     */
,  const mclv* dom_cols       /*  take union over these columns in mx   */
,  const mclv* restrict       /*  only consider row entries in restrict */
,  mcxenum SCRATCH_STATUS     /*  if ready also returned ready          */
,  mclv* dst
)  ;


#define  mclgUnionvInitNode2(vec, node) \
         mclvInsertIdx(vec, node, MCLG_UNIONV_SENTINEL)

#define  mclgUnionvInitList2(vec, list) \
         mclvUpdateMeet(vec, list, flt1p5)

#define  mclgUnionvResetList2(vec, list) \
         mclvUpdateMeet(vec, list, flt1p0)

#define  mclgUnionvReset2(vec) \
         mclvMakeCharacteristic(vec)

mclv* mclgUnionv2             /*  This one has a const matrix argument, additional scratch */
(  const mclx* mx
,  const mclv* dom_cols
,  const mclv* restrict
,  mcxenum SCRATCH_STATUS
,  mclv* dst
,  mclv* scratch
)  ;



#if 0
typedef struct
{  mclx*       mx
;  mclx*       mxtp
;  void*       usr
;
}  mclxAnnot   ;


/* TODO
 *    put mclxStackPush callback in the struct
*/

typedef struct
{  mclxAnnot*  level
;  dim         n_level
;  dim         n_alloc  
;  mcxTing*    name
;
}  mclxStack   ;


void mclxStackInit
(  mclxStack*  stack
,  const char* str
)  ;


mcxstatus mclxStackPush
(  mclxStack*  stack
,  mclx*       mx
,  mcxstatus   (*cb1) (mclx* mx, void* cb_data)
,  void*       cb1_data
,  mcxstatus   (*cb2) (mclx* left, mclx* right, void* cb_data)
,  void*       cb2_data
)  ;


mcxstatus mclxStackRead
(  mcxIO*      xf
,  mclxStack*  stack
,  dim         n_max
,  mcxstatus   (*cb1) (mclx* mx, void* cb_data)
,  void*       cb1_data
,  mcxstatus   (*cb2) (mclx* left, mclx* right, void* cb_data)
,  void*       cb2_data
)  ;


mcxstatus mclxStackReadArgv
(  const char* argv[]
,  int         argc
,  mclxStack*  stack
,  mcxstatus   (*cb1) (mclx* mx, void* cb_data)
,  void*       cb1_data
,  mcxstatus   (*cb2) (mclx* left, mclx* right, void* cb_data)
,  void*       cb2_data
)  ;


mcxstatus mclxStackCBcone
(  mclx* left
,  mclx* right
,  void* cb_data
)  ;


mcxstatus mclxStackCBstack
(  mclx* left
,  mclx* right
,  void* cb_data
)  ;


mcxstatus mclxStackConify
(  mclxStack* st
)  ;


mcxstatus mclxStackUnconify
(  mclxStack* st
)  ;


void mclxStackReverse
(  mclxStack*  stack
)  ;


mcxstatus mclxStackWrite
(  mcxIO*      xf
,  mclxStack*  stack
,  int         valdigits
,  mcxOnFail   ON_FAIL
)  ;
#endif


#endif

