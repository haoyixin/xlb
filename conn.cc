#include "conn.h"

#include <cstdint>

namespace xlb {

Conn::Flag Conn::get_flag(Tcp *hdr) {
  if (hdr->flags & Tcp::Flag::kRst)
    return FLAG_RST;
  else if (hdr->flags & Tcp::Flag::kSyn)
    return hdr->flags & Tcp::Flag::kAck ? FLAG_SYNACK : FLAG_SYN;
  else if (hdr->flags & Tcp::Flag::kFin)
    return FLAG_FIN;
  else if (hdr->flags & Tcp::Flag::kAck)
    return FLAG_ACK;
  else
    return FLAG_NONE;
}

}  // namespace xlb
