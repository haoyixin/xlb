//
// Created by haoyixin on 11/21/18.
//

#ifndef XLB_UTILS_CUCKOO_MAP_H
#define XLB_UTILS_CUCKOO_MAP_H

#include <cassert>

#include <glog/logging.h>
#include <rte_hash_crc.h>

#include "clibs/hash.h"

#include "common.h"

namespace xlb {
namespace utils {

typedef uint32_t HashResult;

template <typename K> class DefaultHash {
public:
  explicit DefaultHash() {}

  HashResult operator()(const K &key) const {
    return rte_hash_crc(&key, sizeof(K), 0);
  }
};

template <typename K> class DefaultEqual {
public:
  explicit DefaultEqual() {}

  int operator()(const K &key1, const K &key2) {
    return memcmp(&key1, &key2, sizeof(K));
  }
};

// A Hash table implementation using rte_hash
template <typename K, typename V, typename H = DefaultHash<K>,
          typename E = DefaultEqual<K>>
class CuckooMap {
public:
  typedef std::pair<K, V> Entry;

  class iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = Entry *;
    using reference = Entry &;
    using iterator_category = std::forward_iterator_tag;

    iterator(CuckooMap &map, uint32_t pos) : map_(map), pos_(pos) {}

    iterator &operator++() { // Pre-increment
    }

    iterator operator++(int) { // Pre-increment
    }

    bool operator==(const iterator &rhs) const {}

    bool operator!=(const iterator &rhs) const {}

    reference operator*() {}

    pointer operator->() {}

  private:
    CuckooMap &map_;
    uint32_t pos_;
  };

  CuckooMap(std::string &name, uint32_t entries, int socket_id) {
    c_hash_parameters parameters = {
        .name = name.c_str(),
        .entries = entries,
        .key_len = sizeof(Entry::first_type),
        .value_len = sizeof(Entry::second_type),
        .socket_id = socket_id,
        .hash_func = rte_hash_func_};
    rte_hash_ = c_hash_create(&parameters);
    CHECK_NOTNULL(rte_hash_);
      c_hash_set_cmp_func(rte_hash_, rte_hash_custom_cmp_eq_);
  }

  ~CuckooMap() { c_hash_free(rte_hash_); }

  // Not allowing copying for now
  CuckooMap(CuckooMap &) = delete;

  CuckooMap &operator=(CuckooMap &) = delete;

  // Allow move
  CuckooMap(CuckooMap &&) = default;

  CuckooMap &operator=(CuckooMap &&) = default;

  iterator begin() { return iterator(*this, 0, 0); }

  iterator end() { return iterator(*this, 0, 0); }

  template <typename... Args> Entry *DoEmplace(const K &key, Args &&... args) {
    Entry *entry;
    auto position = c_hash_add_key(rte_hash_, &key);

    if (position < 0 ||
            c_hash_get_entry_with_position(rte_hash_, position, entry) != 0)
      return nullptr;
    else
      new (&entry->second) V(std::forward<Args>(args)...);

    return entry;
  }

  // Insert/update a key value pair
  // On success returns a pointer to the inserted entry, nullptr otherwise.
  // NOTE: when Insert() returns nullptr, the copy/move constructor of `V` may
  // not be called.
  Entry *Insert(const K &key, const V &value) { return DoEmplace(key, value); }

  Entry *Insert(const K &key, V &&value) {
    return DoEmplace(key, std::move(value));
  }

  // Emplace/update-in-place a key value pair
  // On success returns a pointer to the inserted entry, nullptr otherwise.
  // NOTE: when Emplace() returns nullptr, the constructor of `V` may not be
  // called.
  template <typename... Args> Entry *Emplace(const K &key, Args &&... args) {
    return DoEmplace(key, std::forward<Args>(args)...);
  }

  // Find the pointer to the stored value by the key.
  // Return nullptr if not exist.
  Entry *Find(const K &key) {
    // Blame Effective C++ for this
    return const_cast<Entry *>(
        static_cast<const typename std::remove_reference<decltype(*this)>::type
                        &>(*this)
            .Find(key));
  }

  // const version of Find()
  const Entry *Find(const K &key) const {
    Entry *entry;
    auto position = c_hash_lookup(rte_hash_, &key);
    return position >= 0 && c_hash_get_entry_with_position(rte_hash_, position,
                                                           &entry) == 0
               ? entry
               : nullptr;
  }

  // Remove the stored entry by the key
  // Return false if not exist.
  bool Remove(const K &key) { return c_hash_del_key(rte_hash_, &key) >= 0; }

  void Clear() { c_hash_reset(rte_hash_); }

  // Return the number of stored entries
  int32_t Count() const { return c_hash_count(rte_hash_); }

protected:
  c_hash *rte_hash_;

  static uint32_t rte_hash_func_(const void *key, uint32_t key_len, uint32_t) {
    return H()(*static_cast<K *>(key));
  }

  static int rte_hash_custom_cmp_eq_(const void *key1, const void *key2,
                                     size_t key_len) {
    return E()(*static_cast<K *>(key1), static_cast<Entry *>(key2)->first);
  }
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_CUCKOO_MAP_H
