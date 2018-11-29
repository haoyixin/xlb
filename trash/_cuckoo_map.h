//
// Created by haoyixin on 11/21/18.
//

#ifndef XLB_UTILS_CUCKOO_MAP_H
#define XLB_UTILS_CUCKOO_MAP_H

#include <cassert>

#include <glog/logging.h>
#include <optional>
#include <rte_hash_crc.h>

#include "clibs/_hash.h"

//#include "common.h"

namespace xlb {
namespace utils {

typedef uint32_t HashResult;

template <typename K> class DefaultHash {
public:
  explicit DefaultHash() {}

  inline HashResult operator()(const K &key) const {
    return rte_hash_crc(&key, sizeof(K), 0);
  }
};

template <typename K> class DefaultEqual {
public:
  explicit DefaultEqual() {}

  inline int operator()(const K &key1, const K &key2) {
    return memcmp(&key1, &key2, sizeof(K));
  }
};

// A Hash table implementation using clibs/hash
template <typename K, typename V, typename H = DefaultHash<K>,
          typename E = DefaultEqual<K>>
class CuckooMap {
  static_assert(std::is_trivially_copyable<K>());

public:
  typedef std::pair<typename std::add_const<K>::type, V> Entry;

  class iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = Entry *;
    using reference = Entry &;
    using iterator_category = std::forward_iterator_tag;

    iterator(CuckooMap &map, int32_t pos, uint32_t iter)
        : map_(map), pos_(pos), iter_(iter) {
      if (pos != -ENOENT)
        pos_ = c_hash_iterate(map_.c_hash_, reinterpret_cast<void **>(&entry_),
                              &iter_);
    }

    iterator &operator++() {
      pos_ = c_hash_iterate(map_.c_hash_, reinterpret_cast<void **>(&entry_),
                            &iter_);
      return *this;
    }

    const iterator operator++(int) {
      iterator tmp(*this);
      pos_ = c_hash_iterate(map_.c_hash_, reinterpret_cast<void **>(&entry_),
                            &iter_);
      return tmp;
    }

    bool operator==(const iterator &rhs) const {
      return &map_ == &rhs.map_ && pos_ == rhs.pos_;
    }

    bool operator!=(const iterator &rhs) const {
      return &map_ != &rhs.map_ || pos_ != rhs.pos_;
    }

    reference operator*() { return *entry_; }

    pointer operator->() { return entry_; }

  private:
    CuckooMap &map_;
    int32_t pos_;
    pointer entry_;
    uint32_t iter_;
  };

  CuckooMap(const std::string &name, uint32_t entries, int socket_id) {
    c_hash_parameters parameters{};
    memset(&parameters, 0, sizeof(c_hash_parameters));

    parameters.name = name.c_str();
    parameters.entries = entries;
    parameters.key_len = sizeof(K);
    parameters.value_len = sizeof(V);
    parameters.socket_id = socket_id;
    parameters.extra_flag = C_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;

    if (!std::is_same<H, DefaultHash<K>>())
      parameters.hash_func = c_hash_func_;

    c_hash_ = c_hash_create(&parameters);
    CHECK_NOTNULL(c_hash_);

    if (!std::is_same<E, DefaultEqual<K>>())
      c_hash_set_cmp_func(c_hash_, c_hash_custom_cmp_eq_);
  }

  ~CuckooMap() { c_hash_free(c_hash_); }

  // Not allowing copying for now
  CuckooMap(CuckooMap &) = delete;

  CuckooMap &operator=(CuckooMap &) = delete;

  // Allow move
  CuckooMap(CuckooMap &&) = default;

  CuckooMap &operator=(CuckooMap &&) = default;

  iterator begin() { return iterator(*this, 0, 0); }

  iterator end() { return iterator(*this, -ENOENT, 0); }

  // Insert/update a key value pair
  // On success returns a optional<Entry *> to the inserted entry, nullopt
  // otherwise. NOTE: when Insert() returns nullopt, the copy/move constructor
  // of `V` may not be called.
  std::optional<Entry *> Insert(const K &key, const V &value) {
    return DoEmplace(key, value);
  }

  std::optional<Entry *> Insert(const K &key, V &&value) {
    return DoEmplace(key, std::move(value));
  }

  // Emplace/update-in-place a key value pair
  // On success returns a optional<Entry *> to the inserted entry, nullopt
  // otherwise. NOTE: when Emplace() returns nullopt, the constructor of `V` may
  // not be called.
  template <typename... Args>
  std::optional<Entry *> Emplace(const K &key, Args &&... args) {
    return DoEmplace(key, std::forward<Args>(args)...);
  }

  // Find the pointer to the stored entry by the key.
  std::optional<Entry *> Find(const K &key) { return DoFind(key); }

  // const version of Find()
  std::optional<const Entry *> Find(const K &key) const { return DoFind(key); }

  // Remove the stored entry by the key
  // Return false if not exist.
  bool Remove(const K &key) {
    Entry *entry;

    auto position = c_hash_del_key(c_hash_, &key);
    if (position < 0)
      return false;

    c_hash_get_entry_with_position(c_hash_, position,
                                   reinterpret_cast<void **>(&entry));

    (&entry->second)->~V();
    c_hash_free_key_with_position(c_hash_, position);

    return true;
  }

  void Clear() { c_hash_reset(c_hash_); }

  // Return the number of stored entries
  int32_t Count() const { return c_hash_count(c_hash_); }

protected:
  c_hash *c_hash_;

  inline std::optional<Entry *> DoFind(const K &key) const {
    Entry *entry;

    auto position = c_hash_lookup(c_hash_, &key);
    if (position < 0)
      return std::nullopt;

    c_hash_get_entry_with_position(c_hash_, position,
                                   reinterpret_cast<void **>(&entry));

    return entry;
  }

  template <typename... Args>
  inline std::optional<Entry *> DoEmplace(const K &key, Args &&... args) {
    Entry *entry;
    int replaced;

    auto position = c_hash_add_key(c_hash_, &key, &replaced);

    if (position < 0) {
      return std::nullopt;
    } else {
      c_hash_get_entry_with_position(c_hash_, position,
                                     reinterpret_cast<void **>(&entry));
      if (replaced)
        (&entry->second)->~V();

      new (&entry->second) V(std::forward<Args>(args)...);
    }

    return entry;
  }

  static uint32_t c_hash_func_(const void *key, uint32_t, uint32_t) {
    return H()(*static_cast<const K *>(key));
  }

  static int c_hash_custom_cmp_eq_(const void *key1, const void *key2, size_t) {
    return E()(*static_cast<const K *>(key1),
               static_cast<const Entry *>(key2)->first);
  }
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_CUCKOO_MAP_H
