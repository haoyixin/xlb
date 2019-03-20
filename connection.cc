#include "connection.h"

#include <cstdint>

namespace xlb {

void Connection::UpdateState(Connection::Tcp *hdr, Connection::Dir dir) {
  State new_state =
      smt[dir][hdr->flags & Tcp::kAck
                   ? FLAG_ACK
                   : hdr->flags & Tcp::kSyn
                         ? hdr->flags & Tcp::kAck ? FLAG_SYNACK : FLAG_SYN
                         : hdr->flags & Tcp::kFin
                               ? FLAG_FIN
                               : hdr->flags & Tcp::kRst ? FLAG_RST : FLAG_NONE]
         [state_];

  if (new_state < STATE_MAX)
    state_ = new_state;
}

}  // namespace xlb
