/* (c) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


/* TODO (?)
 * count number of insertions overwrites lookups deletions?
 * perhaps optional logging struct.
 *
 * better bucket fill statistics.
*/

#include <math.h>
#include <stdio.h>

#include "hash.h"
#include "minmax.h"
#include "types.h"
#include "ting.h"
#include "err.h"
#include "gralloc.h"
#include "list.h"


/* check all mcxLink, mcxHLink
 * check all ->kv
 * make alien links work with native links (be careful)
*/


            /* the distribution of bit counts over all 32-bit keys */
int promilles[32] = 
{  0,  0,  0,  0,  0,  0,  0,  0
,  2,  7, 15, 30, 53, 81,110,131
,140,131,110, 81, 53, 30, 15,  7
,  2,  0,  0,  0,  0,  0,  0,  0
}  ;


typedef struct bucket
{  mcxLink*       base
;
}  mcx_bucket     ;


struct mcxHash
{  int         n_buckets      /* 2^n_bits                         */
;  mcx_bucket *buckets
;  int         mask           /* == buckets-1                     */
;  int         n_bits         /* length of mask                   */
;  float       load
;  int         n_entries
;  int         options
;  int         (*cmp) (const void *a, const void *b)
;  u32         (*hash) (const void *a)

;  mcxLink*    src_link
;  mcxGrim*    src_kv
;
}  ;


void* mcx_bucket_init
(  void*  buck
)
   {  ((mcx_bucket*) buck)->base = NULL
   ;  return NULL
;  }


mcxKV* mcx_hash_kv
(  mcxHash* hash
,  void*    key
)
   {  mcxKV* kv = mcxGrimGet(hash->src_kv)
   ;  if (!kv)
      return NULL
   ;  kv->key = key
   ;  kv->val = NULL
   ;  return kv
;  }


void bitprint
(  u32   key
,  FILE* fp
)  ;

int bitcount
(  u32   key
)  ;


mcxHash* mcxHashNew
(  int         n_buckets
,  u32         (*hash)(const void *a)
,  int         (*cmp) (const void *a, const void *b)
)
   {  mcxHash  *h
   ;  mcxbool ok = FALSE
      
   ;  int n_bits   =  0

   ;  if (n_buckets <= 0)
      {  mcxErr("mcxHashNew strange", "alloc request %d", n_buckets)
      ;  n_buckets = 2
   ;  }

      if (!(h = mcxAlloc(sizeof(mcxHash), RETURN_ON_FAIL)))
      return NULL

   ;  h->mask           =  --n_buckets

   ;  while(n_buckets)
      {  h->mask       |=  n_buckets
      ;  n_buckets    >>=  1
      ;  n_bits++
   ;  }

      h->load           =  0.5
   ;  h->n_entries      =  0
   ;  h->n_bits         =  n_bits
   ;  h->n_buckets      =  n_buckets = (1 << n_bits)
   ;  h->cmp            =  cmp
   ;  h->hash           =  hash
   ;  h->options        =  MCX_HASH_OPT_DEFAULTS

   ;  h->src_link       =  NULL
   ;  h->src_kv         =  NULL

   ;  while (1)
      {  if
         (!(h->src_link = mcxLinkNew(h->n_buckets, NULL, MCX_GRIM_ARITHMETIC)))
         break

      ;  if
         ( !(  h->src_kv
         =     mcxGrimNew(sizeof(mcxKV), h->n_buckets, MCX_GRIM_ARITHMETIC)
         )  )
         break

      ;  if
         (! (  h->buckets
            =  mcxNAlloc
               (  h->n_buckets
               ,  sizeof(mcx_bucket)
               ,  mcx_bucket_init
               ,  RETURN_ON_FAIL
         )  )  )
         break

      ;  ok = TRUE
      ;  break
   ;  }

      if (!ok)
      {  mcxLinkFree(&h->src_link, NULL)
      ;  mcxGrimFree(&h->src_kv)
      ;  mcxFree(h)
      ;  return NULL
   ;  }

      return h
;  }



void mcxHashGetSettings
(  mcxHash*          hash
,  mcxHashSettings*  settings
)
   {  settings->n_buckets  =  hash->n_buckets
   ;  settings->n_bits     =  hash->n_bits
   ;  settings->mask       =  hash->mask
   ;  settings->load       =  hash->load
   ;  settings->n_entries  =  hash->n_entries
   ;  settings->options    =  hash->options
;  }


int mcxLinkSize
(  mcxLink*        link
)
   {  int   s        =  0
   ;  while(link)
         link = link->next
      ,  s++
   ;  return(s)
;  }


void mcxHashStats
(  FILE*       fp
,  mcxHash*    h
)
   {  int      buckets  =  h->n_buckets
   ;  int      buckets_used   =  0
   ;  float    ctr      =  0.0
   ;  float    cb       =  0.0
   ;  int      max      =  0
   ;  int      entries  =  0
   ;  const    char* me =  "mcxHashStats"
   ;  mcxGrim* grimlink =  mcxLinkGrim(h->src_link)

   ;  int      j, k, distr[32]
   ;  mcx_bucket  *buck

   ;  for (j=0;j<32;j++)
      distr[j] = 0

   ;  for (buck=h->buckets; buck<h->buckets + h->n_buckets; buck++)
      {  int   i        =  mcxLinkSize(buck->base)
      ;  mcxLink* this  =  buck->base

      ;  if (i)
         {  buckets_used++
         ;  entries    +=  i
         ;  ctr        +=  (float) i * i
         ;  cb         +=  (float) i * i * i
         ;  max         =  MAX(max, i)
      ;  }

         while(this && this->val)
         {  u32   u     =  (h->hash)(((mcxKV*) this->val)->key)
         ;  int   ct    =  bitcount(u)
         ;  this        =  this->next
         ;  distr[ct]++
      ;  }
      }

      ctr               =  ctr / MAX(1, entries)
   ;  cb                =  sqrt(cb  / MAX(1, entries))

   ;  if (buckets && buckets_used)
         mcxTellf
         (  fp
         ,  me
         ,  "%4.2f bucket usage (%ld available, %ld used, %ld entries)"
         ,  (double) ((double) buckets_used) / buckets
         ,  (long) buckets
         ,  (long) buckets_used
         ,  (long) entries
         )
      ,  mcxTellf
         (  fp
         ,  me
         ,  "bucket average: %.2f, center: %.2f, cube: %.2f, max: %ld"
         ,  (double) entries / ((double) buckets_used)
         ,  (double) ctr
         ,  (double) cb
         ,  (long) max
         )

   ;  mcxTellf(fp, me, "bit distribution (promilles):")
   ;  fprintf
      (  fp
      ,  "  %-37s   %s\n"
      ,  "Current bit distribution"
      ,  "Ideally random distribution"
      )
   ;  for (k=0;k<4;k++)
      {  for (j=k*8;j<(k+1)*8;j++)
         fprintf(fp, "%3.0f ",  (1000 * (float)distr[j]) / entries)
      ;  fprintf(fp, "        ");
      ;  for (j=k*8;j<(k+1)*8;j++)
         fprintf(fp, "%3d ",  promilles[j])
      ;  fprintf(fp, "\n")
   ;  }
      mcxTellf
      (  fp
      ,  me
      ,  "sanity check: %ld / %ld"
      ,  (long) mcxGrimCount(grimlink) - 1
      ,  (long) mcxGrimCount(h->src_kv)
      )  
   ;  mcxTellf(fp, me, "done")
;  }


void mcxHashSetOpts
(  mcxHash*    h
,  double      load
,  int         options
)
   {  if (options >= 0)
      h->options |= options
  /* fixme; are there states in which either of these can be corrupting ? */
   ;  h->load  =  load
;  }


void mcxHashFree
(  mcxHash**   hpp
,  void        freekey(void* key)
,  void        freeval(void* key)
)
   {  mcxHash* h        =  *hpp
   ;  mcx_bucket* buck  =  h->buckets
   ;  int i             =  h->n_buckets

   ;  while(i--)
      {  mcxLink* link  =  (buck++)->base

      ;  while(link)
         {  mcxKV* kv  = link->val
         ;  if (kv)
            {  if (freekey)
               freekey(&(kv->key))
            ;  if (freeval)
               freeval(&(kv->val))
         ;  }
            link = link->next
      ;  }
      }

      mcxLinkFree(&h->src_link, NULL)
   ;  mcxGrimFree(&h->src_kv)
   ;  mcxFree(h->buckets)
   ;  mcxFree(h)
   ;  *hpp = NULL
;  }


#define MCX_HASH_DOUBLING MCX_HASH_OPT_UNUSED


mcxLink* mcx_bucket_search
(  mcxHash*          h
,  mcx_bucket*       buck
,  void*             ob
,  mcxmode           ACTION
,  int*              delta
)
   {  int      c     =  1
   ;  mcxLink* link  =  buck->base, *prev = NULL, *new

   ;  *delta =  0

   ;  while
      (   link
      &&  ((mcxKV*) link->val)->key
      &&  (c = h->cmp(ob, ((mcxKV*) link->val)->key)) > 0
      ) 
         prev = link
      ,  link = link->next

   ;  if (!c && ACTION == MCX_DATUM_DELETE)
      {  if (buck->base == link)
         buck->base = link->next
      ;  *delta = -1
      ;  return mcxLinkDelete(link)
   ;  }
      else if (!link || c < 0)
      {  if (ACTION == MCX_DATUM_FIND || ACTION == MCX_DATUM_DELETE)
         return NULL

      ;  else if (ACTION == MCX_DATUM_INSERT)
         {  if (!buck->base)
            new = buck->base = mcxLinkSpawn(h->src_link, NULL)
         ;  else if (link == buck->base)
               new = mcxLinkBefore(buck->base, NULL)
            ,  buck->base = new
         ;  else
            new = mcxLinkAfter(prev, NULL)
         ;  *delta = 1
         ;  return new
      ;  }
      }
      return link
;  }


mcxstatus mcxHashDouble
(  mcxHash* h
)  ;


mcxKV* mcxHashSearch
(  void*       key
,  mcxHash*    h
,  mcxmode     ACTION
)
   {  mcxKV    *fkv     =  NULL     /* found KV */
   ;  u32      hashval  =  (h->hash)(key) & h->mask
   ;  mcx_bucket *buck  =  (h->buckets)+hashval
   ;  int      delta    =  0

   ;  mcxLink *link     =  mcx_bucket_search(h, buck, key, ACTION, &delta)

   ;  if (!(h->options & MCX_HASH_DOUBLING))
      h->n_entries += delta

   ;  if (!link)
      return NULL    /* not found or memory shortage (yes todo errno etc) */

   ;  if (ACTION == MCX_DATUM_INSERT && !link->val)
      link->val = mcx_hash_kv(h, key)

   ;  fkv = (mcxKV*) link->val

   ;  if (ACTION == MCX_DATUM_DELETE)
      mcxLinkDelete(link)        /* warning; fkv must be left alone */

   ;  if
      (  h->load * h->n_buckets < h->n_entries
      && !(h->options & (MCX_HASH_OPT_CONSTANT | MCX_HASH_DOUBLING))
      && mcxHashDouble(h)
      )
      return NULL

   ;  return fkv
;  }


enum
{  ARRAY_OF_KEY
,  ARRAY_OF_KV
}  ;


void** mcxHashArray
(  mcxHash*    hash
,  int*        n_entries
,  int       (*cmp)(const void*, const void*)
,  mcxbits     opts                    /* unused yet */
,  mcxenum     mode
)
   {  mcxHashWalk walk
   ;  void** obs   =  mcxAlloc(sizeof(void*) * hash->n_entries, RETURN_ON_FAIL)
   ;  int i = 0
   ;  mcxKV* kv

   ;  mcxHashWalkInit(hash, &walk)

   ;  if (!obs)
      return NULL

   ;  while ((kv = mcxHashWalkStep(&walk)))
      {  if (i >= hash->n_entries)
         {  mcxErr("mcxHashKeys PANIC", "inconsistent state (n_entries %d)", hash->n_entries)
         ;  break
      ;  }
         obs[i] = mode == ARRAY_OF_KEY ? kv->key : kv
      ;  i++
   ;  }
      if (i != hash->n_entries)
      mcxErr("mcxHashKeys PANIC", "inconsistent state (n_entries %d)", hash->n_entries)

   ;  qsort(obs, i, sizeof(void*), cmp)

   ;  *n_entries = i
   ;  return obs
;  }



void** mcxHashKeys
(  mcxHash*    hash
,  int*        n_entries
,  int       (*cmp)(const void*, const void*)
,  mcxbits     opts                    /* unused yet */
)
   {  return mcxHashArray(hash, n_entries, cmp, opts, ARRAY_OF_KEY)
;  }



void** mcxHashKVs
(  mcxHash*    hash
,  int*        n_entries
,  int       (*cmp)(const void*, const void*)
,  mcxbits     opts                    /* unused yet */
)
   {  return mcxHashArray(hash, n_entries, cmp, opts, ARRAY_OF_KV)
;  }



mcxKV* mcxHashWalkStep
(  mcxHashWalk  *walk
)
   {  mcxLink* step  =  walk->link
   
   ;  while (!step && ++walk->i_bucket < walk->hash->n_buckets)
      step = (walk->hash->buckets+walk->i_bucket)->base

   ;  if (step)
      {  walk->link = step->next
      ;  return step->val
   ;  }
      return NULL
;  }


void mcxHashWalkInit
(  mcxHash  *h
,  mcxHashWalk* walk
)
   {  walk->hash     =  h

   ;  if (!h || !h->buckets)
      return

   ;  walk->i_bucket =  0
   ;  walk->link     =  (h->buckets+0)->base
   ;  return
;  }


void mcxHashWalkFree
(  mcxHashWalk  **walkpp
)
   {  mcxFree(*walkpp)
   ;  *walkpp =  NULL
;  }


mcxHash* mcxHashMerge
(  mcxHash*    h1
,  mcxHash*    h2
,  mcxHash*    hd    /* hash destination */
,  void*       merge(void* val1, void* val2)
)
   {  mcxHash* ha[2] /* hash array */
   ;  mcxHash* h
   ;  int i

   ;  if (!h1 || !h2)
      mcxDie(1, "mcxHashMerge FATAL", "clone functionality not yet supported")

      /*
       * fixme/note I am comparing fie pointers here, is that ok?
      */
   ;  if (h1->hash != h2->hash || h1->cmp != h2->cmp)
      mcxErr("mcxHashMerge WARNING", "non matching hash or cmp fie")

   ;  if (merge)
      mcxErr("mcxHashMerge WARNING", "merge functionality not yet supported")

   ;  hd =  hd
            ?  hd
            :  mcxHashNew
               (  h1->n_entries + h2->n_entries
               ,  h1->hash
               ,  h1->cmp
               )

   ;  if (!hd)
      return NULL

   ;  ha[0] = h1
   ;  ha[1] = h2

   ;  for (i=0;i<2;i++)
      {  h = ha[i]
      ;  if (h != hd)
         {  mcx_bucket* buck

         ;  for (buck = h->buckets; buck<h->buckets + h->n_buckets; buck++)
            {  mcxLink* this =  buck->base

            ;  while(this && this->val)
               {  mcxKV* kv
               ;  if
                  ( !(  kv
                     =  mcxHashSearch
                        (((mcxKV*) this->val)->key, hd, MCX_DATUM_INSERT)
                  )  )
                  return NULL
/*  note/fixme: cannot free hd, don't have key/val free functions */
               ;  if (!kv->val)
                  kv->val = ((mcxKV*)this->val)->val
               ;  this = this->next
            ;  }
            }
         }
      }

      return hd
;  }


mcxstatus mcxHashDouble
(  mcxHash* h
)
   {  mcx_bucket* ole_bucket  =  h->buckets
   ;  mcx_bucket* ole_buckets =  h->buckets
   ;  int i          =  h->n_buckets
   ;  int n_fail     =  0

   ;  if (h->options & MCX_HASH_DOUBLING)     /* called before */
      return STATUS_FAIL

   ;  h->options |= MCX_HASH_DOUBLING

   ;  h->n_buckets  *=  2
   ;  h->n_bits     +=  1
   ;  h->mask        =  (h->mask << 1) | 1

   ;  if
      (! (  h->buckets
         =  mcxNAlloc
            (  h->n_buckets
            ,  sizeof(mcx_bucket)
            ,  mcx_bucket_init
            ,  RETURN_ON_FAIL
      )  )  )
      {  h->options ^= MCX_HASH_DOUBLING
      ;  return STATUS_FAIL
   ;  }

      while(i--)
      {  mcxLink* this     =  ole_bucket->base

      ;  while(this)
         {  mcxKV* kv = this->val
         ;  mcxLink* next = this->next
         ;  if (kv)
            {  void* val   =  kv->val
            ;  void* key   =  kv->key

            ;  mcxGrimLet(h->src_kv, kv)  /* will be used immediately */
            ;  mcxLinkDelete(this)        /* idem */

            ;  kv = mcxHashSearch(key, h, MCX_DATUM_INSERT)
            ;  if (kv->key != key || kv->val)
               n_fail++
            ;  else
               kv->val = val
         ;  }
            else
            n_fail++
         ;  this = next
      ;  }
         ole_bucket++
   ;  }

      if (n_fail)
      mcxErr
      (  "mcxHashDouble PANIC"
      ,  "<%ld> reinsertion failures in hash with <%ld> entries"
      ,  (long) n_fail
      ,  (long) h->n_entries
      )

   ;  mcxFree(ole_buckets)
   ;  h->options ^= MCX_HASH_DOUBLING
   ;  return STATUS_OK
;  }


#define BJmix(a,b,c)             \
{                                \
  a -= b; a -= c; a ^= (c>>13);  \
  b -= c; b -= a; b ^= (a<< 8);  \
  c -= a; c -= b; c ^= (b>>13);  \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16);  \
  c -= a; c -= b; c ^= (b>> 5);  \
  a -= b; a -= c; a ^= (c>> 3);  \
  b -= c; b -= a; b ^= (a<<10);  \
  c -= a; c -= b; c ^= (b>>15);  \
}


/*
 * Thomas Wang says Robert Jenkins says this is a good integer hash function:
 *unsigned int inthash(unsigned int key)
 *{
 *   key += (key << 12);
 *   key ^= (key >> 22);
 *   key += (key << 4);
 *   key ^= (key >> 9);
 *   key += (key << 10);
 *   key ^= (key >> 2);
 *   key += (key << 7);
 *   key ^= (key >> 12);
 *   return key;
 *}
*/

                        /* created by Bob Jenkins */
u32      mcxBJhash
(  register const void*    key
,  register u32            len
)
   {  register u32      a, b, c, l
   ;  char* k     =  (char *) key

   ;  l           =  len
   ;  a = b       =  0x9e3779b9
   ;  c           =  0xabcdef01

   ;  while (l >= 12)
      {
         a += k[0] + (k[1]<<8) + (k[2]<<16) + (k[3]<<24)
      ;  b += k[4] + (k[5]<<8) + (k[6]<<16) + (k[7]<<24)
      ;  c += k[8] + (k[9]<<8) + (k[10]<<16)+ (k[11]<<24)
      ;  BJmix(a,b,c)
      ;  k += 12
      ;  l -= 12
   ;  }

      c += len
   ;  switch(l)         /* all the case statements fall through */
      {
         case 11: c+= k[10]<<24
      ;  case 10: c+= k[9]<<16
      ;  case 9 : c+= k[8]<<8
                        /* the first byte of c is reserved for the length */
      ;  case 8 : b+= k[7]<<24
      ;  case 7 : b+= k[6]<<16
      ;  case 6 : b+= k[5]<<8
      ;  case 5 : b+= k[4]
      ;  case 4 : a+= k[3]<<24
      ;  case 3 : a+= k[2]<<16
      ;  case 2 : a+= k[1]<<8
      ;  case 1 : a+= k[0]
                        /* case 0: nothing left to add */
   ;  }

   ;  BJmix(a,b,c)
   ;  return c
;  }


                        /* created by Chris Torek */
u32   mcxCThash
(  const void *key
,  u32        len
)
#define ctHASH4a   h = (h << 5) - h + *k++;
#define ctHASH4b   h = (h << 5) + h + *k++;
#define ctHASH4 ctHASH4b

   {   u32 h               =  0
   ;   unsigned char *k    =  (unsigned char*) key

   ;   if (len > 0)
       {   unsigned loop = (len + 8 - 1) >> 3
       ;   switch (len & (8 - 1))
           {
               case 0:
                  do
                  {        /* All fall through */
                             ctHASH4
                     case 7: ctHASH4
                     case 6: ctHASH4
                     case 5: ctHASH4
                     case 4: ctHASH4
                     case 3: ctHASH4
                     case 2: ctHASH4
                     case 1: ctHASH4
                  }
                  while (--loop)
               ;
           }
       }
   ;  return h
;  }


/* All 3 hash fies below play on a similar theme.  Interesting: as long as only
 * << >> and ^ are used, a hash function does a partial homogeneous fill of all
 * 2^k different strings of length k built out of two distinct characters --
 * not all buckets need be used. E.g. for k=15, such a hash function might fill
 * 2^13 buckets with 4 entries each, or it might fill 2^10 buckets with 32
 * entries each.  This was observed, not proven.
*/

u32   mcxSvDhash
(  const void        *key
,  u32               len
)
   {  u32   h     =  0x7cabd53e /* 0x7cabd53e */
   ;  char* k     =  (char *) key

   ;  h           =  0x0180244a

   ;  while (len--)
      {  u32  g   =  *k
      ;  u32  gc  =  0xff ^ g
      ;  u32  hc  =  0xffffffff

      ;  hc      ^=  h
      ;  h        =  (  (h << 2) +  h +  (h >> 3))
                  ^  ( (g << 25) + (gc << 18) + (g << 11) + (g << 5) + g )
      ;  k++
   ;  }

   ;  return h
;  }


                        /* created by me */
u32   mcxSvD2hash
(  const void        *key
,  u32               len
)
   {  u32   h     =  0x7cabd53e /* 0x7cabd53e */
   ;  char* k     =  (char *) key

   ;  while (len--)
      {  u32  g   =  *k
      ;  u32  gc  =  0xff ^ g

      ;  h        =  (  (h << 3) ^ h ^ (h >> 5) )
                  ^  ( (g << 25) ^ (gc << 18) ^ (g << 11) ^ (gc << 5) ^ g )
      ;  k++
   ;  }

   ;  return h
;  }


                        /* created by me */
u32   mcxSvD1hash
(  const void        *key
,  u32               len
)
   {  u32   h     =  0xeca96537
   ;  char* k     =  (char *) key

   ;  while (len--)
      {  u32  g   =  *k
      ;  h        =  (  (h << 3)  ^ h ^ (h >> 5) )
                  ^  ( (g << 21) ^  (g << 12)   ^ (g << 5) ^ g )
      ;  k++
   ;  }

   ;  return h
;  }


                        /* created by Daniel Phillips */
u32   mcxDPhash
(  const void        *key
,  u32               len
)
   {  u32   h0    =  0x12a3fe2d
   ,        h1    =  0x37abe8f9
   ;  char* k     =  (char *) key

   ;  while (len--)
      {
         u32 h    =  h1 + (h0 ^ (*k++ * 71523))
      ;  h1       =  h0
      ;  h0       =  h
   ;  }
      return h0
;  }


                           /* "GNU Emacs" hash (from m4) */
u32 mcxGEhash
(  const void* key
,  u32         len
)
   {  char* k  =  (char*) key
   ;  u32 hash =  0
   ;  int t
   ;  while (len--)
      {  if ((t = *k++) >= 0140)
         t -= 40
      ;  hash = ((hash << 3) + (hash >> 28) + t)
   ;  }
      return hash
;  }



                           /* Berkely Database hash */
u32 mcxBDBhash
(  const void *key
,  u32        len
)
   {  char* k     =  (char *) key
   ;  u32   hash  =  0

   ;  while (len--)
      {  hash = *k++ + (hash << 6) + (hash << 16) - hash
   ;  }
      return hash
;  }


                           /* One at a time hash, Bob Jenkins/Colin Plumb */
u32 mcxOAThash
(  const void *key
,  u32        len
)
   {  char* k     =  (char *) key
   ;  u32   hash  =  0

   ;  while (len--)
      {  hash += *k++
      ;  hash += (hash << 10)
      ;  hash ^= (hash >> 6)
   ;  }

      hash += (hash << 3);
   ;  hash ^= (hash >> 11);
   ;  hash += (hash << 15);
   ;  return hash
;  }


                           /* by Dan Bernstein  */
u32 mcxDJBhash
(  const void *key
,  u32        len
)
   {  char* k     =  (char *) key
   ;  u32   hash  =  5381
   ;  while (len--)
      {  hash = *k++ + (hash << 5) + hash
   ;  }
      return hash
;  }


                           /* UNIX ELF hash */
u32 mcxELFhash
(  const void *key
,  u32        len
)
   {  char* k     =  (char *) key
   ;  u32   hash  =  0
   ;  u32   g

   ;  while (len--)
      {  hash = *k++ + (hash << 4)
      ;  if ((g = (hash & 0xF0000000)))
         hash ^= g >> 24

      ;  hash &= ~g
   ;  }
      return hash
;  }



u32 mcxStrHash
(  const void* s
)
   {  int   l  =  strlen((char*) s)
   ;  return(mcxDPhash(s, l))
;  }



void bitprint
(  u32   key
,  FILE* fp
)
   {  do
      {  fputc(key & 1 ? '1' : '0',  fp)
   ;  }  while
         ((key = key >> 1))
;  }


int bitcount
(  u32   key
)
   {  int ct = key & 1
   ;  while ((key = key >> 1))
      {  if (key & 1)
         ct++
   ;  }               
   ;  return ct
;  }

