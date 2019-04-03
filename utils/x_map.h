#pragma once

#include <cstdint>
#include <cstring>

#include <array>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include <rte_prefetch.h>

#include "utils/allocator.h"
#include "utils/bits.h"
#include "utils/boost.h"
#include "utils/common.h"

namespace pmr = std::experimental::pmr;

namespace xlb::utils {

// template <typename K>
// struct XHash {
//  uint32_t operator()(const K &key) { return XXH32(&key, sizeof(key), 0); }
//};

template <typename K, typename V, typename H = std::hash<K>,
          typename E = std::equal_to<K>>
class alignas(64) XMap {
 public:
  struct alignas(16) [[gnu::packed]] Entry {
    alignas(16) K key;
    V value;
  };

  static_assert(sizeof(Entry) == 16);

  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = Entry *;
    using reference = Entry &;
    using iterator_category = std::forward_iterator_tag;

    iterator(XMap &map, uint32_t bucket_idx, uint8_t entry_idx)
        : map_(map), bucket_idx_(bucket_idx), entry_idx_(entry_idx) {
      while (empty()) next();
    }

    iterator &operator++() {
      do {
        next();
      } while (empty());

      return *this;
    }

    const iterator operator++(int) {
      iterator tmp(*this);

      do {
        next();
      } while (empty());

      return tmp;
    }

    bool operator==(const iterator &rhs) const {
      return &map_ == &rhs.map_ && bucket_idx_ == rhs.bucket_idx_ &&
             entry_idx_ == rhs.entry_idx_;
    }

    bool operator!=(const iterator &rhs) const {
      return &map_ != &rhs.map_ || bucket_idx_ != rhs.bucket_idx_ ||
             entry_idx_ != rhs.entry_idx_;
    }

    reference operator*() { return map_.buckets_[bucket_idx_][entry_idx_]; }

    pointer operator->() { return &map_.buckets_[bucket_idx_][entry_idx_]; }

   protected:
    void next() {
      ++entry_idx_;
      if (entry_idx_ == kEntriesPerBucket) {
        entry_idx_ = 0;
        ++bucket_idx_;
      }
    }

    bool empty() {
      return bucket_idx_ < map_.buckets_.size() &&
             !map_.buckets_[bucket_idx_].Busy(entry_idx_);
    }

   private:
    XMap &map_;
    uint32_t bucket_idx_;
    uint8_t entry_idx_;
  };

  explicit XMap(uint32_t num_buckets)
      : bucket_mask_(align_ceil_pow2(num_buckets) - 1),
        num_entries_(0),
        buckets_(bucket_mask_ + 1, &DefaultAllocator()) {}

  XMap(XMap &) = delete;
  XMap &operator=(XMap &) = delete;

  XMap(XMap &&) = default;
  XMap &operator=(XMap &&) = default;

  iterator begin() { return iterator(*this, 0, 0); }
  iterator end() { return iterator(*this, buckets_.size(), 0); }

  const Entry *Find(const K &key) const {
    uint32_t prim_hash = hash(key);
    const Bucket *prim_bkt = &buckets_[prim_hash & bucket_mask_];
    rte_prefetch0(prim_bkt);

    uint32_t sec_hash = hash_secondary(prim_hash);

    return find_in_bucket(*prim_bkt, buckets_[sec_hash & bucket_mask_], key,
                          sec_hash);
  }

  Entry *Find(const K &key) {
    return const_cast<Entry *>(
        static_cast<const typename std::remove_reference<decltype(*this)>::type
                        &>(*this)
            .Find(key));
  }

  template <typename... Args>
  Entry *EmplaceUnsafe(const K &key, Args &&... args) {
    uint32_t prim_hash = hash(key);
    uint32_t sec_hash = hash_secondary(prim_hash);

    return emplace_in_bucket(buckets_[prim_hash & bucket_mask_],
                             buckets_[sec_hash & bucket_mask_], key, sec_hash,
                             std::forward<Args>(args)...);
  }

  template <typename... Args>
  Entry *Emplace(const K &key, Args &&... args) {
    uint32_t prim_hash = hash(key);
    Bucket *prim_bkt = &buckets_[prim_hash & bucket_mask_];
    rte_prefetch0(prim_bkt);

    uint32_t sec_hash = hash_secondary(prim_hash);
    Bucket *sec_bkt = &buckets_[sec_hash & bucket_mask_];

    Entry *entry;
    if ((entry = const_cast<Entry *>(
             find_in_bucket(*prim_bkt, *sec_bkt, key, sec_hash))) != nullptr) {
//      entry->value.~V();
//      new (&entry->value) V(std::forward<Args>(args)...);

      return entry;
    }

    return emplace_in_bucket(*prim_bkt, *sec_bkt, key, sec_hash,
                             std::forward<Args>(args)...);
  }

  Entry *Insert(const K &key, const V &value) { return Emplace(key, value); }

  Entry *Insert(const K &key, V &&value) {
    return Emplace(key, std::move(value));
  }

  Entry *InsertUnsafe(const K &key, const V &value) {
    return EmplaceUnsafe(key, value);
  }

  Entry *InsertUnsafe(const K &key, V &&value) {
    return EmplaceUnsafe(key, std::move(value));
  }

  bool Remove(const K &key) {
    uint32_t prim_hash = hash(key);
    Bucket *prim_bkt = &buckets_[prim_hash & bucket_mask_];
    rte_prefetch0(prim_bkt);

    uint32_t sec_hash = hash_secondary(prim_hash);

    Entry *entry;
    if ((entry = remove_in_bucket(*prim_bkt, buckets_[sec_hash & bucket_mask_],
                                  key, sec_hash)) != nullptr) {
      entry->~Entry();
      //      entry->value.~V();
      return true;
    }

    return false;
  }

  uint32_t Size() const { return num_entries_; }

 protected:
  static constexpr uint8_t kEntriesPerBucket = 3;
  static constexpr uint8_t kMaxCuckooPath = 3;

  static uint32_t hash(const K &key) { return H()(key); }

  static uint32_t hash_secondary(uint32_t primary) {
    return primary ^ (((primary >> 12) + 1) * 0x5bd1e995);
  }

  static bool equal(const K &lhs, const K &rhs) { return E()(lhs, rhs); }

  class alignas(64) [[gnu::packed]] Bucket {
   public:
    int8_t EmptyIndex() const {
      for (auto i : irange(kEntriesPerBucket))
        if (!busy_mask_[i]) return i;

      return -1;
    }

    int8_t FindIndex(const K &key) const {
      for (auto i : irange(kEntriesPerBucket)) {
        if (busy_mask_[i] && equal(key, entries_[i].key)) return i;
      }

      return -1;
    }

    bool PossibleInSecondary(uint32_t sec_hash) const {
      return bloom_filter_[sec_hash & 0x3f] &&
             bloom_filter_[sec_hash >> 6 & 0x3f];
    }

    template <typename T>
    Entry &operator[](T index) {
      return entries_[index];
    }

    template <typename T>
    const Entry &operator[](T index) const {
      return entries_[index];
    }

    bool Full() const { return busy_mask_.all(); }
    //    bool Empty() const { return busy_mask_.none(); }

    bool Busy(uint8_t index) const { return busy_mask_[index]; }
    bool Secondary(uint8_t index) const { return secondary_mask_[index]; }

    void MoveIntoSecondary(uint32_t sec_hash, uint8_t prim_idx,
                           Bucket * sec_bkt, uint8_t sec_idx) {
      sec_bkt->OccupySecondary(sec_hash, sec_idx, *this);
      sec_bkt->entries_[sec_idx] = std::move(entries_[prim_idx]);
      RemoveInPrimary(prim_idx);
    }

    void MoveIntoPrimary(uint8_t sec_idx, Bucket * prim_bkt, uint8_t prim_idx) {
      prim_bkt->OccupyPrimary(prim_idx);
      prim_bkt->entries_[prim_idx] = std::move(entries_[sec_idx]);
      RemoveInSecondary(sec_idx, *prim_bkt);
    }

    void RemoveInPrimary(uint8_t prim_idx) {
      busy_mask_.reset(prim_idx);
      secondary_mask_.reset(prim_idx);
    }

    void RemoveInSecondary(uint8_t sec_idx, Bucket & prim_bkt) {
      if (--prim_bkt.move_counter_ == 0) prim_bkt.bloom_filter_.reset();

      RemoveInPrimary(sec_idx);
    }

    void OccupyPrimary(uint8_t prim_idx) { busy_mask_.set(prim_idx); }

    void OccupySecondary(uint32_t sec_hash, uint8_t sec_idx,
                         Bucket & prim_bkt) {
      prim_bkt.bloom_filter_.set(sec_hash & 0x3f);
      prim_bkt.bloom_filter_.set(sec_hash >> 6 & 0x3f);
      ++prim_bkt.move_counter_;
      secondary_mask_.set(sec_idx);

      OccupyPrimary(sec_idx);
    }

   private:
    alignas(64) bitset<64> bloom_filter_;
    alignas(4) bitset<kEntriesPerBucket> busy_mask_;
    alignas(2) bitset<kEntriesPerBucket> secondary_mask_;
    alignas(4) uint32_t move_counter_ = 0u;
    alignas(16) std::array<Entry, kEntriesPerBucket> entries_;
  };

  static_assert(sizeof(Bucket) == 64);

  Entry *remove_in_bucket(Bucket &prim_bkt, Bucket &sec_bkt, const K &key,
                          uint32_t sec_hash) {
    int8_t idx;

    if ((idx = prim_bkt.FindIndex(key)) != -1) {
      prim_bkt.RemoveInPrimary(idx);
      --num_entries_;

      return &prim_bkt[idx];
    }

    if (likely(!prim_bkt.PossibleInSecondary(sec_hash))) return nullptr;

    rte_prefetch0(&sec_bkt);

    if ((idx = sec_bkt.FindIndex(key)) != -1) {
      sec_bkt.RemoveInSecondary(idx, prim_bkt);
      --num_entries_;

      return &prim_bkt[idx];
    }

    return nullptr;
  }

  const Entry *find_in_bucket(const Bucket &prim_bkt, const Bucket &sec_bkt,
                              const K &key, uint32_t sec_hash) const {
    int8_t idx;

    if ((idx = prim_bkt.FindIndex(key)) != -1) return &prim_bkt[idx];

    if (likely(!prim_bkt.PossibleInSecondary(sec_hash))) return nullptr;

    rte_prefetch0(&sec_bkt);

    if ((idx = sec_bkt.FindIndex(key)) != -1) return &sec_bkt[idx];

    return nullptr;
  }

  template <typename... Args>
  Entry *emplace_in_bucket(Bucket &prim_bkt, Bucket &sec_bkt, const K &key,
                           uint32_t sec_hash, Args &&... args) {
    Entry *entry = occupy_entry(prim_bkt, sec_bkt, sec_hash);

    if (likely(entry != nullptr)) {
      entry->key = key;
      new (&entry->value) V(std::forward<Args>(args)...);

      ++num_entries_;

      return entry;
    }

    return nullptr;
  }

  Entry *occupy_entry(Bucket &prim_bkt, Bucket &sec_bkt, uint32_t sec_hash) {
    int8_t idx;

    if (likely(!prim_bkt.Full())) {
      idx = prim_bkt.EmptyIndex();
      prim_bkt.OccupyPrimary(idx);

      return &prim_bkt[idx];
    }

    if (likely(!sec_bkt.Full())) {
      idx = sec_bkt.EmptyIndex();
      sec_bkt.OccupySecondary(sec_hash, idx, prim_bkt);

      return &sec_bkt[idx];
    }

    if (likely((idx = empty_out_entry(&prim_bkt, 0)) != -1)) {
      prim_bkt.OccupyPrimary(idx);
      return &prim_bkt[idx];
    }

    if (likely((idx = empty_out_entry(&sec_bkt, 0)) != -1)) {
      sec_bkt.OccupySecondary(sec_hash, idx, prim_bkt);
      return &sec_bkt[idx];
    }

    return nullptr;
  }

  int8_t empty_out_entry(Bucket *bkt, uint8_t depth) {
    if (unlikely(depth >= kMaxCuckooPath)) return -1;

    for (auto i : irange(kEntriesPerBucket)) {
      uint32_t prim_hash = hash((*bkt)[i].key);
      uint32_t sec_hash = hash_secondary(prim_hash);
      bool prim = !bkt->Secondary(i);

      Bucket *alt_bkt;

      if (likely(prim))
        alt_bkt = &buckets_[sec_hash & bucket_mask_];
      else
        alt_bkt = &buckets_[prim_hash & bucket_mask_];

      int8_t alt_idx;

      if (likely(!alt_bkt->Full()))
        alt_idx = alt_bkt->EmptyIndex();
      else if (unlikely((alt_idx = empty_out_entry(alt_bkt, depth + 1)) == -1))
        continue;

      if (likely(prim))
        bkt->MoveIntoSecondary(sec_hash, i, alt_bkt, alt_idx);
      else
        bkt->MoveIntoPrimary(i, alt_bkt, alt_idx);

      return i;
    }

    return -1;
  }

 private:
  uint32_t bucket_mask_;

  uint32_t num_entries_;

  utils::vector<Bucket> buckets_;
};

}  // namespace xlb::utils
