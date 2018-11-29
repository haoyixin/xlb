#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_ring.h>

#include "c_hash.h"

#define FOR_EACH_BUCKET(CURRENT_BKT, START_BUCKET)                             \
  for (CURRENT_BKT = START_BUCKET; CURRENT_BKT != NULL;                        \
       CURRENT_BKT = CURRENT_BKT->next)

TAILQ_HEAD(c_hash_list, rte_tailq_entry);

static struct rte_tailq_elem c_hash_tailq = {
    .name = "C_HASH",
};
EAL_REGISTER_TAILQ(c_hash_tailq)

struct c_hash *c_hash_find_existing(const char *name) {
  struct c_hash *h = NULL;
  struct rte_tailq_entry *te;
  struct c_hash_list *hash_list;

  hash_list = RTE_TAILQ_CAST(c_hash_tailq.head, c_hash_list);

  rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
  TAILQ_FOREACH(te, hash_list, next) {
    h = (struct c_hash *)te->data;
    if (strncmp(name, h->name, C_HASH_NAMESIZE) == 0)
      break;
  }
  rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);

  if (te == NULL) {
    rte_errno = ENOENT;
    return NULL;
  }
  return h;
}

static inline struct c_hash_bucket *
c_hash_get_last_bkt(struct c_hash_bucket *lst_bkt) {
  while (lst_bkt->next != NULL)
    lst_bkt = lst_bkt->next;
  return lst_bkt;
}

void c_hash_set_cmp_func(struct c_hash *h, c_hash_cmp_eq_t func) {
  h->c_hash_custom_cmp_eq = func;
}

static inline int c_hash_cmp_eq(const void *key1, const void *key2,
                                const struct c_hash *h) {
  return h->c_hash_custom_cmp_eq(key1, key2, h->key_len);
}

/*
 * We use higher 16 bits of hash as the signature value stored in table.
 * We use the lower bits for the primary bucket
 * location. Then we XOR primary bucket location and the signature
 * to get the secondary bucket location. This is same as
 * proposed in Bin Fan, et al's paper
 * "MemC3: Compact and Concurrent MemCache with Dumber Caching and
 * Smarter Hashing". The benefit to use
 * XOR is that one could derive the alternative bucket location
 * by only using the current bucket location and the signature.
 */
static inline uint16_t c_get_short_sig(const c_hash_sig_t hash) {
  return hash >> 16;
}

static inline uint32_t c_get_prim_bucket_index(const struct c_hash *h,
                                               const c_hash_sig_t hash) {
  return hash & h->bucket_bitmask;
}

static inline uint32_t c_get_alt_bucket_index(const struct c_hash *h,
                                              uint32_t cur_bkt_idx,
                                              uint16_t sig) {
  return (cur_bkt_idx ^ sig) & h->bucket_bitmask;
}

struct c_hash *c_hash_create(const struct c_hash_parameters *params) {
  struct c_hash *h = NULL;
  struct rte_tailq_entry *te = NULL;
  struct c_hash_list *hash_list;
  struct rte_ring *r = NULL;
  char hash_name[C_HASH_NAMESIZE];
  void *k = NULL;
  void *buckets = NULL;
  char ring_name[RTE_RING_NAMESIZE];
  unsigned num_entry_slots;
  unsigned i;
  unsigned int no_free_on_del = 0;

  c_hash_function default_hash_func = (c_hash_function)rte_hash_crc;
  c_hash_cmp_eq_t default_cmp_func = (c_hash_cmp_eq_t)memcmp;

  hash_list = RTE_TAILQ_CAST(c_hash_tailq.head, c_hash_list);

  if (params == NULL) {
    RTE_LOG(ERR, HASH, "c_hash_create has no parameters\n");
    return NULL;
  }

  /* Check for valid parameters */
  if ((params->entries > C_HASH_ENTRIES_MAX) ||
      (params->entries < C_HASH_BUCKET_ENTRIES) ||
      (params->key_len + params->value_len <= 0)) {
    rte_errno = EINVAL;
    RTE_LOG(ERR, HASH, "c_hash_create has invalid parameters\n");
    return NULL;
  }

  /* Check extra flags field to check extra options. */
  if (params->extra_flag & C_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL)
    no_free_on_del = 1;

  /* Store all entries and leave the first entry as a dummy entry
   */
  num_entry_slots = params->entries + 1;

  snprintf(ring_name, sizeof(ring_name), "C_HT_%s", params->name);
  /* Create ring (Dummy slot index is not enqueued) */
  r = rte_ring_create(ring_name, rte_align32pow2(num_entry_slots),
                      params->socket_id, RING_F_SP_ENQ | RING_F_SC_DEQ);
  if (r == NULL) {
    RTE_LOG(ERR, HASH, "memory allocation failed\n");
    goto err;
  }

  const uint32_t num_buckets =
      rte_align32pow2(params->entries) / C_HASH_BUCKET_ENTRIES;

  snprintf(hash_name, sizeof(hash_name), "C_HT_%s", params->name);

  rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

  /* guarantee there's no existing: this is normally already checked
   * by ring creation above */
  TAILQ_FOREACH(te, hash_list, next) {
    h = (struct c_hash *)te->data;
    if (strncmp(params->name, h->name, C_HASH_NAMESIZE) == 0)
      break;
  }
  h = NULL;
  if (te != NULL) {
    rte_errno = EEXIST;
    te = NULL;
    goto err_unlock;
  }

  te = rte_zmalloc("HASH_TAILQ_ENTRY", sizeof(*te), 0);
  if (te == NULL) {
    RTE_LOG(ERR, HASH, "tailq entry allocation failed\n");
    goto err_unlock;
  }

  h = (struct c_hash *)rte_zmalloc_socket(
      hash_name, sizeof(struct c_hash), RTE_CACHE_LINE_SIZE, params->socket_id);

  if (h == NULL) {
    RTE_LOG(ERR, HASH, "memory allocation failed\n");
    goto err_unlock;
  }

  buckets = rte_zmalloc_socket(NULL, num_buckets * sizeof(struct c_hash_bucket),
                               RTE_CACHE_LINE_SIZE, params->socket_id);

  if (buckets == NULL) {
    RTE_LOG(ERR, HASH, "buckets memory allocation failed\n");
    goto err_unlock;
  }

  const uint32_t entry_size = RTE_ALIGN(sizeof(struct c_hash_entry) +
                                            params->key_len + params->value_len,
                                        C_ENTRY_ALIGNMENT);
  const uint64_t entry_tbl_size = (uint64_t)entry_size * num_entry_slots;

  k = rte_zmalloc_socket(NULL, entry_tbl_size, RTE_CACHE_LINE_SIZE,
                         params->socket_id);

  if (k == NULL) {
    RTE_LOG(ERR, HASH, "memory allocation failed\n");
    goto err_unlock;
  }

  /* Default hash function */
#if defined(RTE_ARCH_X86)
  default_hash_func = (c_hash_function)rte_hash_crc;
#endif
  /* Setup hash context */
  snprintf(h->name, sizeof(h->name), "%s", params->name);
  h->entries = params->entries;
  h->key_len = params->key_len;
  h->entry_size = entry_size;
  h->hash_func_init_val = params->hash_func_init_val;

  h->num_buckets = num_buckets;
  h->bucket_bitmask = h->num_buckets - 1;
  h->buckets = buckets;
  h->hash_func =
      (params->hash_func == NULL) ? default_hash_func : params->hash_func;
  h->c_hash_custom_cmp_eq =
      (params->cmp_func == NULL) ? default_cmp_func : params->cmp_func;
  h->entry_store = k;
  h->free_slots = r;
  h->no_free_on_del = no_free_on_del;

  /* Populate free slots ring. Entry zero is reserved for key misses. */
  for (i = 1; i < num_entry_slots; i++)
    rte_ring_sp_enqueue(r, (void *)((uintptr_t)i));

  te->data = (void *)h;
  TAILQ_INSERT_TAIL(hash_list, te, next);
  rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

  return h;
err_unlock:
  rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
err:
  rte_ring_free(r);
  rte_free(te);
  rte_free(h);
  rte_free(buckets);
  rte_free(k);
  return NULL;
}

void c_hash_free(struct c_hash *h) {
  struct rte_tailq_entry *te;
  struct c_hash_list *hash_list;

  if (h == NULL)
    return;

  hash_list = RTE_TAILQ_CAST(c_hash_tailq.head, c_hash_list);

  rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

  /* find out tailq entry */
  TAILQ_FOREACH(te, hash_list, next) {
    if (te->data == (void *)h)
      break;
  }

  if (te == NULL) {
    rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
    return;
  }

  TAILQ_REMOVE(hash_list, te, next);

  rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

  rte_ring_free(h->free_slots);
  rte_free(h->entry_store);
  rte_free(h->buckets);
  rte_free(h);
  rte_free(te);
}

c_hash_sig_t c_hash_hash(const struct c_hash *h, const void *key) {
  /* calc hash result by key */
  return h->hash_func(key, h->key_len, h->hash_func_init_val);
}

int32_t c_hash_count(const struct c_hash *h) {
  if (h == NULL)
    return -EINVAL;

  return h->entries - rte_ring_count(h->free_slots);
}

void c_hash_reset(struct c_hash *h) {
  void *ptr;
  uint32_t tot_ring_cnt, i;

  if (h == NULL)
    return;

  memset(h->buckets, 0, h->num_buckets * sizeof(struct c_hash_bucket));
  memset(h->entry_store, 0, h->entry_size * (h->entries + 1));

  /* clear the free ring */
  while (rte_ring_dequeue(h->free_slots, &ptr) == 0)
    continue;

  /* Repopulate the free slots ring. Entry zero is reserved for key misses */
  tot_ring_cnt = h->entries;

  for (i = 1; i < tot_ring_cnt + 1; i++)
    rte_ring_sp_enqueue(h->free_slots, (void *)((uintptr_t)i));
}

/*
 * Function called to enqueue back an index in the cache/ring,
 * as slot has not being used and it can be used in the
 * next addition attempt.
 */
static inline void c_enqueue_slot_back(const struct c_hash *h, void *slot_id) {
  rte_ring_sp_enqueue(h->free_slots, slot_id);
}

/* Search a key from bucket and update its data.
 * Writer holds the lock before calling this.
 */
static inline int32_t c_search_and_update(const struct c_hash *h,
                                          const void *key,
                                          struct c_hash_bucket *bkt,
                                          uint16_t sig) {
  int i;
  struct c_hash_entry *k, *keys = h->entry_store;

  for (i = 0; i < C_HASH_BUCKET_ENTRIES; i++) {
    if (bkt->sig_current[i] == sig) {
      k = (struct c_hash_entry *)((char *)keys +
                                  bkt->key_idx[i] * h->entry_size);
      if (c_hash_cmp_eq(key, k->entry, h) == 0) {
        /*
         * Return index where key is stored,
         * subtracting the first dummy index
         */
        return bkt->key_idx[i] - 1;
      }
    }
  }
  return -1;
}

/* Only tries to insert at one bucket (@prim_bkt) without trying to push
 * buckets around.
 * return 0 if succeeds, return -1 for no empty entry.
 */
static inline int32_t c_hash_cuckoo_insert(const struct c_hash *h,
                                           struct c_hash_bucket *prim_bkt,
                                           struct c_hash_bucket *sec_bkt,
                                           const struct c_hash_entry *key,
                                           uint16_t sig, uint32_t new_idx,
                                           int32_t *ret_val) {
  unsigned int i;
  struct c_hash_bucket *cur_bkt;
  int32_t ret;

  /* Insert new entry if there is room in the primary
   * bucket.
   */
  for (i = 0; i < C_HASH_BUCKET_ENTRIES; i++) {
    /* Check if slot is available */
    if (likely(prim_bkt->key_idx[i] == C_EMPTY_SLOT)) {
      prim_bkt->sig_current[i] = sig;
      prim_bkt->key_idx[i] = new_idx;
      break;
    }
  }

  if (i != C_HASH_BUCKET_ENTRIES)
    return 0;

  /* no empty entry */
  return -1;
}

/* Shift buckets along provided cuckoo_path (@leaf and @leaf_slot) and fill
 * the path head with new entry (sig, alt_hash, new_idx)
 * return -1 if cuckoo path invalided and fail, return 0 if succeeds.
 */
static inline int
c_hash_cuckoo_move_insert(const struct c_hash *h, struct c_hash_bucket *bkt,
                          struct c_hash_bucket *alt_bkt,
                          const struct c_hash_entry *key,
                          struct c_queue_node *leaf, uint32_t leaf_slot,
                          uint16_t sig, uint32_t new_idx, int32_t *ret_val) {
  uint32_t prev_alt_bkt_idx;
  struct c_queue_node *prev_node, *curr_node = leaf;
  struct c_hash_bucket *prev_bkt, *curr_bkt = leaf->bkt;
  uint32_t prev_slot, curr_slot = leaf_slot;
  int32_t ret;

  while (likely(curr_node->prev != NULL)) {
    prev_node = curr_node->prev;
    prev_bkt = prev_node->bkt;
    prev_slot = curr_node->prev_slot;

    prev_alt_bkt_idx = c_get_alt_bucket_index(h, prev_node->cur_bkt_idx,
                                              prev_bkt->sig_current[prev_slot]);

    if (unlikely(&h->buckets[prev_alt_bkt_idx] != curr_bkt)) {
      /* revert it to empty, otherwise duplicated keys */
      curr_bkt->key_idx[curr_slot] = C_EMPTY_SLOT;
      return -1;
    }

    /* Need to swap current/alt sig to allow later
     * Cuckoo insert to move elements back to its
     * primary bucket if available
     */
    curr_bkt->sig_current[curr_slot] = prev_bkt->sig_current[prev_slot];
    /* Release the updated bucket entry */
    curr_bkt->key_idx[curr_slot] = prev_bkt->key_idx[prev_slot];

    curr_slot = prev_slot;
    curr_node = prev_node;
    curr_bkt = curr_node->bkt;
  }

  curr_bkt->sig_current[curr_slot] = sig;
  /* Release the new bucket entry */
  curr_bkt->key_idx[curr_slot] = new_idx;

  return 0;
}

/*
 * Make space for new key, using bfs Cuckoo Search
 */
static inline int c_hash_cuckoo_make_space(const struct c_hash *h,
                                           struct c_hash_bucket *bkt,
                                           struct c_hash_bucket *sec_bkt,
                                           const struct c_hash_entry *key,
                                           uint16_t sig, uint32_t bucket_idx,
                                           uint32_t new_idx, int32_t *ret_val) {
  unsigned int i;
  struct c_queue_node queue[C_HASH_BFS_QUEUE_MAX_LEN];
  struct c_queue_node *tail, *head;
  struct c_hash_bucket *curr_bkt, *alt_bkt;
  uint32_t cur_idx, alt_idx;

  tail = queue;
  head = queue + 1;
  tail->bkt = bkt;
  tail->prev = NULL;
  tail->prev_slot = -1;
  tail->cur_bkt_idx = bucket_idx;

  /* Cuckoo bfs Search */
  while (likely(tail != head && head < queue + C_HASH_BFS_QUEUE_MAX_LEN -
                                           C_HASH_BUCKET_ENTRIES)) {
    curr_bkt = tail->bkt;
    cur_idx = tail->cur_bkt_idx;
    for (i = 0; i < C_HASH_BUCKET_ENTRIES; i++) {
      if (curr_bkt->key_idx[i] == C_EMPTY_SLOT) {
        return c_hash_cuckoo_move_insert(h, bkt, sec_bkt, key, tail, i, sig,
                                         new_idx, ret_val);
      }

      /* Enqueue new node and keep prev node info */
      alt_idx = c_get_alt_bucket_index(h, cur_idx, curr_bkt->sig_current[i]);
      alt_bkt = &(h->buckets[alt_idx]);
      head->bkt = alt_bkt;
      head->cur_bkt_idx = alt_idx;
      head->prev = tail;
      head->prev_slot = i;
      head++;
    }
    tail++;
  }

  return -ENOSPC;
}

static inline int32_t __c_hash_add_key_with_hash(const struct c_hash *h,
                                                 const void *key,
                                                 c_hash_sig_t sig,
                                                 int *replaced) {
  uint16_t short_sig;
  uint32_t prim_bucket_idx, sec_bucket_idx;
  struct c_hash_bucket *prim_bkt, *sec_bkt, *cur_bkt;
  struct c_hash_entry *new_k, *keys = h->entry_store;
  void *slot_id = NULL;
  uint32_t new_idx;
  int ret;
  int32_t ret_val;

  short_sig = c_get_short_sig(sig);
  prim_bucket_idx = c_get_prim_bucket_index(h, sig);
  sec_bucket_idx = c_get_alt_bucket_index(h, prim_bucket_idx, short_sig);
  prim_bkt = &h->buckets[prim_bucket_idx];
  sec_bkt = &h->buckets[sec_bucket_idx];
  rte_prefetch0(prim_bkt);
  rte_prefetch0(sec_bkt);

  *replaced = 1;

  /* Check if key is already inserted in primary location */
  ret = c_search_and_update(h, key, prim_bkt, short_sig);
  if (ret != -1) {
    return ret;
  }

  /* Check if key is already inserted in secondary location */
  FOR_EACH_BUCKET(cur_bkt, sec_bkt) {
    ret = c_search_and_update(h, key, cur_bkt, short_sig);
    if (ret != -1) {
      return ret;
    }
  }

  *replaced = 0;

  /* Did not find a match, so get a new slot for storing the new key */
  if (rte_ring_sc_dequeue(h->free_slots, &slot_id) != 0) {
    return -ENOSPC;
  }

  new_k = RTE_PTR_ADD(keys, (uintptr_t)slot_id * h->entry_size);
  new_idx = (uint32_t)((uintptr_t)slot_id);
  /* Copy key */
  memcpy(new_k->entry, key, h->key_len);

  /* Find an empty slot and insert */
  ret = c_hash_cuckoo_insert(h, prim_bkt, sec_bkt, key, short_sig, new_idx,
                             &ret_val);
  if (ret == 0)
    return new_idx - 1;

  /* Primary bucket full, need to make space for new entry */
  ret = c_hash_cuckoo_make_space(h, prim_bkt, sec_bkt, key, short_sig,
                                 prim_bucket_idx, new_idx, &ret_val);
  if (ret == 0)
    return new_idx - 1;

  /* Also search secondary bucket to get better occupancy */
  ret = c_hash_cuckoo_make_space(h, sec_bkt, prim_bkt, key, short_sig,
                                 sec_bucket_idx, new_idx, &ret_val);

  if (ret == 0)
    return new_idx - 1;
  else {
    c_enqueue_slot_back(h, slot_id);
    return ret_val;
  }
}

int32_t c_hash_add_key_with_hash(const struct c_hash *h, const void *key,
                                 c_hash_sig_t sig, int *replaced) {
  C_RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
  return __c_hash_add_key_with_hash(h, key, sig, replaced);
}

int32_t c_hash_add_key(const struct c_hash *h, const void *key, int *replaced) {
  C_RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
  return __c_hash_add_key_with_hash(h, key, c_hash_hash(h, key), replaced);
}

/* Search one bucket to find the match key */
static inline int32_t c_search_one_bucket_lf(const struct c_hash *h,
                                             const void *key, uint16_t sig,
                                             const struct c_hash_bucket *bkt) {
  int i;
  uint32_t key_idx;
  struct c_hash_entry *k, *keys = h->entry_store;

  for (i = 0; i < C_HASH_BUCKET_ENTRIES; i++) {
    key_idx = bkt->key_idx[i];
    if (bkt->sig_current[i] == sig && key_idx != C_EMPTY_SLOT) {
      k = (struct c_hash_entry *)((char *)keys + key_idx * h->entry_size);

      if (c_hash_cmp_eq(key, k->entry, h) == 0) {
        /*
         * Return index where key is stored,
         * subtracting the first dummy index
         */
        return key_idx - 1;
      }
    }
  }
  return -1;
}

static inline int32_t __c_hash_lookup_with_hash_lf(const struct c_hash *h,
                                                   const void *key,
                                                   c_hash_sig_t sig) {
  uint32_t prim_bucket_idx, sec_bucket_idx;
  struct c_hash_bucket *bkt, *cur_bkt;
  int ret;
  uint16_t short_sig;

  short_sig = c_get_short_sig(sig);
  prim_bucket_idx = c_get_prim_bucket_index(h, sig);
  sec_bucket_idx = c_get_alt_bucket_index(h, prim_bucket_idx, short_sig);

  /* Check if key is in primary location */
  bkt = &h->buckets[prim_bucket_idx];
  ret = c_search_one_bucket_lf(h, key, short_sig, bkt);
  if (ret != -1) {
    return ret;
  }
  /* Calculate secondary hash */
  bkt = &h->buckets[sec_bucket_idx];

  /* Check if key is in secondary location */
  FOR_EACH_BUCKET(cur_bkt, bkt) {
    ret = c_search_one_bucket_lf(h, key, short_sig, cur_bkt);
    if (ret != -1) {
      return ret;
    }
  }

  return -ENOENT;
}

static inline int32_t __c_hash_lookup_with_hash(const struct c_hash *h,
                                                const void *key,
                                                c_hash_sig_t sig) {
  return __c_hash_lookup_with_hash_lf(h, key, sig);
}

int32_t c_hash_lookup_with_hash(const struct c_hash *h, const void *key,
                                c_hash_sig_t sig) {
  C_RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
  return __c_hash_lookup_with_hash(h, key, sig);
}

int32_t c_hash_lookup(const struct c_hash *h, const void *key) {
  C_RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
  return __c_hash_lookup_with_hash(h, key, c_hash_hash(h, key));
}

static inline void c_remove_entry(const struct c_hash *h,
                                  struct c_hash_bucket *bkt, unsigned i) {
  rte_ring_sp_enqueue(h->free_slots, (void *)((uintptr_t)bkt->key_idx[i]));
}

/* Compact the linked list by moving key from last entry in linked list to the
 * empty slot.
 */
static inline void __c_hash_compact_ll(struct c_hash_bucket *cur_bkt, int pos) {
  int i;
  struct c_hash_bucket *last_bkt;

  if (!cur_bkt->next)
    return;

  last_bkt = c_hash_get_last_bkt(cur_bkt);

  for (i = C_HASH_BUCKET_ENTRIES - 1; i >= 0; i--) {
    if (last_bkt->key_idx[i] != C_EMPTY_SLOT) {
      cur_bkt->key_idx[pos] = last_bkt->key_idx[i];
      cur_bkt->sig_current[pos] = last_bkt->sig_current[i];
      last_bkt->sig_current[i] = C_NULL_SIGNATURE;
      last_bkt->key_idx[i] = C_EMPTY_SLOT;
      return;
    }
  }
}

/* Search one bucket and remove the matched key.
 * Writer is expected to hold the lock while calling this
 * function.
 */
static inline int32_t c_search_and_remove(const struct c_hash *h,
                                          const void *key,
                                          struct c_hash_bucket *bkt,
                                          uint16_t sig, int *pos) {
  struct c_hash_entry *k, *keys = h->entry_store;
  unsigned int i;
  uint32_t key_idx;

  /* Check if key is in bucket */
  for (i = 0; i < C_HASH_BUCKET_ENTRIES; i++) {
    key_idx = bkt->key_idx[i];
    if (bkt->sig_current[i] == sig && key_idx != C_EMPTY_SLOT) {
      k = (struct c_hash_entry *)((char *)keys + key_idx * h->entry_size);
      if (c_hash_cmp_eq(key, k->entry, h) == 0) {
        bkt->sig_current[i] = C_NULL_SIGNATURE;
        /* Free the key store index if
         * no_free_on_del is disabled.
         */
        if (!h->no_free_on_del)
          c_remove_entry(h, bkt, i);

        bkt->key_idx[i] = C_EMPTY_SLOT;

        *pos = i;
        /*
         * Return index where key is stored,
         * subtracting the first dummy index
         */
        return key_idx - 1;
      }
    }
  }
  return -1;
}

static inline int32_t __c_hash_del_key_with_hash(const struct c_hash *h,
                                                 const void *key,
                                                 c_hash_sig_t sig) {
  uint32_t prim_bucket_idx, sec_bucket_idx;
  struct c_hash_bucket *prim_bkt, *sec_bkt;
  struct c_hash_bucket *cur_bkt;
  int pos;
  int32_t ret;
  uint16_t short_sig;

  short_sig = c_get_short_sig(sig);
  prim_bucket_idx = c_get_prim_bucket_index(h, sig);
  sec_bucket_idx = c_get_alt_bucket_index(h, prim_bucket_idx, short_sig);
  prim_bkt = &h->buckets[prim_bucket_idx];

  /* look for key in primary bucket */
  ret = c_search_and_remove(h, key, prim_bkt, short_sig, &pos);
  if (ret != -1) {
    __c_hash_compact_ll(prim_bkt, pos);
    return ret;
  }

  /* Calculate secondary hash */
  sec_bkt = &h->buckets[sec_bucket_idx];

  FOR_EACH_BUCKET(cur_bkt, sec_bkt) {
    ret = c_search_and_remove(h, key, cur_bkt, short_sig, &pos);
    if (ret != -1) {
      __c_hash_compact_ll(cur_bkt, pos);
      return ret;
    }
  }

  return -ENOENT;
}

int32_t c_hash_del_key_with_hash(const struct c_hash *h, const void *key,
                                 c_hash_sig_t sig) {
  C_RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
  return __c_hash_del_key_with_hash(h, key, sig);
}

int32_t c_hash_del_key(const struct c_hash *h, const void *key) {
  C_RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
  return __c_hash_del_key_with_hash(h, key, c_hash_hash(h, key));
}

int c_hash_get_entry_with_position(const struct c_hash *h, int32_t position,
                                   void **entry) {
  C_RETURN_IF_TRUE(((h == NULL) || (entry == NULL)), -EINVAL);

  struct c_hash_entry *k, *keys = h->entry_store;
  k = (struct c_hash_entry *)((char *)keys + (position + 1) * h->entry_size);
  *entry = k->entry;

  return 0;
}

int c_hash_free_key_with_position(const struct c_hash *h,
                                  const int32_t position) {
  C_RETURN_IF_TRUE(((h == NULL) || (position == C_EMPTY_SLOT)), -EINVAL);

  const int32_t total_entries = h->num_buckets * C_HASH_BUCKET_ENTRIES;

  /* Out of bounds */
  if (position >= total_entries)
    return -EINVAL;

  rte_ring_sp_enqueue(h->free_slots, (void *)((uintptr_t)position));

  return 0;
}

int32_t c_hash_iterate(const struct c_hash *h, void **entry, uint32_t *next) {
  uint32_t bucket_idx, idx, position;
  struct c_hash_entry *next_key;

  C_RETURN_IF_TRUE(((h == NULL) || (next == NULL)), -EINVAL);

  const uint32_t total_entries_main = h->num_buckets * C_HASH_BUCKET_ENTRIES;

  /* Out of bounds of all buckets (both main table and ext table) */
  if (*next >= total_entries_main)
    return -ENOENT;

  /* Calculate bucket and index of current iterator */
  bucket_idx = *next / C_HASH_BUCKET_ENTRIES;
  idx = *next % C_HASH_BUCKET_ENTRIES;

  /* If current position is empty, go to the next one */
  while ((position = h->buckets[bucket_idx].key_idx[idx]) == C_EMPTY_SLOT) {
    (*next)++;
    /* End of table */
    if (*next == total_entries_main)
      return -ENOENT;
    bucket_idx = *next / C_HASH_BUCKET_ENTRIES;
    idx = *next % C_HASH_BUCKET_ENTRIES;
  }

  next_key = (struct c_hash_entry *)((char *)h->entry_store +
                                     position * h->entry_size);
  /* Return key */
  *entry = next_key->entry;

  /* Increment iterator */
  (*next)++;

  return position - 1;
}
