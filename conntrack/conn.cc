#include "conntrack/conn.h"
#include "conntrack/table.h"

namespace xlb::conntrack {

namespace {

#define sNO TCP_CONNTRACK_NONE
#define sSS TCP_CONNTRACK_SYN_SENT
#define sSR TCP_CONNTRACK_SYN_RECV
#define sES TCP_CONNTRACK_ESTABLISHED
#define sFW TCP_CONNTRACK_FIN_WAIT
#define sCW TCP_CONNTRACK_CLOSE_WAIT
#define sLA TCP_CONNTRACK_LAST_ACK
#define sTW TCP_CONNTRACK_TIME_WAIT
#define sCL TCP_CONNTRACK_CLOSE
#define sS2 TCP_CONNTRACK_SYN_SENT2
#define sIV TCP_CONNTRACK_MAX
#define sIG TCP_CONNTRACK_IGNORE

/*
 * The TCP state transition table needs a few words...
 *
 * We are the man in the middle. All the packets go through us
 * but might get lost in transit to the destination.
 * It is assumed that the destinations can't receive segments
 * we haven't seen.
 *
 * The checked segment is in window, but our windows are *not*
 * equivalent with the ones of the sender/receiver. We always
 * try to guess the state of the current sender.
 *
 * The meaning of the states are:
 *
 * NONE:	initial state
 * SYN_SENT:	SYN-only packet seen
 * SYN_SENT2:	SYN-only packet seen from reply dir, simultaneous open
 * SYN_RECV:	SYN-ACK packet seen
 * ESTABLISHED:	ACK packet seen
 * FIN_WAIT:	FIN packet seen
 * CLOSE_WAIT:	ACK seen (after FIN)
 * LAST_ACK:	FIN seen (after FIN)
 * TIME_WAIT:	last ACK seen
 * CLOSE:	closed connection (RST)
 *
 * Packets marked as IGNORED (sIG):
 *	if they may be either invalid or valid
 *	and the receiver may send back a connection
 *	closing RST or a SYN/ACK.
 *
 * Packets marked as INVALID (sIV):
 *	if we regard them as truly invalid packets
 */
constexpr tcp_conntrack
    tcp_conntracks[IP_CT_DIR_MAX][TCP_MAX_SET][TCP_CONNTRACK_MAX] = {
        {/* ORIGINAL */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*syn*/ {sSS, sSS, sIG, sIG, sIG, sIG, sIG, sSS, sSS, sS2},
         /*
          *	sNO -> sSS	Initialize a new connection
          *	sSS -> sSS	Retransmitted SYN
          *	sS2 -> sS2	Late retransmitted SYN
          *	sSR -> sIG
          *	sES -> sIG	Error: SYNs in window outside the SYN_SENT state
          *			are errors. Receiver will reply with RST
          *			and close the connection.
          *			Or we are not in sync and hold a dead
          *connection. sFW -> sIG sCW -> sIG sLA -> sIG sTW -> sSS Reopened
          *connection (RFC 1122). sCL -> sSS
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*synack*/ {sIV, sIV, sSR, sIV, sIV, sIV, sIV, sIV, sIV, sSR},
         /*
          *	sNO -> sIV	Too late and no reason to do anything
          *	sSS -> sIV	Client can't send SYN and then SYN/ACK
          *	sS2 -> sSR	SYN/ACK sent to SYN2 in simultaneous open
          *	sSR -> sSR	Late retransmitted SYN/ACK in simultaneous open
          *	sES -> sIV	Invalid SYN/ACK packets sent by the client
          *	sFW -> sIV
          *	sCW -> sIV
          *	sLA -> sIV
          *	sTW -> sIV
          *	sCL -> sIV
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*fin*/ {sIV, sIV, sFW, sFW, sLA, sLA, sLA, sTW, sCL, sIV},
         /*
          *	sNO -> sIV	Too late and no reason to do anything...
          *	sSS -> sIV	Client migth not send FIN in this state:
          *			we enforce waiting for a SYN/ACK reply first.
          *	sS2 -> sIV
          *	sSR -> sFW	Close started.
          *	sES -> sFW
          *	sFW -> sLA	FIN seen in both directions, waiting for
          *			the last ACK.
          *			Migth be a retransmitted FIN as well...
          *	sCW -> sLA
          *	sLA -> sLA	Retransmitted FIN. Remain in the same state.
          *	sTW -> sTW
          *	sCL -> sCL
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*ack*/ {sES, sIV, sES, sES, sCW, sCW, sTW, sTW, sCL, sIV},
         /*
          *	sNO -> sES	Assumed.
          *	sSS -> sIV	ACK is invalid: we haven't seen a SYN/ACK yet.
          *	sS2 -> sIV
          *	sSR -> sES	Established state is reached.
          *	sES -> sES	:-)
          *	sFW -> sCW	Normal close request answered by ACK.
          *	sCW -> sCW
          *	sLA -> sTW	Last ACK detected (RFC5961 challenged)
          *	sTW -> sTW	Retransmitted last ACK. Remain in the same
          *state. sCL -> sCL
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*rst*/ {sIV, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL},
         /*none*/ {sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV}},
        {/* REPLY */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*syn*/ {sIV, sS2, sIV, sIV, sIV, sIV, sIV, sSS, sIV, sS2},
         /*
          *	sNO -> sIV	Never reached.
          *	sSS -> sS2	Simultaneous open
          *	sS2 -> sS2	Retransmitted simultaneous SYN
          *	sSR -> sIV	Invalid SYN packets sent by the server
          *	sES -> sIV
          *	sFW -> sIV
          *	sCW -> sIV
          *	sLA -> sIV
          *	sTW -> sSS	Reopened connection, but server may have
          *switched role sCL -> sIV
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*synack*/ {sIV, sSR, sIG, sIG, sIG, sIG, sIG, sIG, sIG, sSR},
         /*
          *	sSS -> sSR	Standard open.
          *	sS2 -> sSR	Simultaneous open
          *	sSR -> sIG	Retransmitted SYN/ACK, ignore it.
          *	sES -> sIG	Late retransmitted SYN/ACK?
          *	sFW -> sIG	Might be SYN/ACK answering ignored SYN
          *	sCW -> sIG
          *	sLA -> sIG
          *	sTW -> sIG
          *	sCL -> sIG
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*fin*/ {sIV, sIV, sFW, sFW, sLA, sLA, sLA, sTW, sCL, sIV},
         /*
          *	sSS -> sIV	Server might not send FIN in this state.
          *	sS2 -> sIV
          *	sSR -> sFW	Close started.
          *	sES -> sFW
          *	sFW -> sLA	FIN seen in both directions.
          *	sCW -> sLA
          *	sLA -> sLA	Retransmitted FIN.
          *	sTW -> sTW
          *	sCL -> sCL
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*ack*/ {sIV, sIG, sSR, sES, sCW, sCW, sTW, sTW, sCL, sIG},
         /*
          *	sSS -> sIG	Might be a half-open connection.
          *	sS2 -> sIG
          *	sSR -> sSR	Might answer late resent SYN.
          *	sES -> sES	:-)
          *	sFW -> sCW	Normal close request answered by ACK.
          *	sCW -> sCW
          *	sLA -> sTW	Last ACK detected (RFC5961 challenged)
          *	sTW -> sTW	Retransmitted last ACK.
          *	sCL -> sCL
          */
         /* 	     sNO, sSS, sSR, sES, sFW, sCW, sLA, sTW, sCL, sS2	*/
         /*rst*/ {sIV, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL},
         /*none*/ {sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV}}};

#define HZ 1
#define SECS *HZ
#define MINS *60 SECS
#define HOURS *60 MINS
#define DAYS *24 HOURS

constexpr size_t tcp_timeouts[TCP_CONNTRACK_MAX] = {
    [TCP_CONNTRACK_NONE] = HZ,          [TCP_CONNTRACK_SYN_SENT] = 2 MINS,
    [TCP_CONNTRACK_SYN_RECV] = 60 SECS, [TCP_CONNTRACK_ESTABLISHED] = 5 DAYS,
    [TCP_CONNTRACK_FIN_WAIT] = 2 MINS,  [TCP_CONNTRACK_CLOSE_WAIT] = 60 SECS,
    [TCP_CONNTRACK_LAST_ACK] = 30 SECS, [TCP_CONNTRACK_TIME_WAIT] = 2 MINS,
    [TCP_CONNTRACK_CLOSE] = 10 SECS,    [TCP_CONNTRACK_SYN_SENT2] = 2 MINS,
};

#define TCPHDR_FIN 0x01ul
#define TCPHDR_SYN 0x02ul
#define TCPHDR_RST 0x04ul
#define TCPHDR_PSH 0x08ul
#define TCPHDR_ACK 0x10ul
#define TCPHDR_URG 0x20ul
#define TCPHDR_ECE 0x40ul
#define TCPHDR_CWR 0x80ul

class flags_index_t {
 public:
  constexpr flags_index_t() : map_() {
    for (uint8_t i = 0; i < 0xfful; ++i) map_[i] = calc_index(i);
  }

  constexpr tcp_bit_set operator[](uint8_t flags) const { return map_[flags]; }

 private:
  constexpr tcp_bit_set calc_index(uint8_t flags) {
    if (flags & TCPHDR_RST)
      return TCP_RST_SET;
    else if (flags & TCPHDR_SYN)
      return (flags & TCPHDR_ACK ? TCP_SYNACK_SET : TCP_SYN_SET);
    else if (flags & TCPHDR_FIN)
      return TCP_FIN_SET;
    else if (flags & TCPHDR_ACK)
      return TCP_ACK_SET;
    else
      return TCP_NONE_SET;
  }

  tcp_bit_set map_[0xfful];
};

constexpr auto tcp_conntrack_indexes = flags_index_t();

constexpr char *const tcp_conntrack_names[] = {
    "NONE",       "SYN_SENT", "SYN_RECV",  "ESTABLISHED", "FIN_WAIT",
    "CLOSE_WAIT", "LAST_ACK", "TIME_WAIT", "CLOSE",       "SYN_SENT2",
};

}  // namespace

tcp_bit_set get_conntrack_index(const Tcp *tcph) {
  return tcp_conntrack_indexes[tcph->flags];
}

tcp_conntrack Conn::UpdateState(tcp_bit_set index, ip_conntrack_dir dir) {
  auto new_state = tcp_conntracks[dir][index][state_];

  if (likely(new_state < TCP_CONNTRACK_MAX)) {
    if (state_ == TCP_CONNTRACK_SYN_RECV &&
        new_state == TCP_CONNTRACK_ESTABLISHED) {
      real_->IncrConns(1);
      virt_->IncrConns(1);
    }
    state_ = new_state;
    W_DVLOG(3) << "[Conn] state updated: " << *this;
    auto timeout = tcp_timeouts[state_] * tsc_hz;
    CTABLE.timer_.ScheduleInRange(this, timeout, timeout + 100 * tsc_ms);
  }

  return new_state;
}

uint32_t Conn::index() const { return (this - &CTABLE.conns_[0]); }

ip_conntrack_dir Conn::direction(Tuple4 &tuple) {
  // Need to make sure local ip is not used as vip, rsip or cip
  if (tuple.dst.ip == local_.ip)
    return IP_CT_DIR_REPLY;
  else
    return IP_CT_DIR_ORIGINAL;
}

void Conn::execute(TimerWheel<Conn> *timer) {
  W_DVLOG(2) << "[Conn] destructing: " << *this;

  real_->PutLocal(local_);

  CTABLE.idx_map_.Remove({client_, virt_->tuple()});
  CTABLE.idx_map_.Remove({local_, real_->tuple()});
  CTABLE.idx_pool_.push(index());

  real_.reset();
  virt_.reset();
}

std::ostream &operator<<(std::ostream &os, const Conn &conn) {
  os << "[client: " << conn.client_ << " virt: " << conn.virt_->tuple()
     << " local: " << conn.local_ << " real: " << conn.real_->tuple()
     << " state: " << tcp_conntrack_names[conn.state_]
     << " index: " << conn.index();
  return os;
}

}  // namespace xlb::conntrack
