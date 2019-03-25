#pragma once

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <string>
#include <type_traits>

#include <rte_atomic.h>
#include <rte_config.h>
#include <rte_mbuf.h>

#include <utils/copy.h>
#include <utils/simd.h>

#include "xbuf_layout.h"

/* NOTE: NEVER use rte_pktmbuf_*() directly,
 *       unless you know what you are doing */
static_assert(XBUF_MBUF == sizeof(struct rte_mbuf),
              "DPDK compatibility check failed");
static_assert(XBUF_HEADROOM == RTE_PKTMBUF_HEADROOM,
              "DPDK compatibility check failed");

static_assert(XBUF_IMMUTABLE_OFF == 128,
              "Packet immbutable offset must be 128");
static_assert(XBUF_METADATA_OFF == 192, "Packet metadata offset must by 192");
static_assert(XBUF_SCRATCHPAD_OFF == 320,
              "Packet scratchpad offset must be 320");

#define check_offset(field)                                    \
  static_assert(                                               \
      offsetof(Packet, field##_) == offsetof(rte_mbuf, field), \
      "Incompatibility detected between class Packet and struct rte_mbuf")

namespace xlb {
// For the layout of xbuf, see xbuf_layout.h
class alignas(64) Packet {
 public:
  static const size_t kMaxBurst = 32;

  Packet() = delete;  // Packet must be allocated from PacketPool

  Packet *vaddr() const { return vaddr_; }
  void set_vaddr(Packet *addr) { vaddr_ = addr; }

  phys_addr_t paddr() { return paddr_; }
  void set_paddr(phys_addr_t addr) { paddr_ = addr; }

  uint32_t sid() const { return sid_; }
  void set_sid(uint32_t sid) { sid_ = sid; }

  uint32_t index() const { return index_; }
  void set_index(uint32_t index) { index_ = index; }

  template <typename T = char *>
  T reserve() {
    return reinterpret_cast<T>(reserve_);
  }

  template <typename T = void *>
  const T head_data(uint16_t offset = 0) const {
    return reinterpret_cast<T>(static_cast<char *>(buf_addr_) + data_off_ +
                               offset);
  }

  template <typename T = void *>
  T head_data(uint16_t offset = 0) {
    return const_cast<T>(
        static_cast<const Packet &>(*this).head_data<T>(offset));
  }

  template <typename T = char *>
  T data() {
    return reinterpret_cast<T>(data_);
  }

  template <typename T = char *>
  T metadata() const {
    return reinterpret_cast<T>(metadata_);
  }

  template <typename T = char *>
  T scratchpad() {
    return reinterpret_cast<T>(scratchpad_);
  }

  template <typename T = void *>
  T buffer() {
    return reinterpret_cast<T>(buf_addr_);
  }

  int nb_segs() const { return nb_segs_; }
  void set_nb_segs(int n) { nb_segs_ = n; }

  Packet *next() const { return next_; }
  void set_next(Packet *next) { next_ = next; }

  uint16_t data_off() { return data_off_; }
  void set_data_off(uint16_t offset) { data_off_ = offset; }

  uint16_t data_len() { return data_len_; }
  void set_data_len(uint16_t len) { data_len_ = len; }

  int head_len() const { return data_len_; }

  int total_len() const { return pkt_len_; }
  void set_total_len(uint32_t len) { pkt_len_ = len; }

  void set_l2_len(size_t len) { mbuf_.l2_len = len; }
  void set_l3_len(size_t len) { mbuf_.l3_len = len; }
  void set_l4_len(size_t len) { mbuf_.l4_len = len; }

  uint64_t ol_flags() { return ol_flags_; }
  void set_ol_flags(uint64_t bit) { ol_flags_ |= bit; }

  bool rx_ip_cksum_good() {
    return (ol_flags_ & PKT_RX_IP_CKSUM_MASK) == PKT_RX_IP_CKSUM_GOOD;
  }

  bool rx_l4_cksum_good() {
    return (ol_flags_ & PKT_RX_L4_CKSUM_MASK) == PKT_RX_L4_CKSUM_GOOD;
  }

  uint16_t headroom() const { return rte_pktmbuf_headroom(&mbuf_); }

  uint16_t tailroom() const { return rte_pktmbuf_tailroom(&mbuf_); }

  // single segment?
  int is_linear() const { return rte_pktmbuf_is_contiguous(&mbuf_); }

  // single segment and direct?
  int is_simple() const { return is_linear() && RTE_MBUF_DIRECT(&mbuf_); }

  void reset() { rte_pktmbuf_reset(&mbuf_); }

  void *prepend(uint16_t len) {
    if (unlikely(data_off_ < len)) return nullptr;

    data_off_ -= len;
    data_len_ += len;
    pkt_len_ += len;

    return head_data();
  }

  // remove bytes from the beginning
  void *adj(uint16_t len) {
    if (unlikely(data_len_ < len)) return nullptr;

    data_off_ += len;
    data_len_ -= len;
    pkt_len_ -= len;

    return head_data();
  }

  // add bytes to the end
  void *append(uint16_t len) { return rte_pktmbuf_append(&mbuf_, len); }

  // remove bytes from the end
  void trim(uint16_t to_remove) {
    int ret;

    ret = rte_pktmbuf_trim(&mbuf_, to_remove);
    DCHECK_EQ(ret, 0);
  }

  // Duplicate a new Packet object, allocated from the same PacketPool as src.
  // Returns nullptr if memory allocation failed
  static Packet *Copy(const Packet *src) {
    DCHECK(src->is_linear());

    Packet *dst = reinterpret_cast<Packet *>(rte_pktmbuf_alloc(src->pool_));
    if (!dst) {
      return nullptr;  // FAIL.
    }

    utils::CopyInlined(dst->append(src->total_len()), src->head_data(),
                       src->total_len(), true);

    return dst;
  }

  phys_addr_t dma_addr() { return buf_physaddr_ + data_off_; }

  std::string Dump() {
    std::ostringstream dump;
    Packet *pkt;
    uint32_t dump_len = total_len();
    uint32_t nb_segs;
    uint32_t len;

    dump << "refcnt chain: ";
    for (pkt = this; pkt; pkt = pkt->next_) {
      dump << pkt->refcnt_ << ' ';
    }
    dump << std::endl;

    dump << "pool chain: ";
    for (pkt = this; pkt; pkt = pkt->next_) {
      int i;
      dump << pkt->pool_ << "("
           << ") ";
      // TODO: pool chain ?
    }
    dump << std::endl;

    dump << "dump packet at " << this << ", phys=" << buf_physaddr_
         << ", buf_len=" << buf_len_ << std::endl;
    dump << "  pkt_len=" << pkt_len_ << ", ol_flags=" << std::hex
         << mbuf_.ol_flags << ", nb_segs=" << std::dec << nb_segs_
         << ", in_port=" << mbuf_.port << std::endl;

    nb_segs = nb_segs_;
    pkt = this;
    while (pkt && nb_segs != 0) {
      __rte_mbuf_sanity_check(&pkt->mbuf_, 0);

      dump << "  segment at " << pkt << ", data=" << pkt->head_data()
           << ", data_len=" << std::dec << unsigned{data_len_} << std::endl;

      len = total_len();
      if (len > data_len_) {
        len = data_len_;
      }

      if (len != 0) {
        dump << HexDump(head_data(), len);
      }

      dump_len -= len;
      pkt = pkt->next_;
      nb_segs--;
    }

    return dump.str();
  }

  // TODO: multi metadata

  // pkt may be nullptr
  static void Free(Packet *pkt) {
    rte_pktmbuf_free(reinterpret_cast<struct rte_mbuf *>(pkt));
  }

  // All pointers in pkts must not be nullptr.
  // cnt must be [0, PacketBatch::kMaxBurst]
  // for packets to be processed in the fast path, all packets must:
  // 1. share the same mempool
  // 2. single segment
  // 3. reference counter == 1
  // 4. the data buffer is embedded in the mbuf
  static inline void Free(Packet **pkts, size_t cnt) {
    DCHECK_LE(cnt, kMaxBurst);

    // rte_mempool_put_bulk() crashes when called with cnt == 0
    if (unlikely(cnt <= 0)) {
      return;
    }

    struct rte_mempool *_pool = pkts[0]->pool_;

    // broadcast
    __m128i offset = _mm_set1_epi64x(XBUF_HEADROOM_OFF);
    __m128i info_mask = _mm_set1_epi64x(0x0000ffffffff0000UL);
    __m128i info_simple = _mm_set1_epi64x(0x0000000100010000UL);
    __m128i pool = _mm_set1_epi64x((uintptr_t)_pool);

    size_t i;

    for (i = 0; i < (cnt & ~1); i += 2) {
      auto *mbuf0 = pkts[i];
      auto *mbuf1 = pkts[i + 1];

      __m128i buf_addrs_derived;
      __m128i buf_addrs_actual;
      __m128i info;
      __m128i pools;
      __m128i vcmp1, vcmp2, vcmp3;

      __m128i mbuf_ptrs = _mm_set_epi64x(reinterpret_cast<uintptr_t>(mbuf1),
                                         reinterpret_cast<uintptr_t>(mbuf0));

      buf_addrs_actual = gather_m128i(&mbuf0->buf_addr_, &mbuf1->buf_addr_);
      buf_addrs_derived = _mm_add_epi64(mbuf_ptrs, offset);

      /* refcnt and nb_segs must be 1 */
      info = gather_m128i(&mbuf0->rearm_data_, &mbuf1->rearm_data_);
      info = _mm_and_si128(info, info_mask);

      pools = gather_m128i(&mbuf0->pool_, &mbuf1->pool_);

      vcmp1 = _mm_cmpeq_epi64(buf_addrs_derived, buf_addrs_actual);
      vcmp2 = _mm_cmpeq_epi64(info, info_simple);
      vcmp3 = _mm_cmpeq_epi64(pool, pools);

      vcmp1 = _mm_and_si128(vcmp1, vcmp2);
      vcmp1 = _mm_and_si128(vcmp1, vcmp3);

      if (unlikely(_mm_movemask_epi8(vcmp1) != 0xffff)) goto slow_path;
    }

    if (i < cnt) {
      const Packet *pkt = pkts[i];

      if (unlikely(pkt->pool_ != _pool || pkt->next_ != nullptr ||
                   pkt->refcnt_ != 1 || pkt->buf_addr_ != pkt->headroom_)) {
        goto slow_path;
      }
    }

    // When a rte_mbuf is returned to a mempool, the following conditions
    // must hold:
    for (i = 0; i < cnt; i++) {
      Packet *pkt = pkts[i];
      DCHECK_EQ(pkt->mbuf_.refcnt, 1);
      DCHECK_EQ(pkt->mbuf_.nb_segs, 1);
      DCHECK_EQ(pkt->mbuf_.next, static_cast<struct rte_mbuf *>(nullptr));
    }

    rte_mempool_put_bulk(_pool, reinterpret_cast<void **>(pkts), cnt);
    return;

  slow_path:
    for (i = 0; i < cnt; i++) {
      Free(pkts[i]);
    }
  }

  // basically rte_hexdump() from eal_common_hexdump.c
  static std::string HexDump(const void *buffer, size_t len) {
    std::ostringstream dump;
    size_t i, ofs;
    const char *data = reinterpret_cast<const char *>(buffer);

    dump << "Dump data at [" << buffer << "], len=" << len << std::endl;
    ofs = 0;
    while (ofs < len) {
      dump << std::setfill('0') << std::setw(8) << std::hex << ofs << ":";
      for (i = 0; ((ofs + i) < len) && (i < 16); i++) {
        dump << " " << std::setfill('0') << std::setw(2) << std::hex
             << (data[ofs + i] & 0xFF);
      }
      for (; i <= 16; i++) {
        dump << " | ";
      }
      for (i = 0; (ofs < len) && (i < 16); i++, ofs++) {
        char c = data[ofs];
        if ((c < ' ') || (c > '~')) {
          c = '.';
        }
        dump << c;
      }
      dump << std::endl;
    }
    return dump.str();
  }

  void CheckSanity() {
    static_assert(offsetof(Packet, mbuf_) == 0, "mbuf_ must be at offset 0");
    check_offset(buf_addr);
    check_offset(rearm_data);
    check_offset(data_off);
    check_offset(refcnt);
    check_offset(nb_segs);
    check_offset(rx_descriptor_fields1);
    check_offset(pkt_len);
    check_offset(data_len);
    check_offset(buf_len);
    check_offset(pool);
    check_offset(next);
  }

 private:
  union {
    struct {
      // offset 0: Virtual address of segment buffer.
      void *buf_addr_;
      // offset 8: Physical address of segment buffer.
      alignas(8) phys_addr_t buf_physaddr_;

      union {
        __m128i rearm_data_;

        struct {
          // offset 16:
          alignas(8) uint16_t data_off_;

          // offset 18:
          uint16_t refcnt_;

          // offset 20:
          uint16_t nb_segs_;  // Number of segments

          // offset 22:
          uint16_t _dummy0_;  // rte_mbuf.port
          // offset 24:
          uint64_t ol_flags_;  // rte_mbuf.ol_flags
        };
      };

      union {
        __m128i rx_descriptor_fields1_;

        struct {
          // offset 32:
          uint32_t _dummy2_;  // rte_mbuf.packet_type_;

          // offset 36:
          uint32_t pkt_len_;  // Total pkt length: sum of all segments

          // offset 40:
          uint16_t data_len_;  // Amount of data in this segment

          // offset 42:
          uint16_t _dummy3_;  // rte_mbuf.vlan_tci

          // offset 44:
          uint32_t _dummy4_lo;  // rte_mbuf.fdir.lo and rte_mbuf.rss
        };
      };

      // offset 48:
      uint32_t _dummy4_hi;  // rte_mbuf.fdir.hi

      // offset 52:
      uint16_t _dummy5_;  // rte_mbuf.vlan_tci_outer

      // offset 54:
      const uint16_t buf_len_;

      // offset 56:
      uint64_t _dummy6_;  // rte_mbuf.timestamp

      // 2nd cacheline - fields only used in slow path or on TX --------------
      // offset 64:
      uint64_t _dummy7_;  // rte_mbuf.userdata

      // offset 72:
      struct rte_mempool *pool_;  // Pool from which mbuf was allocated.

      // offset 80:
      Packet *next_;  // Next segment. nullptr if not scattered.

      // offset 88:
      uint64_t _dummy8;   // rte_mbuf.tx_offload
      uint16_t _dummy9;   // rte_mbuf.priv_size
      uint16_t _dummy10;  // rte_mbuf.timesync
      uint32_t _dummy11;  // rte_mbuf.seqn

      // offset 104:
    };

    struct rte_mbuf mbuf_;
  };

  union {
    char reserve_[XBUF_RESERVE];

    struct {
      union {
        char immutable_[XBUF_IMMUTABLE];

        const struct {
          // must be the first field
          Packet *vaddr_;

          phys_addr_t paddr_;

          // socket ID
          uint32_t sid_;

          // packet index within the pool
          uint32_t index_;
        };
      };

      // Dynamic metadata.
      char metadata_[XBUF_METADATA];

      // Used for module/port-specific data
      char scratchpad_[XBUF_SCRATCHPAD];
    };
  };

  char headroom_[XBUF_HEADROOM];
  char data_[XBUF_DATA];

  friend class PacketPool;
};

static_assert(std::is_standard_layout<Packet>::value, "Incorrect class Packet");
static_assert(sizeof(Packet) == XBUF_SIZE, "Incorrect class Packet");

}  // namespace xlb
