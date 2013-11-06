/*   (C) Copyright 2007 Stijn van Dongen
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 3 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/


/* TODO
 * add ensureRoot routine.
 *
 * read_skeleton fails in unlimited cat mode. Only trailing whitespace
 * is allowed, but read_skeleton leaves the body.
 * Conceivably read_skeleton should flush the body if more than
 * one matrix is being read.
*/


#include "cat.h"
#include "util/err.h"
#include "util/compile.h"

#include "io.h"
#include "compose.h"
#include "tab.h"

static const char* us = "mclxCat";

mcxstatus mclxCatPush
(  mclxCat*    stack
,  mclx*       mx
,  mcxstatus   (*cb1) (mclx* mx, void* cb_data)
,  void*       cb1_data
,  mcxstatus   (*cb2) (mclx* left, mclx* right, void* cb_data)
,  void*       cb2_data
)
   {  if
      (  stack->n_level
      && cb2
      && cb2(stack->level[stack->n_level-1].mx, mx, cb2_data)
      )
      {  mcxErr
         (  "mclxCatPush"
         ,  "stack <%s> chain error at level %d"
         ,  stack->name->str
         ,  (int) stack->n_level
         )
      ;  return STATUS_FAIL
   ;  }

      if (cb1 && cb1(mx, cb1_data))
      {  mcxErr
         (  "mclxCatPush"
         ,  "stack <%s> matrix error at level %d"
         ,  stack->name->str
         ,  (int) stack->n_level
         )
      ;  return STATUS_FAIL
   ;  }

      if
      (  !stack->level
      || (stack->n_level >= stack->n_alloc)
      )
      {  dim n_alloc = 5 + 1.5 * stack->n_alloc
      ;  mclxAnnot* level2
         =  mcxRealloc
            (  stack->level
            ,  sizeof stack->level[0] * n_alloc
            ,  RETURN_ON_FAIL
            )
      ;  if (!level2)
         return STATUS_FAIL
      ;  stack->level = level2
      ;  stack->n_alloc = n_alloc
   ;  }

      stack->level[stack->n_level].mx     =  mx
   ;  stack->level[stack->n_level].mxtp   =  NULL
   ;  stack->level[stack->n_level].usr    =  NULL
   ;  stack->n_level++
   ;  return STATUS_OK
;  }


void mclxCatTransposeAll
(  mclxCat* cat
)
   {  dim l
   ;  for (l=0;l<cat->n_level;l++)
      if (!cat->level[l].mxtp)
      cat->level[l].mxtp = mclxTranspose(cat->level[l].mx)
;  }


void mclxCatInit
(  mclxCat*  stack
,  const char* str
)
   {  stack->name =  mcxTingNew(str)
   ;  stack->n_level = 0
   ;  stack->n_alloc = 0
   ;  stack->level = NULL
;  }
 

mcxstatus mclxCatConify
(  mclxCat* st
)
   {  dim i
   ;  if (st->n_level <= 1)
      return STATUS_OK
   ;  for (i=st->n_level-1;i>0;i--)
      {  mclx* cltp = mclxTranspose(st->level[i-1].mx)
      ;  mclx* clcontracted = mclxCompose(cltp, st->level[i].mx, 0)
      ;  mclxFree(&cltp)
      ;  mclxFree(&st->level[i].mx)

      ;  mclxMakeCharacteristic(clcontracted)
      ;  st->level[i].mx = clcontracted
   ;  }
      return STATUS_OK
;  }


mcxstatus mclxCatUnconify
(  mclxCat* st
)
   {  dim i
   ;  mcxstatus status = STATUS_OK
   ;  for (i=0;i<st->n_level-1;i++)
      {  mclx* clprojected = mclxCompose(st->level[i].mx, st->level[i+1].mx, 0)
      ;  if
         (  mclxCBdomTree
            (  st->level[i].mx
            ,  st->level[i+1].mx
            ,  NULL
            )
         )
         {  mcxErr
            (  "mclxCatUnconify warning"
            ,  "stack <%s> domain inconsistency at level %d-%d"
            ,  st->name->str
            ,  (int) i
            ,  (int) (i+1)
            )
         ;  status = STATUS_FAIL
      ;  }

         mclxFree(&st->level[i+1].mx)
      ;  st->level[i+1].mx = clprojected
   ;  }
      return status
;  }


mcxstatus mclxCBdomTree
(  mclx* left
,  mclx* right
,  void* cb_data_unused cpl__unused
)
   {  return
         MCLD_EQUAL(left->dom_cols, right->dom_rows)
      ?  STATUS_OK
      :  STATUS_FAIL
;  }


mcxstatus mclxCBdomStack
(  mclx* left
,  mclx* right
,  void* cb_data_unused cpl__unused
)
   {  return
         mcldEquate(left->dom_rows, right->dom_rows, MCLD_EQT_EQUAL)
      ?  STATUS_OK
      :  STATUS_FAIL
;  }


mcxstatus mclxCBunary
(  mclx* mx
,  void* cb_data
)
   {  dim o, m, e
   ;  mcxbits bits = *((mcxbits*) cb_data)

   ;  if
      (  bits & MCLX_REQUIRE_CANONICALR
      && !mclxRowCanonical(mx)
      )
      return STATUS_FAIL

   ;  if
      (  bits & MCLX_REQUIRE_CANONICALC
      && !mclxColCanonical(mx)
      )
      return STATUS_FAIL

   ;  if
      (  bits & MCLX_REQUIRE_GRAPH
      && !mclxIsGraph(mx)
      )
      return STATUS_FAIL

   ;  if
      (  bits & MCLX_REQUIRE_PARTITION
      && clmEnstrict(mx, &o, &m, &e, ENSTRICT_REPORT_ONLY)
      )
      return STATUS_FAIL

   ;  return STATUS_OK
;  }


void mclxCatReverse
(  mclxCat*  stack
)
   {  dim i
   ;  for (i=0;i<stack->n_level/2;i++)
      {  mclxAnnot annot = stack->level[i]
      ;  stack->level[i] = stack->level[stack->n_level-i-1]
      ;  stack->level[stack->n_level-i-1] = annot
   ;  }
   }


mcxstatus mclxCatWrite
(  mcxIO*      xf
,  mclxCat*  stack
,  int         valdigits
,  mcxOnFail   ON_FAIL
)
   {  dim i
   ;  if (mcxIOtestOpen(xf, ON_FAIL))
      return STATUS_FAIL
   ;  for (i=0;i<stack->n_level;i++)
      if (mclxWrite(stack->level[i].mx, xf, valdigits, ON_FAIL))
      return STATUS_FAIL
   ;  return STATUS_OK
;  }


/* fixme: need mclxCatIsCone
 *
*/

struct newicky
{  mcxTing* node
;
}  ;


void* newicky_init
(  void* nkp
)
   {  struct newicky* nk = nkp
   ;  nk->node = NULL
   ;  return nkp
;  }


   /* For each node, count the branch length, i.e.
    * number of trivial nodes right beneath it.
    *
    * hierverder: change to compute the number of trivial nodes right above it.
   */
static void compute_branch_length
(  mclxCat* cat
,  dim      lev
,  dim      v
,  double   value
)
   {  mclv* vec = cat->level[lev].mx->cols+v
   ;  mclv* usrvec = cat->level[lev].usr
   ;  double new_value = vec->n_ivps == 1 ? value + 1.0 : 1.5
   ;  dim i

   ;  usrvec->ivps[v].val = value

   ;  if (lev > 0)
      for (i=0;i<vec->n_ivps;i++)
      compute_branch_length(cat, lev-1, vec->ivps[i].idx, new_value)
;  }



   /* For each node, count the number of trivial nodes
    * in the path to root.
   */
static void compute_trivial_count
(  mclxCat* cat
,  dim      lev
,  dim      v
,  double   value
)
   {  mclv* vec = cat->level[lev].mx->cols+v
   ;  double delta = vec->n_ivps == 1 ? 1.0 : 0.0
   ;  dim i

   ;  vec->val = value

   ;  if (lev > 0)
      for (i=0;i<vec->n_ivps;i++)
      compute_trivial_count(cat, lev-1, vec->ivps[i].idx, value+delta)
;  }


void compute_branch_factors
(  mclxCat* cat
,  mcxbits  bits
)
   {  dim v
   ;  mclx* mx
   ;  if (!cat->n_level)
      return

   ;  if
      (  bits & MCLX_NEWICK_NOINDENT
      && bits & MCLX_NEWICK_NONUM
      )
      return

   ;  mx = cat->level[cat->n_level-1].mx
   ;  for (v=0;v<N_COLS(mx);v++)
      {  if (!(bits & MCLX_NEWICK_NOINDENT))
         compute_trivial_count(cat, cat->n_level-1, v, 0.5)
      ;  if (!(bits & MCLX_NEWICK_NONUM))
         compute_branch_length(cat, cat->n_level-1, v, 1.5)
   ;  }
;  }


/* TODO
 * smarter string building: assemble the bits.
 * and then print them rather than repeated assembling/printing.
 *
 * sort according to node size.
 *
 * make sure that cat does in fact encode a tree.
*/

mcxTing* mclxCatNewick
(  mclxCat*  cat
,  mclTab*   tab
,  mcxbits   bits
)
   {  struct newicky* nklast, *nknext
   ;  mcxTing* tree = NULL
   ;  mcxTing* spacey = mcxTingKAppend(NULL, " ", cat->n_level)
   ;  const char* prefix = spacey->str
   ;  dim l, j, lm = cat->n_level            /* level-max */

   ;  if (!lm)
      return mcxTingNew("")
   ;  nklast
      =  mcxNAlloc
         (  N_COLS(cat->level[0].mx)
         ,  sizeof(struct newicky)
         ,  newicky_init
         ,  RETURN_ON_FAIL
         )

   ;  for (l=0;l<lm;l++)
      {  if (cat->level[l].usr)
         mcxDie(1, us, "user object defined")
      ;  else
         cat->level[l].usr = mclvCopy(NULL, cat->level[l].mx->dom_cols)
   ;  }
   
      compute_branch_factors(cat, bits)

   ;  for (j=0;j<N_COLS(cat->level[0].mx);j++)
      {  mclv* vec = cat->level[0].mx->cols+j
      ;  mcxTing* pivot
      ;  dim k

      ;  nklast[j].node = mcxTingNew("")

      ;  if (!vec->n_ivps)
         continue

      ;  pivot = nklast[j].node

      ;  if (!(bits & MCLX_NEWICK_NOINDENT))
         mcxTingPrint(pivot, "%s", prefix+1+((int)vec->val))      /* apparently denotes depth .. */

      ;  if (vec->n_ivps > 1 || !(bits & MCLX_NEWICK_NOPTHS))
         mcxTingNAppend(pivot, "(", 1)

      ;  if (tab)
         mcxTingPrintAfter(pivot, "%s", tab->labels[vec->ivps[0].idx])
      ;  else
         mcxTingPrintAfter(pivot, "%ld", (long) vec->ivps[0].idx)

      ;  for (k=1;k<vec->n_ivps;k++)
         {  if (tab)
            mcxTingPrintAfter(pivot, ",%s", tab->labels[vec->ivps[k].idx])
         ;  else
            mcxTingPrintAfter(pivot, ",%ld", (long) vec->ivps[k].idx)
      ;  }

         if (vec->n_ivps > 1 || !(bits & MCLX_NEWICK_NOPTHS))
         mcxTingNAppend(pivot, ")", 1)

      ;  if (!(bits & MCLX_NEWICK_NONUM))
         mcxTingPrintAfter
         (  pivot
         ,  ":%d"
         ,  (int) ((mclv*)cat->level[0].usr)->ivps[j].val
         )
   ;  }

      for (l=1;l<lm;l++)
      {  prefix = spacey->str+l
      ;  nknext
         =  mcxNAlloc
            (  N_COLS(cat->level[l].mx)
            ,  sizeof(struct newicky)
            ,  newicky_init
            ,  ENQUIRE_ON_FAIL
            )
      ;  for (j=0;j<N_COLS(cat->level[l].mx);j++)
         {  mclv* vec = cat->level[l].mx->cols+j
         ;  mcxTing* pivot
         ;  dim k
         ;  ofs idx

         ;  if (!vec->n_ivps)
            continue
         ;  if (vec->n_ivps == 1)
            {  ofs idx = vec->ivps[0].idx
            ;  if (!nklast[idx].node)
               mcxDie(1, "newick panic", "corruption 1")
            ;  nknext[j].node = nklast[idx].node
            ;  nklast[idx].node = NULL
            ;  continue
         ;  }

            idx = vec->ivps[0].idx

         ;  pivot = nknext[j].node = mcxTingEmpty(NULL, 20)

         ;  if (!(bits & MCLX_NEWICK_NOINDENT))
            mcxTingPrint(pivot, "%s", prefix+1+((int) vec->val))

         ;  mcxTingNAppend(pivot, "(", 1)

         ;  if (!(bits & MCLX_NEWICK_NONL))
            mcxTingNAppend(pivot, "\n", 1)

         ;  mcxTingPrintAfter
            (  pivot
            ,  "%s"
            ,  nklast[idx].node->str
            )
         ;  mcxTingFree(&nklast[idx].node)

         ;  for (k=1;k<vec->n_ivps;k++)
            {  ofs idx = vec->ivps[k].idx
;  if (!nklast[idx].node)
   mcxDie(1, "newick panic", "corruption 2 level %d vec %d idx %d", (int) l, (int) j, (int) idx)

            ;  mcxTingNAppend(pivot, ",", 1)

            ;  if (!(bits & MCLX_NEWICK_NONL))
               mcxTingNAppend(pivot, "\n", 1)

            ;  mcxTingPrintAfter(pivot, "%s", nklast[idx].node->str)
            ;  mcxTingFree(&nklast[idx].node)
         ;  }

            if (!(bits & MCLX_NEWICK_NONL))
            mcxTingNAppend(pivot, "\n", 1)

         ;  if (!(bits & MCLX_NEWICK_NOINDENT))
            mcxTingPrintAfter
            (pivot, "%s", prefix+1+((int) vec->val))

         ;  mcxTingNAppend(pivot, ")", 1)

         ;  if (!(bits & MCLX_NEWICK_NONUM))
            mcxTingPrintAfter
            (pivot, ":%d", (int) ((mclv*)cat->level[l].usr)->ivps[j].val)
      ;  }
         mcxFree(nklast)
      ;  nklast = nknext
   ;  }

      tree = nklast[0].node

   ;  for (l=0;l<lm;l++)
      {  mclv* vec = cat->level[l].usr
      ;  mclvFree(&vec)
      ;  cat->level[l].usr = NULL
   ;  }

      mcxFree(nklast)
   ;  return tree
;  }


/* Currently this fails if there is no matrix to be read.
 * allowing zero matrices *could* be an option.
 * Even more so in conjunction with MCLX_ENSURE_ROOT
*/

mcxstatus mclxCatRead
(  mcxIO*      xf
,  mclxCat*    st
,  dim         n_max
,  mclv*       base_dom_cols
,  mclv*       base_dom_rows
,  mcxbits     bits
)
   {  dim n_read = 0
   ;  mcxstatus status = STATUS_OK
   ;  mcxTing* line = mcxTingEmpty(NULL, 20)
   ;  const char* me = "mclxCatRead"
   ;  dim n_uncanon = 0          

   ;  mclx* mx = NULL
   ;  mcxbool have_stack = FALSE
   ;  mcxbool have_cone = FALSE
   ;  mcxbool stack_or_cone
      =     MCLX_PRODUCE_DOMTREE | MCLX_PRODUCE_DOMSTACK
         |  MCLX_REQUIRE_DOMTREE | MCLX_REQUIRE_DOMSTACK
   ;  const char* err = NULL

   ;  mclxCatInit(st, xf->fn->str)

   ;  while (status == STATUS_OK)
      {  dim o, m, e
      ;  status = STATUS_FAIL

      ;  if (bits & MCLX_READ_SKELETON)
         {  if (!(mx = mclxReadSkeleton(xf, bits & MCLX_REQUIRE_GRAPH, TRUE)))
            break
      ;  }
         else
         {  if (bits & MCLX_REQUIRE_GRAPH)
            {  if (!(mx = mclxReadx(xf, RETURN_ON_FAIL, MCLX_REQUIRE_GRAPH)))
               break
         ;  }
            else if (!(mx = mclxRead(xf, RETURN_ON_FAIL)))
            break
      ;  }
                           /* we required this for clusters; docme why!
                            * enstrict / map related ?
                           */
         if (stack_or_cone && !MCLV_IS_CANONICAL(mx->dom_cols) && ++n_uncanon == 2)
         {  mcxErr(me, "matrix indices not in canonical format")
         ;  mcxErr(me, "code path not tested!")
         ;  mcxErr(me, "you might experience bugs!")
         ;  mcxErr(me, "three exclamations for cargo cult programming!")
      ;  }

         if ((bits & MCLX_REQUIRE_CANONICALC) && !MCLV_IS_CANONICAL(mx->dom_cols))
         {  err = "column domain not canonical"
         ;  break
      ;  }

         if ((bits & MCLX_REQUIRE_CANONICALR) && !MCLV_IS_CANONICAL(mx->dom_rows))
         {  err = "row domain not canonical"
         ;  break
      ;  }

         if
         (  (bits & MCLX_REQUIRE_PARTITION)
         && clmEnstrict(mx, &o, &m, &e, ENSTRICT_REPORT_ONLY)
         )
         {  err = "not a partition"
         ;  break
      ;  }
                           /* check base if given
                           */
         if
         (  st->n_level == 0
         && (  (  base_dom_cols
               && !MCLD_EQUAL(base_dom_cols, mx->dom_cols)
               )
            || (  base_dom_rows
               && !MCLD_EQUAL(base_dom_rows, mx->dom_rows)
               )
            )
         )
         {  err = "base domain mismatch"
         ;  break
      ;  }

                           /* check stack/cone status
                           */
         else if (stack_or_cone && st->n_level >= 1)
         {  mclx* mxprev = st->level[st->n_level-1].mx
         ;  mcxbool see_stack = MCLD_EQUAL(mxprev->dom_rows, mx->dom_rows)
         ;  mcxbool see_cone  = MCLD_EQUAL(mxprev->dom_cols, mx->dom_rows)
         ;  if (!see_stack && !see_cone)
            {  err = "fish nor fowl"
            ;  break
         ;  }
            if (!have_stack && !have_cone)
            {  if (see_stack && see_cone)
               NOTHING              /* all clusters could be singletons */
            ;  else if (see_stack)
               have_stack = TRUE
            ;  else if (see_cone)
               have_cone = TRUE
         ;  }
            else if (have_cone)
            {  if (see_stack || (bits & MCLX_REQUIRE_DOMSTACK))
               {  err = "cone/stack violation"
               ;  break
            ;  }
            }
            else if (have_stack)
            {  if (see_cone || (bits & MCLX_REQUIRE_DOMTREE))
               {  err = "stack/cone violation"
               ;  break
            ;  }
            }

            if ((bits & MCLX_REQUIRE_NESTED) && have_stack)
            {  mclx* cing = clmContingency(mxprev, mx)
            ;  mcxbool ok = TRUE
            ;  dim j
            ;  for (j=0;j<N_COLS(cing);j++)
               if (cing->cols[j].n_ivps != 1)
               {  ok = FALSE
               ;  break
            ;  }
               mclxFree(&cing)
            ;  if (!ok)
               break
         ;  }
         }

      ;  if (mclxCatPush(st, mx, NULL, NULL, NULL, NULL))
         {  err = "no push!"
         ;  break
      ;  }

                     /* read trailing ')' so that EOF check later works */
         if (mclxIOformat(xf) == 'a')
         mcxIOreadLine(xf, line, MCX_READLINE_CHOMP)
      ;  mcxIOreset(xf)

      ;  status = STATUS_OK
      ;  if
         (  (n_max && ++n_read >= n_max)
         || EOF == mcxIOskipSpace(xf)
         )        /* skipSpace is funny for binary format, but it works */
         break
   ;  }

      mcxTingFree(&line)

   ;  if
      (  !status
      && (bits & MCLX_ENSURE_ROOT)
      && N_COLS(mx) != 1
      )
      {  mclx* root
         =  mclxCartesian
            (  mclvCanonical(NULL, 1, 1.0)
            ,  mclvCopy(NULL, mx->dom_cols)
            ,  1.0
            )
      ;  if (mclxCatPush(st, root, NULL, NULL, NULL, NULL))
            err = "no push!"
         ,  status = STATUS_FAIL
   ;  }

      if (status && st->n_level && st->level[st->n_level-1].mx != mx)
      mclxFree(&mx)

   ;  if (err)
      mcxErr(me, "%s at level %lu", err, (ulong) st->n_level)

   ;  if (!status && stack_or_cone)
      {  if (have_stack && (bits & MCLX_PRODUCE_DOMTREE))
         return mclxCatConify(st)
      ;  else if (have_cone && (bits & MCLX_PRODUCE_DOMSTACK))
         return mclxCatUnconify(st)
   ;  }

      return status
;  }



static void clm_split_overlap
(  mclx* cl
)
   {  dim n_entries     =  mclxNrofEntries(cl)
   ;  dim n_entries_old =  DIM_MAX
   ;  dim n_ite         =  0
   ;  mclv** clnew      =  mcxAlloc
                           (  N_ROWS(cl) * sizeof clnew[0]
                           ,  EXIT_ON_FAIL
                           )

   ;  while (n_entries > N_ROWS(cl))
      {  mclx* cltp = mclxTranspose(cl)
      ;  dim n_clnew = 0, i, n_empty = 0


      ;  if (n_entries >= n_entries_old)
         {  mcxErr("clm_split_overlap panic", "no decrease no sentinel")
         ;  mclxFree(&cltp)
         ;  break
      ;  }

                  /* for each node check its cluster count
                  */
         for (i=0;i<N_COLS(cltp);i++)
         {  mclv* cllist = cltp->cols+i
         ;  if (cllist->n_ivps > 1)
            {  dim j
            ;  mclv* meet = NULL, *cl0 = NULL

                  /* Compute meet of intersecting clusters
                  */
            ;  for (j=0;j<cllist->n_ivps;j++)
               {  cl0 = mclxGetVector
                  (  cl
                  ,  cllist->ivps[j].idx
                  ,  EXIT_ON_FAIL
                  ,  cl0
                  )
               ;  meet = meet ? mcldMeet(cl0, meet, meet) : mclvClone(cl0)
            ;  }

                  /* Get rid of the meet in each cluster
                  */
               cl0 = NULL
            ;  if (meet->n_ivps)
               {  for (j=0;j<cllist->n_ivps;j++)
                  {  cl0 = mclxGetVector
                     (  cl
                     ,  cllist->ivps[j].idx
                     ,  EXIT_ON_FAIL
                     ,  cl0
                     )
                  ;  if (cl0->n_ivps)
                     {  mcldMinus(cl0, meet, cl0)
                     ;  if (!cl0->n_ivps)
                        n_empty++
                  ;  }
               ;  }
                  clnew[n_clnew++] = meet
            ;  }
            }
         }
               /* WARNING: below may change N_COLS(cl)
                * AND makes cltp out of sync .....
               */
         if (n_empty)
         {  dim o, m, e
         ;  clmEnstrict
            (  cl
            ,  &o
            ,  &m
            ,  &e
            ,  ENSTRICT_KEEP_OVERLAP | ENSTRICT_LEAVE_MISSING
            )
         ;  if (n_empty != e)
            mcxDie(1, "clmEnstrict", "empty but not empty - what am I?")
      ;  }

               /*  ..... so free it right away.
               */
         mclxFree(&cltp)

      ;  if (n_clnew)
         {  dim n_clold = N_COLS(cl)
         ;  ofs clid = MAXID_COLS(cl) + 1
         ;  cl->cols
            =  mcxNRealloc
               (  cl->cols
               ,  n_clold + n_clnew
               ,  n_clold
               ,  sizeof(mclv)
               ,  mclvInit_v
               ,  EXIT_ON_FAIL
               )
         ;  for (i=n_clold;i<n_clold + n_clnew;i++)
            {  mclvCopy(cl->cols+i, clnew[i-n_clold])
            ;  mclvFree(&clnew[i-n_clold])
         ;  }
            mclvResize(cl->dom_cols, n_clold+ n_clnew)
         ;  for (i=n_clold;i<n_clold + n_clnew;i++)
               cl->dom_cols->ivps[i].idx = clid
            ,  cl->cols[i].vid = clid
            ,  clid++
      ;  }

         n_entries_old = n_entries
      ;  n_entries = mclxNrofEntries(cl)
      ;  n_ite++

      ;  mcxLog
         (  MCX_LOG_MODULE
         ,  "clmEnstrict"
         ,  "removed %lu overlap and added %lu clusters (removed %lu)"
         ,  (ulong) (n_entries_old - n_entries)
         ,  (ulong) n_clnew
         ,  (ulong) n_empty
         )
   ;  }
      mcxFree(clnew)
;  }



   /*
    *    Remove overlap
    *    Add missing entries
    *    Remove empty clusters
   */

dim  clmEnstrict
(  mclx*  cl
,  dim         *overlap
,  dim         *missing
,  dim         *empty
,  mcxbits     flags
)
   {  dim  c
   ;  const char* me =  "clmEnstrict"
   ;  double half = 0.5
   ;  mclv* ct   =  NULL
   ;  mcxbool remove_overlap
      =  !(flags & (ENSTRICT_SPLIT_OVERLAP | ENSTRICT_KEEP_OVERLAP))

   ;  *overlap       =  0
   ;  *missing       =  0
   ;  *empty         =  0

                     /* fixme: document reentry.
                      * clm_split_overlap may also call clmEnstrict
                     */
   ;  if (flags & ENSTRICT_SPLIT_OVERLAP)
      {  dim ome
      ;  BIT_OFF(flags, ENSTRICT_SPLIT_OVERLAP)
      ;  BIT_ON(flags, ENSTRICT_KEEP_OVERLAP)
      ;  ome = clmEnstrict(cl, overlap, missing, empty, flags)
      ;  if (overlap)
         clm_split_overlap(cl)
      ;  return ome
   ;  }

      ct = mclvClone(cl->dom_rows)
   ;  mclvMakeConstant(ct, 0.0)

   ;  for (c=0;c<N_COLS(cl);c++)
      {  mclIvp*  ivp      =  (cl->cols+c)->ivps
      ;  mclIvp*  ivpmax   =  ivp + (cl->cols+c)->n_ivps
      ;  dim      olap     =  0
      ;  long     l        =  -1

      ;  while(ivp< ivpmax)
         {  if ((l = mclvGetIvpOffset(cl->dom_rows, ivp->idx, l)) < 0)
               mcxErr
               (  me
               ,  "index <%ld> rank <%ld> not in domain for cluster <%ld>"
               ,  (long) ivp->idx
               ,  (long) (ivp - cl->cols[c].ivps)
               ,  (long) cl->cols[c].vid
               )
            ,  mcxExit(1)     /* fixme do not exit; create clustering */

                                         /* already seen (overlap) */
         ;  ct->ivps[l].val++

         ;  if (ct->ivps[l].val > 1.5)
            {  olap++
            ;  if (remove_overlap)
               ivp->val = -1.0
         ;  }

            ivp++
         ;  l++
      ;  }

         *overlap  += olap

      ;  if (remove_overlap)
         mclvSelectGqBar(cl->cols+c, 0.0)
   ;  }

      *missing = mclvCountGiven(ct, mclpGivenValLQ, &half) 

   ;  if (*missing && !(flags & ENSTRICT_LEAVE_MISSING))
      {  long  newvid  =  mclvHighestIdx(cl->dom_cols) + 1
      ;  dim cloldsize = N_COLS(cl), z

      ;  mclv* cladd   =  mclvRange(NULL, *missing, newvid, 1.0)
      ;  mclxAccomodate(cl, cladd, NULL)

      ;  for(z=0;z<ct->n_ivps;z++)
         {  if (!ct->ivps[z].val)
            {  mclvInsertIdx
               (  cl->cols+cloldsize
               ,  cl->dom_rows->ivps[z].idx
               ,  1.0
               )
            ;  cloldsize++ 
         ;  }
         }
     /*  NOW N_COLS(cl) has changed */
      }

      {  dim q  =  0

      ;  for (c=0;c<N_COLS(cl);c++)
         {  if (cl->cols[c].n_ivps > 0)
            {  if (c>q  && !(flags & ENSTRICT_KEEP_EMPTY))
               {  (cl->cols+q)->n_ivps    =  (cl->cols+c)->n_ivps
               ;  (cl->cols+q)->ivps      =  (cl->cols+c)->ivps
            ;  }
               q++
         ;  }
         }

      ;  *empty  =  N_COLS(cl) - q

      ;  if (q < N_COLS(cl) && !(flags & ENSTRICT_KEEP_EMPTY))
         {  cl->cols =     mcxRealloc
                           (  cl->cols
                           ,  q*sizeof(mclv)
                           ,  EXIT_ON_FAIL
                           )
         ;  mclvResize(cl->dom_cols, q)
                 /*  careful; this automatically effects N_COLS(cl) */
      ;  }
      }

      mclvFree(&ct)
   ;  return (*overlap + *missing + *empty)
;  }



mclx*  clmContingency
(  const mclx*  cla
,  const mclx*  clb
)
   {  mclx  *clbt  =  mclxTranspose(clb)
   ;  mclx  *ct   =  mclxCompose(clbt, cla, 0)
   ;  mclxFree(&clbt)
   ;  return ct
;  }


