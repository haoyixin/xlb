#ifndef _C_HASH_H_
#define _C_HASH_H_

/**
 * @file
 *
 * C Hash Table
 */

#include <stddef.h>
#include <stdint.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of hash table that can be created. */
#define C_HASH_ENTRIES_MAX (1 << 30)

/** Maximum number of characters in hash name.*/
#define C_HASH_NAMESIZE 32

/** Flag to disable freeing of key index on hash delete.
 * Refer to c_hash_del_xxx APIs for more details.
 */
#define C_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL 0x10

/**
 * The type of hash value of a key.
 * It should be a value of at least 32bit with fully random pattern.
 */
typedef uint32_t c_hash_sig_t;

/** Type of function that can be used for calculating the hash value. */
typedef uint32_t (*c_hash_function)(const void *key, uint32_t key_len,
                                    uint32_t init_val);

/** Type of function used to compare the hash key. */
typedef int (*c_hash_cmp_eq_t)(const void *key1, const void *key2,
                               size_t key_len);

/**
 * Parameters used when creating the hash table.
 */
struct c_hash_parameters {
  const char *name;   /**< Name of the hash. */
  uint32_t entries;   /**< Total hash table entries. */
  uint32_t key_len;   /**< Length of entry key. */
  uint32_t value_len; /**< Length of entry value. */
  c_hash_function
      hash_func; /**< Primary Hash function used to calculate hash. */
  c_hash_cmp_eq_t cmp_func;    /**< Function used to compare key. */
  uint32_t hash_func_init_val; /**< Init value used by hash_func. */
  int socket_id;               /**< NUMA Socket ID for memory. */
  uint8_t extra_flag; /**< Indicate if additional parameters are present. */
};

/** @internal A hash table structure. */
struct c_hash;

/**
 * Create a new hash table.
 *
 * @param params
 *   Parameters used to create and initialise the hash table.
 * @return
 *   Pointer to hash table structure that is used in future hash table
 *   operations, or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - ENOENT - missing entry
 *    - EINVAL - invalid parameter passed to function
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct c_hash *c_hash_create(const struct c_hash_parameters *params);

/**
 * Set a new hash compare function other than the default one.
 *
 * @note Function pointer does not work with multi-process, so do not use it
 * in multi-process mode.
 *
 * @param h
 *   Hash table for which the function is to be changed
 * @param func
 *   New compare function
 */
void c_hash_set_cmp_func(struct c_hash *h, c_hash_cmp_eq_t func);

/**
 * Find an existing hash table object and return a pointer to it.
 *
 * @param name
 *   Name of the hash table as passed to rte_hash_create()
 * @return
 *   Pointer to hash table or NULL if object not found
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - ENOENT - value not available for return
 */
struct c_hash *c_hash_find_existing(const char *name);

/**
 * De-allocate all memory used by hash table.
 * @param h
 *   Hash table to free
 */
void c_hash_free(struct c_hash *h);

/**
 * Reset all hash structure, by zeroing all entries.
 * It is application's responsibility to make sure that
 * none of the readers are referencing the hash table
 * while calling this API.
 *
 * @param h
 *   Hash table to reset
 */
void c_hash_reset(struct c_hash *h);

/**
 * Return the number of keys in the hash table
 * @param h
 *  Hash table to query from
 * @return
 *   - -EINVAL if parameters are invalid
 *   - A value indicating how many keys were inserted in the table.
 */
int32_t c_hash_count(const struct c_hash *h);

/**
 * Add a key to an existing hash table. This operation is not multi-thread safe
 * and should only be called from one thread by default.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key.
 */
int32_t c_hash_add_key(const struct c_hash *h, const void *key);

/**
 * Add a key to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @param sig
 *   Precomputed hash value for 'key'.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key.
 */
int32_t c_hash_add_key_with_hash(const struct c_hash *h, const void *key,
                                 c_hash_sig_t sig);

/**
 * Remove a key from an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * If C_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL is enabled,
 * the key index returned by c_hash_add_key_xxx APIs will not be
 * freed by this API. c_hash_free_key_with_position API must be called
 * additionally to free the index associated with the key.
 * c_hash_free_key_with_position API should be called after all
 * the readers have stopped referencing the entry corresponding to
 * this key. RCU mechanisms could be used to determine such a state.
 *
 * @param h
 *   Hash table to remove the key from.
 * @param key
 *   Key to remove from the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t c_hash_del_key(const struct c_hash *h, const void *key);

/**
 * Remove a key from an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * If RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL is enabled,
 * the key index returned by c_hash_add_key_xxx APIs will not be
 * freed by this API. c_hash_free_key_with_position API must be called
 * additionally to free the index associated with the key.
 * c_hash_free_key_with_position API should be called after all
 * the readers have stopped referencing the entry corresponding to
 * this key. RCU mechanisms could be used to determine such a state.
 *
 * @param h
 *   Hash table to remove the key from.
 * @param key
 *   Key to remove from the hash table.
 * @param sig
 *   Precomputed hash value for 'key'.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t c_hash_del_key_with_hash(const struct c_hash *h, const void *key,
                                 c_hash_sig_t sig);

/**
 * Find a key in the hash table given the position.
 * This operation is multi-thread safe with regarding to other lookup threads.
 *
 * @param h
 *   Hash table to get the key from.
 * @param position
 *   Position returned when the key was inserted.
 * @param entry
 *   Output containing a pointer to the entry
 * @return
 *   - 0 if retrieved successfully
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if no valid key is found in the given position.
 */
int c_hash_get_entry_with_position(const struct c_hash *h, int32_t position,
                                   void **entry);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Free a hash key in the hash table given the position
 * of the key. This operation is not multi-thread safe and should
 * only be called from one thread by default.
 * If C_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL is enabled,
 * the key index returned by c_hash_del_key_xxx APIs must be freed
 * using this API. This API should be called after all the readers
 * have stopped referencing the entry corresponding to this key.
 * RCU mechanisms could be used to determine such a state.
 * This API does not validate if the key is already freed.
 *
 * @param h
 *   Hash table to free the key from.
 * @param position
 *   Position returned when the key was deleted.
 * @return
 *   - 0 if freed successfully
 *   - -EINVAL if the parameters are invalid.
 */
int c_hash_free_key_with_position(const struct c_hash *h,
                                                     const int32_t position);

/**
 * Find a key in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t c_hash_lookup(const struct c_hash *h, const void *key);

/**
 * Find a key in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param sig
 *   Precomputed hash value for 'key'.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t c_hash_lookup_with_hash(const struct c_hash *h, const void *key,
                                c_hash_sig_t sig);

/**
 * Calc a hash value by key.
 * This operation is not multi-thread safe.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @return
 *   - hash value
 */
c_hash_sig_t c_hash_hash(const struct c_hash *h, const void *key);

/**
 * Iterate through the hash table, returning key-value pairs.
 *
 * @param h
 *   Hash table to iterate
 * @param entry
 *   Output containing the entry where current iterator
 *   was pointing at
 * @param next
 *   Pointer to iterator. Should be 0 to start iterating the hash table.
 *   Iterator is incremented after each call of this function.
 * @return
 *   Position where key was stored, if successful.
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if end of the hash table.
 */
int32_t c_hash_iterate(const struct c_hash *h, const void **entry,
                       uint32_t *next);
#ifdef __cplusplus
}
#endif

#endif /* _C_HASH_H_ */
