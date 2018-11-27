//
// Created by haoyixin on 11/26/18.
//

#ifndef XLB_UTILS_HACKS_H
#define XLB_UTILS_HACKS_H

#include <generic/rte_rwlock.h>
#include <rte_hash.h>

#define KEY_ALIGNMENT 16

/* Structure that stores key-value pair */
struct _rte_hash_key {
  union {
    uintptr_t idata;
    void *pdata;
  };
  /* Variable key size */
  char key[0];
} __attribute__((aligned(KEY_ALIGNMENT)));

/* All different signature compare functions */
enum _rte_hash_sig_compare_function {
  RTE_HASH_COMPARE_SCALAR = 0,
  RTE_HASH_COMPARE_SSE,
  RTE_HASH_COMPARE_NUM
};

/*
 * All different options to select a key compare function,
 * based on the key size and custom function.
 */
enum _cmp_jump_table_case {
  KEY_CUSTOM = 0,
  KEY_OTHER_BYTES,
  NUM_KEY_CMP_CASES,
};

/** A hash table structure. */
struct _rte_hash {
  char name[RTE_HASH_NAMESIZE]; /**< Name of the hash. */
  uint32_t entries;             /**< Total table entries. */
  uint32_t num_buckets;         /**< Number of buckets in table. */

  struct rte_ring *free_slots;
  /**< Ring that stores all indexes of the free slots in the key table */

  struct lcore_cache *local_free_slots;
  /**< Local cache per lcore, storing some indexes of the free slots */

  /* Fields used in lookup */

  uint32_t key_len __rte_cache_aligned;
  /**< Length of hash key. */
  uint8_t hw_trans_mem_support;
  /**< If hardware transactional memory is used. */
  uint8_t use_local_cache;
  /**< If multi-writer support is enabled, use local cache
   * to allocate key-store slots.
   */
  uint8_t readwrite_concur_support;
  /**< If read-write concurrency support is enabled */
  uint8_t ext_table_support; /**< Enable extendable bucket table */
  uint8_t no_free_on_del;
  /**< If key index should be freed on calling rte_hash_del_xxx APIs.
   * If this is set, rte_hash_free_key_with_position must be called to
   * free the key index associated with the deleted entry.
   * This flag is enabled by default.
   */
  uint8_t readwrite_concur_lf_support;
  /**< If read-write concurrency lock free support is enabled */
  uint8_t writer_takes_lock;
  /**< Indicates if the writer threads need to take lock */
  rte_hash_function hash_func; /**< Function used to calculate hash. */
  uint32_t hash_func_init_val; /**< Init value used by hash_func. */
  rte_hash_cmp_eq_t rte_hash_custom_cmp_eq;
  /**< Custom function used to compare keys. */
  enum _cmp_jump_table_case cmp_jump_table_idx;
  /**< Indicates which compare function to use. */
  enum _rte_hash_sig_compare_function sig_cmp_fn;
  /**< Indicates which signature compare function to use. */
  uint32_t bucket_bitmask;
  /**< Bitmask for getting bucket index from hash signature. */
  uint32_t key_entry_size; /**< Size of each key entry. */

  void *key_store; /**< Table storing all keys and data */
  struct rte_hash_bucket *buckets;
  /**< Table with buckets storing all the	hash values and key indexes
   * to the key table.
   */
  rte_rwlock_t *readwrite_lock;        /**< Read-write lock thread-safety. */
  struct rte_hash_bucket *buckets_ext; /**< Extra buckets array */
  struct rte_ring *free_ext_bkts;      /**< Ring of indexes of free buckets */
  uint32_t *tbl_chng_cnt;
  /**< Indicates if the hash table changed from last read. */
} __rte_cache_aligned;

/**
 * Find a key in the hash table given the position.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to get the key from.
 * @param position
 *   Position returned when the key was inserted.
 * @param key
 *   Output containing a pointer to the key
 * @return
 *   - 0 if retrieved successfully
 *   - -EINVAL if the parameters are invalid.
 */
int _rte_hash_get_key_with_position(const struct rte_hash *h,
                                    const int32_t position, void **key) {
  if (h == nullptr || key == nullptr)
    return -EINVAL;

  const struct _rte_hash *_h = reinterpret_cast<const struct _rte_hash *>(h);
  struct _rte_hash_key *k,
      *keys = reinterpret_cast<struct _rte_hash_key *>(_h->key_store);
  k = (struct _rte_hash_key *)((char *)keys +
                               (position + 1) * _h->key_entry_size);
  *key = k->key;

  return 0;
}

#endif // XLB_UTILS_HACKS_H
