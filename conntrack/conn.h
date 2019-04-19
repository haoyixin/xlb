#pragma once

#include "conntrack/common.h"
#include "conntrack/service.h"
#include "conntrack/tuple.h"

namespace xlb::conntrack {

// TODO: not to expose these

enum ip_conntrack_dir : uint8_t {
  IP_CT_DIR_ORIGINAL,
  IP_CT_DIR_REPLY,
  IP_CT_DIR_MAX
};

enum tcp_conntrack : uint8_t {
  TCP_CONNTRACK_NONE,
  TCP_CONNTRACK_SYN_SENT,
  TCP_CONNTRACK_SYN_RECV,
  TCP_CONNTRACK_ESTABLISHED,
  TCP_CONNTRACK_FIN_WAIT,
  TCP_CONNTRACK_CLOSE_WAIT,
  TCP_CONNTRACK_LAST_ACK,
  TCP_CONNTRACK_TIME_WAIT,
  TCP_CONNTRACK_CLOSE,
  TCP_CONNTRACK_LISTEN, /* obsolete */
#define TCP_CONNTRACK_SYN_SENT2 TCP_CONNTRACK_LISTEN
  TCP_CONNTRACK_MAX,
  TCP_CONNTRACK_IGNORE,
};

/* What TCP flags are set from RST/SYN/FIN/ACK. */
enum tcp_bit_set : uint8_t {
  TCP_SYN_SET,
  TCP_SYNACK_SET,
  TCP_FIN_SET,
  TCP_ACK_SET,
  TCP_RST_SET,
  TCP_NONE_SET,
  TCP_MAX_SET,
};

inline tcp_bit_set get_conntrack_index(const Tcp *tcph);

class alignas(64) Conn : public EventBase<Conn> {
 public:
  Conn() = default;
  ~Conn() = default;

  void execute(TimerWheel<Conn> *timer);

  // Here you need to ensure that the tuple belongs to this connection
  inline ip_conntrack_dir direction(Tuple4 &tuple);

  tcp_conntrack UpdateState(tcp_bit_set index, ip_conntrack_dir dir);

 private:
  tcp_conntrack state_;

  Tuple2 client_;
  Tuple2 local_;

  VirtSvc::Ptr virt_;
  RealSvc::Ptr real_;

  inline uint32_t index() const;

  friend inline std::ostream &operator<<(std::ostream &os, const Conn &conn);

  friend class ConnTable;
};

static_assert(sizeof(EventBase<Conn>) == 32);
static_assert(sizeof(Conn) == 64);

}  // namespace xlb::conntrack
