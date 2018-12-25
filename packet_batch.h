#ifndef XLB_PACKET_BATCH_H
#define XLB_PACKET_BATCH_H

#include "utils/copy.h"

namespace xlb {

class Packet;

class PacketBatch {
public:
  class iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Packet;
    using pointer = Packet *;
    using reference = Packet &;
    using iterator_category = std::forward_iterator_tag;

    iterator(PacketBatch &batch, size_t pos) : batch_(batch), pos_(pos) {}

    iterator &operator++() { // Pre-increment
      ++pos_;
      return *this;
    }

    const iterator operator++(int) { // Post-increment
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
    size_t pos_;
  };

  iterator begin() { return iterator(*this, 0); }
  iterator end() { return iterator(*this, cnt_); }

  uint64_t cnt() const { return cnt_; }
  void set_cnt(int cnt) { cnt_ = cnt; }
  void incr_cnt(int n = 1) { cnt_ += n; }

  Packet *const *pkts() const { return pkts_; }
  Packet **pkts() { return pkts_; }

  void clear() { cnt_ = 0; }

  // WARNING: this function has no bounds checks and so it's possible to
  // overrun the buffer by calling this. We are not adding bounds check because
  // we want maximum GOFAST.
  void add(Packet *pkt) { pkts_[cnt_++] = pkt; }
  void add(PacketBatch *batch) {
    xlb::utils::CopyInlined(pkts_ + cnt_, batch->pkts(),
                            batch->cnt() * sizeof(Packet *));
    cnt_ += batch->cnt();
  }

  bool empty() { return (cnt_ == 0); }

  bool full() { return (cnt_ == kMaxBurst); }

  void Copy(const PacketBatch *src) {
    cnt_ = src->cnt_;
    xlb::utils::CopyInlined(pkts_, src->pkts_, cnt_ * sizeof(Packet *));
  }

  static const size_t kMaxBurst = 32;

private:
  int cnt_;
  Packet *pkts_[kMaxBurst];
};

static_assert(std::is_pod<PacketBatch>::value, "PacketBatch is not a POD Type");

} // namespace xlb

#endif // XLB_PACKET_BATCH_H
