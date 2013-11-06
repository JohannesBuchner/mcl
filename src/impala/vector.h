/*   (C) Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


#ifndef impala_vector_h
#define impala_vector_h

#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include "pval.h"
#include "ivp.h"
#include "iface.h"

#include "util/types.h"


enum
{  KBAR_SELECT_SMALL =  10000
,  KBAR_SELECT_LARGE
}  ;


enum
{  MCL_VECTOR_COMPLETE = 1
,  MCL_VECTOR_SPARSE
}  ;


typedef struct
{  int         n_ivps
;  long        vid      /* vector id */
;  double      val      /* for misc purposes */
;  mclIvp*     ivps
;
}  mclVector   ;

#define mclv mclVector

#define MCLV_SIZE(vec)   ((vec)->n_ivps)
#define MCLV_MAXID(vec)  (MCLV_SIZE(vec) ? ((vec)->ivps)[MCLV_SIZE(vec)-1].idx : 0)
#define MCLV_MINID(vec)  (MCLV_SIZE(vec) ? ((vec)->ivps)[0].idx : 0)


/* below are used for reading raw input */

#define  MCLV_WARN_REPEAT_ENTRIES  1
#define  MCLV_WARN_REPEAT_VECTORS  2
#define  MCLV_WARN_REPEAT (MCLV_WARN_REPEAT_ENTRIES|MCLV_WARN_REPEAT_VECTORS)


/* INTEGRITY
 *    The idx members of the ivp array need to be nonnegative and ascending.
 *    The val members are usually positive, but this is not mandated.
 *
 *    The setting n_ivps == 0, ivps == NULL is obviously legal.
 *
 * NOTE
 *    All unary and binary operations remove resulting entries that are zero.
 *
 *    This means it is dangerous to use those operations on vectors
 *    representing value lists (i.e. columns from value tables) that contain
 *    zero-valued entries - one might want to combine two different such
 *    vectors simply by using mclvAdd, but the zero-valued entries will be
 *    removed.
 *
 *    There is currently no easy way around this. See also ../README .
*/


#define  MCLV_CHECK_DEFAULT      0
#define  MCLV_CHECK_POSITIVE     1
#define  MCLV_CHECK_NONZERO      2
#define  MCLV_CHECK_NONNEGATIVE  (MCLV_CHECK_POSITIVE | MCLV_CHECK_NONZERO)

mcxstatus mclvCheck
(  const mclVector*  vec
,  long              min         /* inclusive */
,  long              max         /* inclusive */
,  mcxbits           modes
,  mcxOnFail         ON_FAIL
)  ;


mclVector* mclvInit
(  mclVector*     vec
)  ;

void* mclvInit_v
(  void*     vec
)  ;


mclVector* mclvNew
(  mclp*    ivps
,  int      n_ivps
)  ;


mclVector* mclvRenew
(  mclv*    dst
,  mclp*    ivps
,  int      n_ivps
)  ;


/* This bugger leaves vec in inconsistent state unless it is shrinking
 * Inconsistent meaning: ivp with idx set to -1.
 *
 * vec argument can be NULL.
*/

mclVector* mclvResize
(  mclVector*     vec
,  int            n_ivps
)  ;


mclVector* mclvCopy
(  mclVector*        dst
,  const mclVector*  src
)  ;


mclVector* mclvClone
(  const mclVector*  src
)  ;


void mclvFree
(  mclVector**    vec_p
)  ;

void mclvRelease
(  mclVector*        vec
)  ;


mclv* mclvFromIvps
(  mclv* dst
,  mclp* ivps
,  int n_ivps
)  ;


mclv* mclvFromIvps_x
(  mclv* dst
,  mclp* ivps
,  int n_ivps
,  mcxbits sortbits
,  mcxbits warnbits
,  void (*ivpmerge)(void* ivp1, const void* ivp2)
,  double (*fltbinary)(pval val1, pval val2)
)  ;


/*
 *    If mul > 0, it will map indices i to mul*i + shift. 
 *    If mul < 0, it will map indices i to (i+shift)/mul.
 *    Be careful to check the validity of the result (mul < 0)!
 *    shift may assume all values.
*/

mclVector* mclvMap
(  mclVector*     dst
,  long           mul
,  long           shift
,  mclVector*     src
)  ;


mclVector* mclvCanonical
(  mclVector*     dst_vec
,  int            n_ivps
,  double         val
)  ;


/* fixme: mask_mode interface ? */

mclVector* mclvMaskedCopy
(  mclVector*        dst
,  const mclVector*  src
,  const mclVector*  msk
,  int               mask_mode
)  ;


void mclvSort
(  mclVector*     vec
,  int           (*mclpCmp)(const void*, const void*)
)  ;


void mclvSortAscVal
(  mclVector*     vec
)  ;


void mclvSortDescVal
(  mclVector*     vec
)  ;


int mclvUniqueIdx
(  mclVector*     vec
,  void (*merge)(void* ivp1, const void* ivp2)
)  ;


/* sorts vectors and discards duplicates (based on idx) */

void mclvCleanse
(  mclVector*  vec
)  ;


void mclvSelectHighest
(  mclVector*     vec
,  int            max_n_ivps
)  ;


/*
 * ignore:
 *    when searching k large elements, consider only those elements that are <
 *    ignore. case mode = KBAR_SELECT_LARGE when searching k small elements,
 *    consider only those elements that are >= ignore. case mode =
 *    KBAR_SELECT_SMALL
*/

double mclvKBar
(  mclVector      *vec
,  int            max_n_ivps
,  double         ignore
,  int            mode
)  ;


/* TODO
 * can the implementation be (significantly) improved?
*/

long  mclvTop
(  mclVector      *vec
,  double         psum
,  double*        maxptr
,  double*        minptr
,  mcxbool        select
)  ;


double mclvSelectValues
(  mclVector*     vec
,  double         *lft        /* NULL for turning of lft comparison  */
,  double         *rgt        /* NULL for turning of rgt comparison  */
,  mcxbits        equate      /*  0,1,or 2 of { MCLX_GQ,  MCLX_LQ }  */
)  ;


/* this one should be a wee bit more efficient than
 * mclSelectValues - it should be measured sometime though.
 * mclvSelectGqBar is often called by mcl - hence the
 * special purpose routine. No siblings for lt, lq, gt,
 * you have to call mclvSelectValues for those.
*/

double mclvSelectGqBar
(  mclVector*     vec
,  double         bar
)  ;


void mclvUnary
(  mclVector*     vec
,  double        (*operation)(pval val, void* argument)
,  void*          argument
)  ;


int mclvCountGiven
(  mclVector*     src
,  mcxbool       (*operation)(mclIvp* ivp, void* arg)
,  void*          arg
)  ;


/*
 *    src may equal dst.
 *    all ivps are copied/retained for which operation
 *    returns true.
*/

mclVector* mclvCopyGiven
(  mclVector*     dst
,  mclVector*     src
,  mcxbool        (*operation)(mclIvp* ivp, void* arg)
,  void*          arg
,  int            size_upper_bound  /* 0 for I-don't-know */
)  ;


mclVector* mclvAdd
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*  dst
)  ;  


void mclvScale
(  mclVector*     vec
,  double         fac
)  ;


double mclvNormalize
(  mclVector*     vec
)  ;


double mclvInflate
(  mclVector*     vec
,  double         power
)  ;

void mclvMakeConstant
(  mclVector*     vec
,  double         val
)  ;

void mclvZeroValues
(  mclVector*  vec
)  ;  

void mclvMakeCharacteristic
(  mclVector*     vec
)  ;


void mclvHdp
(  mclVector*     vec
,  double         pow
)  ;


long mclvHighestIdx
(  mclVector*     vec
)  ;


void mclvRemoveIdx
(  mclVector*     vec
,  long           idx
)  ;


mclVector* mclvInsertIdx
(  mclVector*     vec
,  long           idx
,  double         val
)  ;

   /* todo: make

      mclVector* mclvInsertIdcs
      (  mclVector*  vec
      ,  int         k
      ,  ...
      )  ;

      and possibly

      mclVector* mclvAddtoIdcs
      (  mclVector*  vec
      ,  int         k
      ,  ...
      )  ;

   */


mclVector* mclvBinary
(  const mclVector*  src1
,  const mclVector*  src2
,  mclVector*        dst
,  double           (*operation)(pval val1, pval val2)
)  ;


double mclvIn
(  const mclVector*        lft
,  const mclVector*        rgt
)  ;


mclVector* mcldMinus
(  const mclVector*  vecl
,  const mclVector*  vecr
,  mclVector*        dst      /* value from flt (naturally) */
)  ;


mclVector* mcldMerge
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*        dst      /* values in lft prefered over rgt */
)  ;


mclVector* mcldMeet
(  const mclVector*  lft
,  const mclVector*  rgt
,  mclVector*        dst      /* values in dst are from lft */
)  ;

mcxbool mcldIsCanonical
(  mclVector* vec
)  ;

               /*    l: size of left difference
                *    r: size of right difference
                *    m: size of meet.
               */
enum
{  MCLD_EQ_SUPER           /*    r == 0                     */
,  MCLD_EQ_SUB             /*    l == 0                     */
,  MCLD_EQ_EQUAL           /*    r == 0 && l == 0           */
,  MCLD_EQ_DISJOINT        /*    m == 0                     */
,  MCLD_EQ_MEET            /*    m != 0                     */
,  MCLD_EQ_TRISPHERE       /*    l != 0 && m != 0 && r != 0 */ 
,  MCLD_EQ_LDIFF           /*    l != 0                     */
,  MCLD_EQ_RDIFF           /*    r != 0                     */
}  ;

mcxbool mcldEquate
(  const mclVector* dom1
,  const mclVector* dom2
,  mcxenum    mode
)  ;

#define MCLD_EQUAL(d1, d2) mcldEquate(d1, d2, MCLD_EQ_EQUAL)
#define MCLD_SUPER(d1, d2) mcldEquate(d1, d2, MCLD_EQ_SUPER)
#define MCLD_SUB(d1, d2) mcldEquate(d1, d2, MCLD_EQ_SUB)
#define MCLD_DISJOINT(d1, d2) mcldEquate(d1, d2, MCLD_EQ_DISJOINT)


void mcldCountParts
(  const mclVector* dom1
,  const mclVector* dom2
,  int*       ldif
,  int*       meet
,  int*       rdif
)  ;


#define MCLD_CT_LDIFF 1
#define MCLD_CT_MEET  2
#define MCLD_CT_RDIFF 4
#define MCLD_CT_JOIN  7

int mcldCountSet
(  mclVector*  dom1
,  mclVector*  dom2
,  mcxbits     parts
)  ;


double mclvSum
(  const mclVector*   vec
)  ;


double mclvSelf
(  const mclVector*   vec
)  ;


double mclvSize
(  const mclVector*   vec
)  ;

double mclvSelf
(  const mclVector* vec
)  ;  


double mclvPowSum
(  const mclVector*   vec
,  double             power
)  ;


double mclvNorm
(  const mclVector*   vec
,  double             power
)  ;


double mclvMaxValue
(  const mclVector*   vec
)  ;


double mclvIdxVal
(  const mclVector*   vec
,  long         idx
,  int*         p_offset
)  ;


mclIvp* mclvGetIvp
(  const mclVector*  vec
,  long              idx
,  mclIvp*           offset
)  ;


int mclvGetIvpOffset
(  const mclVector*   vec
,  long         idx
,  long         offset
)  ;


double mclvAdjustForce
(  mclv* vec
,  long  r
,  void* data
)  ;


double mclvAdjustDiscard
(  mclv* vec
,  long  r
,  void* data
)  ;


/* looks first at size, then at lexicographic ordering, ignores vid. */

int mclvSizeCmp
(  const void*  p1
,  const void*  p2
)  ;

int mclvSizeRevCmp
(  const void*  p1
,  const void*  p2
)  ;


/* looks at lexicographic ordering, ignores vid. */

int mclvLexCmp
(  const void*  p1
,  const void*  p2
)  ;


/* looks at vid only. */

int mclvVidCmp
(  const void*  p1
,  const void*  p2
)  ;


int mclvSumCmp
(  const void*  p1
,  const void*  p2
)  ;


int mclvSumRevCmp
(  const void*  p1
,  const void*  p2
)  ;


#endif

