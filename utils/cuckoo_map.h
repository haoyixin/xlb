#pragma once

#include <array>
#include <functional>
#include <iterator>
#include <limits>
#include <stack>
#include <type_traits>
#include <utility>
#include <vector>

#include <glog/logging.h>

#include "common.h"
#include "utils/allocator.h"
#include "utils/boost.h"

namespace xlb::utils {

// A hash table implementation using cuckoo hashing on hugepage
// Note: this is not thread-safe
template <typename K, typename V, typename H = std::hash<K>,
          typename E = std::equal_to<K>>
class CuckooMap {
 public:
  using Entry = std::pair<K, V>;
  using HashResult = uint32_t;
  using EntryIndex = uint32_t;

  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = Entry *;
    using reference = Entry &;
    using iterator_category = std::forward_iterator_tag;

    iterator(CuckooMap &map, size_t bucket, size_t slot)
        : map_(map), bucket_idx_(bucket), slot_idx_(slot) {
      while (bucket_idx_ < map_.buckets_.size() &&
             map_.buckets_[bucket_idx_].hash_values[slot_idx_] == 0) {
        slot_idx_++;
        if (slot_idx_ == kEntriesPerBucket) {
          slot_idx_ = 0;
          bucket_idx_++;
        }
      }
    }

    iterator &operator++() {  // Pre-increment
      do {
        slot_idx_++;
        if (slot_idx_ == kEntriesPerBucket) {
          slot_idx_ = 0;
          bucket_idx_++;
        }
      } while (bucket_idx_ < map_.buckets_.size() &&
               map_.buckets_[bucket_idx_].hash_values[slot_idx_] == 0);
      return *this;
    }

    const iterator operator++(int) {  // Post-increment
      iterator tmp(*this);
      do {
        slot_idx_++;
        if (slot_idx_ == kEntriesPerBucket) {
          slot_idx_ = 0;
          bucket_idx_++;
        }
      } while (bucket_idx_ < map_.buckets_.size() &&
               map_.buckets_[bucket_idx_].hash_values[slot_idx_] == 0);
      return tmp;
    }

    bool operator==(const iterator &rhs) const {
      return &map_ == &rhs.map_ && bucket_idx_ == rhs.bucket_idx_ &&
             slot_idx_ == rhs.slot_idx_;
    }

    bool operator!=(const iterator &rhs) const {
      return &map_ != &rhs.map_ || bucket_idx_ != rhs.bucket_idx_ ||
             slot_idx_ != rhs.slot_idx_;
    }

    reference operator*() {
      EntryIndex idx = map_.buckets_[bucket_idx_].entry_indices[slot_idx_];
      return map_.entries_[idx];
    }

    pointer operator->() {
      EntryIndex idx = map_.buckets_[bucket_idx_].entry_indices[slot_idx_];
      return &map_.entries_[idx];
    }

   private:
    CuckooMap &map_;
    size_t bucket_idx_;
    size_t slot_idx_;
  };

  explicit CuckooMap(int socket = SOCKET_ID_ANY,
                     size_t reserve_buckets = kInitNumBucket,
                     size_t reserve_entries = kInitNumEntries)
      : bucket_mask_(reserve_buckets - 1),
        num_entries_(0),
        allocator_(socket),
        buckets_(reserve_buckets, &allocator_),
        entries_(reserve_entries, &allocator_),
        free_entry_indices_() {
    // the number of buckets must be a power of 2
    CHECK_EQ(align_ceil_pow2(reserve_buckets), reserve_buckets);

    for (auto i : irange(int(reserve_entries - 1), -1, -1))
      free_entry_indices_.push(i);
  }

  // Not allowing copying for now
  CuckooMap(CuckooMap &) = delete;
  CuckooMap &operator=(CuckooMap &) = delete;

  // Allow move
  CuckooMap(CuckooMap &&) = default;
  CuckooMap &operator=(CuckooMap &&) = default;

  iterator begin() { return iterator(*this, 0, 0); }
  iterator end() { return iterator(*this, buckets_.size(), 0); }

  // Insert/update a key value pair
  // On success returns a pointer to the inserted entry, nullptr otherwise.
  // NOTE: when Insert() returns nullptr, the copy/move constructor of `V` may
  // not be called.
  Entry *Insert(const K &key, const V &value, const H &hasher = H(),
                const E &eq = E()) {
    return do_emplace(key, hasher, eq, value);
  }

  Entry *Insert(const K &key, V &&value, const H &hasher = H(),
                const E &eq = E()) {
    return do_emplace(key, hasher, eq, std::move(value));
  }

  // Emplace/update-in-place a key value pair
  // On success returns a pointer to the inserted entry, nullptr otherwise.
  // NOTE: when Emplace() returns nullptr, the constructor of `V` may not be
  // called.
  template <typename... Args>
  Entry *Emplace(const K &key, Args &&... args) {
    return do_emplace(key, H(), E(), std::forward<Args>(args)...);
  }

  // Find the pointer to the stored value by the key.
  // Return nullptr if not exist.
  Entry *Find(const K &key, const H &hasher = H(), const E &eq = E()) {
    // Blame Effective C++ for this
    return const_cast<Entry *>(
        static_cast<const typename std::remove_reference<decltype(*this)>::type
                        &>(*this)
            .Find(key, hasher, eq));
  }

  // const version of Find()
  const Entry *Find(const K &key, const H &hasher = H(),
                    const E &eq = E()) const {
    EntryIndex idx = find_with_hash(hash(key, hasher), key, eq);
    if (idx == kInvalidEntryIdx) {
      return nullptr;
    }

    const Entry *ret = &entries_[idx];
    promise(ret != nullptr);
    return ret;
  }

  // Remove the stored entry by the key
  // Return false if not exist.
  bool Remove(const K &key, const H &hasher = H(), const E &eq = E()) {
    HashResult pri = hash(key, hasher);
    if (remove_from_bucket(pri, pri & bucket_mask_, key, eq)) return true;

    HashResult sec = hash_secondary(pri);
    return remove_from_bucket(pri, sec & bucket_mask_, key, eq);
  }

  void Clear() {
    buckets_.clear();
    entries_.clear();

    // std::stack doesn't have a clear() method. Strange.
    while (!free_entry_indices_.empty()) free_entry_indices_.pop();

    num_entries_ = 0;
    bucket_mask_ = kInitNumBucket - 1;
    buckets_.resize(kInitNumBucket);
    entries_.resize(kInitNumEntries);

    for (auto i : irange(int(kInitNumEntries - 1), -1, -1))
      free_entry_indices_.push(i);
  }

  // Return the number of stored entries
  size_t Count() const { return num_entries_; }

  int socket() const { return allocator_.socket(); }

 protected:
  // Tunable macros
  static const int kInitNumBucket = 8;
  static const int kInitNumEntries = 32;
  static const int kEntriesPerBucket = 4;  // 4-way set associative

  // 4^kMaxCuckooPath buckets will be considered to make a empty slot,
  // before giving up and expand the table.
  // Higher number will yield better occupancy, but the worst case performance
  // of insertion will grow exponentially, so be careful.
  static const int kMaxCuckooPath = 3;

  /* non-tunable macros */
  static const EntryIndex kInvalidEntryIdx =
      std::numeric_limits<EntryIndex>::max();

  struct Bucket {
    std::array<HashResult, kEntriesPerBucket> hash_values;
    std::array<EntryIndex, kEntriesPerBucket> entry_indices;

    Bucket() : hash_values(), entry_indices() {}
  };

  template <typename... Args>
  Entry *do_emplace(const K &key, const H &hasher, const E &eq,
                    Args &&... args) {
    Entry *entry;
    HashResult primary = hash(key, hasher);
    HashResult secondary = hash_secondary(primary);

    int trials = 0;

    while ((entry = emplace_entry(primary, secondary, key, hasher,
                                  std::forward<Args>(args)...)) == nullptr) {
      if (++trials >= 3) {
        LOG_FIRST_N(WARNING, 1)
            << "CuckooMap: Excessive hash colision detected:\n";
        return nullptr;
      }

      // expand the table as the last resort
      expand_buckets<std::conditional_t<std::is_move_constructible<V>::value,
                                        V &&, const V &>>(hasher, eq);
    }
    return entry;
  }

  // Push an unused entry index back to the stack
  void push_free_entry_index(EntryIndex idx) { free_entry_indices_.push(idx); }

  // Pop a free entry index from stack and return the index
  EntryIndex pop_free_entry_index() {
    if (free_entry_indices_.empty()) expand_entries();

    EntryIndex idx = free_entry_indices_.top();
    free_entry_indices_.pop();
    return idx;
  }

  template <typename... Args>
  Entry *emplace_in_bucket(Bucket &bucket, int slot_idx, const K &key,
                           const H &hasher, Args &&... args) {
    HashResult &hash_val = bucket.hash_values[slot_idx];
    EntryIndex &entry_idx = bucket.entry_indices[slot_idx];
    Entry *entry;

    // why use likely here cause performance degradation ?
    if (hash_val == 0) {
      entry_idx = pop_free_entry_index();
      entry = &entries_[entry_idx];
      entry->first = key;
      hash_val = hash(key, hasher);
      num_entries_++;
    } else {
      entry = &entries_[entry_idx];
      entry->second.~V();
    }

    new (&entry->second) V(std::forward<Args>(args)...);

    return entry;
  }

  // Try to add (key, value) to the bucket indexed by bucket_idx
  // Return the pointer to the entry if success. Otherwise return nullptr.
  template <typename... Args>
  Entry *emplace_in_bucket(HashResult bucket_idx, const K &key, const H &hasher,
                           Args &&... args) {
    Bucket &bucket = buckets_[bucket_idx];

    int slot_idx = find_empty_slot(bucket);
    if (slot_idx == -1) return nullptr;

    return emplace_in_bucket(bucket, slot_idx, key, hasher,
                             std::forward<Args>(args)...);
  }

  // Remove key from the bucket indexed by bucket_idx
  // Return true if success.
  bool remove_from_bucket(HashResult primary, HashResult bucket_idx,
                          const K &key, const E &eq) {
    Bucket &bucket = buckets_[bucket_idx];

    int slot_idx = find_slot(bucket, primary, key, eq);
    if (slot_idx == -1) {
      return false;
    }

    bucket.hash_values[slot_idx] = 0;

    EntryIndex idx = bucket.entry_indices[slot_idx];
    entries_[idx].~Entry();
    push_free_entry_index(idx);

    num_entries_--;
    return true;
  }

  // Find key from the bucket indexed by bucket_idx
  // Return the index of the entry if success. Otherwise return nullptr.
  EntryIndex get_from_bucket(HashResult primary, HashResult bucket_idx,
                             const K &key, const E &eq) const {
    const Bucket &bucket = buckets_[bucket_idx];

    int slot_idx = find_slot(bucket, primary, key, eq);
    if (slot_idx == -1) {
      return kInvalidEntryIdx;
    }

    // this promise gains 5% performance improvement
    EntryIndex idx = bucket.entry_indices[slot_idx];
    promise(idx != kInvalidEntryIdx);
    return idx;
  }

  // Try to add the entry (key, value)
  // Return the pointer to the entry if success. Otherwise return nullptr.
  template <typename... Args>
  Entry *emplace_entry(HashResult primary, HashResult secondary, const K &key,
                       const H &hasher, Args &&... args) {
    HashResult primary_bucket_index, secondary_bucket_index;
    Entry *entry = nullptr;

    primary_bucket_index = primary & bucket_mask_;
    if ((entry = emplace_in_bucket(primary_bucket_index, key, hasher,
                                   std::forward<Args>(args)...)) != nullptr) {
      return entry;
    }

    secondary_bucket_index = secondary & bucket_mask_;
    if ((entry = emplace_in_bucket(secondary_bucket_index, key, hasher,
                                   std::forward<Args>(args)...)) != nullptr) {
      return entry;
    }

    int slot_idx = make_space(primary_bucket_index, 0, hasher);
    if (slot_idx >= 0) {
      return emplace_in_bucket(buckets_[primary_bucket_index], slot_idx, key,
                               hasher, std::forward<Args>(args)...);
    }

    slot_idx = make_space(secondary_bucket_index, 0, hasher);
    if (slot_idx >= 0) {
      return emplace_in_bucket(buckets_[secondary_bucket_index], slot_idx, key,
                               hasher, std::forward<Args>(args)...);
    }

    return nullptr;
  }

  // Return an empty slot index in the bucket
  int find_empty_slot(const Bucket &bucket) const {
    for (auto i : irange(kEntriesPerBucket))
      if (bucket.hash_values[i] == 0) return i;

    return -1;
  }

  // Return the slot index in the bucket that matches the primary hash_value
  // and the actual key. Return -1 if not found.
  int find_slot(const Bucket &bucket, HashResult primary, const K &key,
                const E &eq) const {
    for (auto i : irange(kEntriesPerBucket))
      if (bucket.hash_values[i] == primary) {
        EntryIndex idx = bucket.entry_indices[i];
        const Entry &entry = entries_[idx];

        if (likely(equal(entry.first, key, eq))) {
          return i;
        }
      }

    return -1;
  }

  // Recursively try making an empty slot in the bucket
  // Returns a slot index in [0, kEntriesPerBucket) for successful operation,
  // or -1 if failed.
  int make_space(HashResult index, int depth, const H &hasher) {
    if (depth >= kMaxCuckooPath) {
      return -1;
    }

    Bucket &bucket = buckets_[index];

    for (int i = 0; i < kEntriesPerBucket; i++) {
      EntryIndex idx = bucket.entry_indices[i];
      const K &key = entries_[idx].first;
      HashResult pri = hash(key, hasher);
      HashResult sec = hash_secondary(pri);

      HashResult alt_index;

      // this entry is in its primary bucket?
      if (pri == bucket.hash_values[i]) {
        alt_index = sec & bucket_mask_;
      } else if (sec == bucket.hash_values[i]) {
        alt_index = pri & bucket_mask_;
      } else {
        return -1;
      }

      int j = find_empty_slot(buckets_[alt_index]);
      if (j == -1) {
        j = make_space(alt_index, depth + 1, hasher);
      }
      if (j >= 0) {
        Bucket &alt_bucket = buckets_[alt_index];
        alt_bucket.hash_values[j] = bucket.hash_values[i];
        alt_bucket.entry_indices[j] = bucket.entry_indices[i];
        bucket.hash_values[i] = 0;
        return i;
      }
    }

    return -1;
  }

  // Get the entry given the primary hash value of the key.
  // Returns the pointer to the entry or nullptr if failed.
  EntryIndex find_with_hash(HashResult primary, const K &key,
                            const E &eq) const {
    EntryIndex ret = get_from_bucket(primary, primary & bucket_mask_, key, eq);
    if (ret != kInvalidEntryIdx) {
      return ret;
    }
    return get_from_bucket(primary, hash_secondary(primary) & bucket_mask_, key,
                           eq);
  }

  // Secondary hash value
  static HashResult hash_secondary(HashResult primary) {
    HashResult tag = primary >> 12;
    return primary ^ ((tag + 1) * 0x5bd1e995);
  }

  // Primary hash value. Should always be non-zero (= not empty)
  static HashResult hash(const K &key, const H &hasher) {
    return hasher(key) | (1u << 31);
  }

  static bool equal(const K &lhs, const K &rhs, const E &eq) {
    return eq(lhs, rhs);
  }

  // Resize the space of entries. Grow less aggressively than buckets.
  void expand_entries() {
    size_t old_size = entries_.size();
    size_t new_size = old_size + old_size / 2;

    entries_.resize(new_size);

    for (auto i : irange(int(new_size - 1), int(old_size - 1), -1))
      free_entry_indices_.push(i);
  }

  // Resize the space of buckets, and rehash existing entries
  template <typename VV>
  void expand_buckets(const H &hasher, const E &eq) {
    CuckooMap<K, V, H, E> bigger(allocator_.socket(), buckets_.size() * 2,
                                 entries_.size());

    for (auto &e : *this) {
      // While very unlikely, this do_emplace() may cause recursive expansion
      Entry *ret =
          bigger.do_emplace(e.first, hasher, eq, std::forward<VV>(e.second));
      if (!ret) {
        return;
      }
    }

    *this = std::move(bigger);
  }

  // # of buckets == mask + 1
  HashResult bucket_mask_;

  // # of entries
  size_t num_entries_;

  // polymorphic_allocator
  MemoryResource allocator_;

  // bucket and entry arrays grow independently
  std::vector<Bucket, pmr::polymorphic_allocator<Bucket>> buckets_;
  std::vector<Entry, pmr::polymorphic_allocator<Entry>> entries_;

  // Stack of free entries (I don't know why using hugepage here will cause
  // performance degradation.)
  std::stack<EntryIndex> free_entry_indices_;
};

}  // namespace xlb::utils
