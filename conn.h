#pragma once

#include <cstdint>

#include "utils/endian.h"

#include "headers/tcp.h"

namespace xlb {

class Conn {
 private:
  using be32_t = utils::be32_t;
  using be16_t = utils::be16_t;
  using Tcp = headers::Tcp;

 public:
  enum State {
    STATE_NONE = 0,
    STATE_SYN_SENT,
    STATE_SYN_RECV,
    STATE_ESTABLISHED,
    STATE_FIN_WAIT,
    STATE_CLOSE_WAIT,
    STATE_LAST_ACK,
    STATE_TIME_WAIT,
    STATE_CLOSE,
    STATE_SYN_SENT2,
    STATE_MAX,
    STATE_IGNORE
  };

  enum Dir { DIR_ORIGINAL = 0, DIR_REPLY, DIR_MAX };

  enum Flag { FLAG_SYN = 0, FLAG_SYNACK, FLAG_FIN, FLAG_ACK, FLAG_RST, FLAG_NONE, FLAG_MAX };

  void SetState(Tcp *hdr, Dir dir);

 private:
  be32_t cip, vip, lip, rip;
  be16_t cport, vport, lport, rport;

  State state_;

  static Flag get_flag(Tcp *hdr);

  static constexpr uint8_t smm[DIR_MAX][FLAG_MAX][STATE_MAX] = {
      {/* ORIGINAL */
       /*syn*/ {STATE_SYN_SENT, STATE_SYN_SENT, STATE_IGNORE, STATE_IGNORE, STATE_IGNORE,
                STATE_IGNORE, STATE_IGNORE, STATE_SYN_SENT, STATE_SYN_SENT, STATE_SYN_SENT2},
       /*synack*/
       {STATE_MAX, STATE_MAX, STATE_IGNORE, STATE_IGNORE, STATE_IGNORE, STATE_IGNORE, STATE_IGNORE,
        STATE_IGNORE, STATE_IGNORE, STATE_SYN_RECV},
       /*fin*/
       {STATE_MAX, STATE_MAX, STATE_FIN_WAIT, STATE_FIN_WAIT, STATE_LAST_ACK, STATE_LAST_ACK,
        STATE_LAST_ACK, STATE_TIME_WAIT, STATE_CLOSE, STATE_MAX},
       /*ack*/
       {STATE_ESTABLISHED, STATE_MAX, STATE_ESTABLISHED, STATE_ESTABLISHED, STATE_CLOSE_WAIT,
        STATE_CLOSE_WAIT, STATE_TIME_WAIT, STATE_TIME_WAIT, STATE_CLOSE, STATE_MAX},
       /*rst*/
       {STATE_MAX, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE,
        STATE_CLOSE, STATE_CLOSE, STATE_CLOSE},
       /*STATE_NONE*/
       {STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX,
        STATE_MAX, STATE_MAX}},
      {/* REPLY */
       /*syn*/ {STATE_MAX, STATE_SYN_SENT2, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX,
                STATE_MAX, STATE_MAX, STATE_SYN_SENT2},
       /*synack*/
       {STATE_MAX, STATE_SYN_RECV, STATE_SYN_RECV, STATE_IGNORE, STATE_IGNORE, STATE_IGNORE,
        STATE_IGNORE, STATE_IGNORE, STATE_IGNORE, STATE_SYN_RECV},
       /*fin*/
       {STATE_MAX, STATE_MAX, STATE_FIN_WAIT, STATE_FIN_WAIT, STATE_LAST_ACK, STATE_LAST_ACK,
        STATE_LAST_ACK, STATE_TIME_WAIT, STATE_CLOSE, STATE_MAX},
       /*ack*/
       {STATE_MAX, STATE_IGNORE, STATE_SYN_RECV, STATE_ESTABLISHED, STATE_CLOSE_WAIT,
        STATE_CLOSE_WAIT, STATE_TIME_WAIT, STATE_TIME_WAIT, STATE_CLOSE, STATE_IGNORE},
       /*rst*/
       {STATE_MAX, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE, STATE_CLOSE,
        STATE_CLOSE, STATE_CLOSE, STATE_CLOSE},
       /*STATE_NONE*/
       {STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX, STATE_MAX,
        STATE_MAX, STATE_MAX}}};

  uint32_t timeouts[STATE_MAX] = {
      [STATE_NONE] = 2,         [STATE_SYN_SENT] = 30, [STATE_SYN_RECV] = 30,
      [STATE_ESTABLISHED] = 90, [STATE_FIN_WAIT] = 30, [STATE_CLOSE_WAIT] = 30,
      [STATE_LAST_ACK] = 30,    [STATE_TIME_WAIT] = 0, [STATE_CLOSE] = 0,
      [STATE_SYN_SENT2] = 0,
  };
};

}  // namespace xlb
