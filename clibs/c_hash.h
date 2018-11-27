#ifndef _CUCKOO_HASH_H_
#define _CUCKOO_HASH_H_

/* Macro to enable/disable run-time checking of function parameters */
#if defined(C_HASH_DEBUG)
#define C_RETURN_IF_TRUE(cond, retval)                                         \
  do {                                                                         \
    if (cond)                                                                  \
      return retval;                                                           \
  } while (0)
#else
#define C_RETURN_IF_TRUE(cond, retval)
#endif

#include "hash.h"

/** Number of items per bucket. */
#define C_HASH_BUCKET_ENTRIES 8

#if !RTE_IS_POWER_OF_2(C_HASH_BUCKET_ENTRIES)
#error C_HASH_BUCKET_ENTRIES must be a power of 2
#endif

#define C_NULL_SIGNATURE 0

#define C_EMPTY_SLOT 0

#define C_ENTRY_ALIGNMENT 16

#define C_HASH_BFS_QUEUE_MAX_LEN 1000

/* Structure that stores key-value pair */
struct c_hash_entry {
  /* Variable entry size */
  char entry[0];
};

/** Bucket structure */
struct c_hash_bucket {
  uint16_t sig_current[C_HASH_BUCKET_ENTRIES];

  uint32_t key_idx[C_HASH_BUCKET_ENTRIES];

  uint8_t flag[C_HASH_BUCKET_ENTRIES];

  void *next;
} __rte_cache_aligned;

/** A hash table structure. */
struct c_hash {
  char name[C_HASH_NAMESIZE]; /**< Name of the hash. */
  uint32_t entries;           /**< Total table entries. */
  uint32_t num_buckets;       /**< Number of buckets in table. */

  struct rte_ring *free_slots;
  /**< Ring that stores all indexes of the free slots in the key table */

  /* Fields used in lookup */

  uint32_t key_len __rte_cache_aligned;
  /**< Length of entry key. */
  uint8_t no_free_on_del;
  /**< If key index should be freed on calling c_hash_del_xxx APIs.
   * If this is set, c_hash_free_key_with_position must be called to
   * free the key index associated with the deleted entry.
   * This flag is enabled by default.
   */
  c_hash_function hash_func;   /**< Function used to calculate hash. */
  uint32_t hash_func_init_val; /**< Init value used by hash_func. */
  c_hash_cmp_eq_t c_hash_custom_cmp_eq;
  /**< Custom function used to compare keys. */
  uint32_t bucket_bitmask;
  /**< Bitmask for getting bucket index from hash signature. */
  uint32_t entry_size; /**< Size of each key entry. */

  void *entry_store; /**< Table storing all keys and data */
  struct c_hash_bucket *buckets;
  /**< Table with buckets storing all the	hash values and key indexes
   * to the key table.
   */
  uint32_t *tbl_chng_cnt;
  /**< Indicates if the hash table changed from last read. */
} __rte_cache_aligned;

struct c_queue_node {
  struct c_hash_bucket *bkt; /* Current bucket on the bfs search */
  uint32_t cur_bkt_idx;

  struct c_queue_node *prev; /* Parent(bucket) in search path */
  int prev_slot;             /* Parent(slot) in search path */
};

#endif
