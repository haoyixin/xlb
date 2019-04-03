#pragma once

#include <array>

#include "utils/copy.h"

#include "packet.h"

namespace xlb {

class PacketBatch {
 public:
  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Packet;
    using pointer = Packet *;
    using reference = Packet &;
    using iterator_category = std::forward_iterator_tag;

    iterator(PacketBatch &batch, uint16_t pos) : batch_(batch), pos_(pos) {}

    iterator &operator++() {  // Pre-increment
      ++pos_;
      return *this;
    }

    const iterator operator++(int) {  // Post-increment
      iterator tmp(*this);
      pos_++;
      return tmp;
    }

    bool operator==(const iterator &rhs) const {
      return &batch_ == &rhs.batch_ && pos_ == rhs.pos_;
    }

    bool operator!=(const iterator &rhs) const {
      return &batch_ != &rhs.batch_ || pos_ != rhs.pos_;
    }

    reference operator*() { return *(batch_.pkts()[pos_]); }

    pointer operator->() { return batch_.pkts()[pos_]; }

   private:
    PacketBatch &batch_;
    uint16_t pos_;
  };

  iterator begin() { return iterator(*this, 0); }
  iterator end() { return iterator(*this, cnt_); }

  uint16_t cnt() const { return cnt_; }
  uint16_t free_cnt() const { return kMaxCnt - cnt_; }

  void SetCnt(uint16_t cnt) { cnt_ = cnt; }
  void IncrCnt(uint16_t n = 1) { cnt_ += n; }

  Packet *const *pkts() const { return pkts_.data(); }
  Packet **pkts() { return pkts_.data(); }

  Packet **Alloc(uint16_t cnt) {
    cnt_ = cnt;
    return pkts_.data();
  }

  void Clear() { cnt_ = 0; }

  // WARNING: this function has no bounds checks as we want maximum performance.
  void Push(Packet *pkt) { pkts_[cnt_++] = pkt; }
  void Push(PacketBatch *batch) {
    xlb::utils::CopyInlined(pkts_.data() + cnt_, batch->pkts(),
                            batch->cnt() * sizeof(Packet *));
    cnt_ += batch->cnt();
  }
  void Push(Packet **pkts, uint16_t cnt) {
    xlb::utils::CopyInlined(pkts_.data() + cnt_, pkts, cnt * sizeof(Packet *));

    cnt_ += cnt;
  }

  bool Empty() { return (cnt_ == 0); }

  bool Full() { return (cnt_ == kMaxCnt); }

  void Copy(const PacketBatch *src) {
    cnt_ = src->cnt_;
    xlb::utils::CopyInlined(pkts_.data(), src->pkts_.data(),
                            cnt_ * sizeof(Packet *));
  }

  void Free() {
    if (!Empty()) Packet::Free(pkts(), cnt_);
//    Clear();
  }

  static const uint16_t kMaxCnt = Packet::kMaxBurst * 2;

 private:
  std::array<Packet *, kMaxCnt> pkts_;
  uint16_t cnt_;
};

static_assert(std::is_pod<PacketBatch>::value, "PacketBatch is not a POD Type");

}  // namespace xlb
